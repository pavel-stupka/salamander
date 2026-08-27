// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// intake.cpp - text/binary classification, decoding and language
// identification. See intake.h.

#include "precomp.h"
#include "intake.h"
#include "langmap.h"

#include <algorithm>

#define CV_SNIFF_BYTES 8192

// ==========================================================================
// low-level file access (long paths, UTF-8 names)
// ==========================================================================

// Every char* crossing the plugin interface is UTF-8 (feature 004); the wide
// helpers add the \\?\ prefix so a path over MAX_PATH still opens.
static HANDLE CvOpenRead(const char* nameUtf8)
{
    wchar_t* w = SplU8ToWExtAlloc(nameUtf8);
    if (w == NULL)
        return INVALID_HANDLE_VALUE; // not convertible (WTF-8 surrogate name): decline
    HANDLE h = CreateFileW(w, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    SalamanderGeneral->Free(w);
    return h;
}

// ==========================================================================
// text / binary decision (contracts/claimed-types.md S5)
// ==========================================================================

static BOOL CvLooksUtf16NoBom(const BYTE* d, size_t n, BOOL* bigEndian)
{
    // UTF-16 text without a BOM shows NULs in a strict parity: Latin text is
    // 00 in every odd byte (LE) or every even byte (BE). The built-in viewer
    // does not detect this at all, which is why a UTF-16 .reg file looks like
    // garbage there; the source viewer is the first to handle it.
    if (n < 16)
        return FALSE;
    size_t even = 0, odd = 0, pairs = n / 2;
    for (size_t i = 0; i + 1 < n; i += 2)
    {
        if (d[i] == 0)
            even++;
        if (d[i + 1] == 0)
            odd++;
    }
    if (pairs == 0)
        return FALSE;
    if (odd * 4 >= pairs * 3 && even * 8 < pairs)
    {
        *bigEndian = FALSE;
        return TRUE;
    }
    if (even * 4 >= pairs * 3 && odd * 8 < pairs)
    {
        *bigEndian = TRUE;
        return TRUE;
    }
    return FALSE;
}

// The WHATWG "binary" control set: C0 controls minus TAB, LF, FF, CR, ESC.
static BOOL CvIsBinaryControl(BYTE b)
{
    if (b <= 0x08)
        return TRUE;
    if (b == 0x0B)
        return TRUE;
    if (b >= 0x0E && b <= 0x1A)
        return TRUE;
    if (b >= 0x1C && b <= 0x1F)
        return TRUE;
    return FALSE;
}

static BOOL CvSniffIsText(const BYTE* d, size_t n)
{
    if (n == 0)
        return TRUE; // an empty file is text
    if (n >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF)
        return TRUE;
    if (n >= 2 && ((d[0] == 0xFF && d[1] == 0xFE) || (d[0] == 0xFE && d[1] == 0xFF)))
        return TRUE;
    BOOL be = FALSE;
    if (CvLooksUtf16NoBom(d, n, &be))
        return TRUE;
    size_t ctrl = 0;
    for (size_t i = 0; i < n; i++)
    {
        if (d[i] == 0)
            return FALSE; // a NUL outside UTF-16 means binary
        if (CvIsBinaryControl(d[i]))
            ctrl++;
    }
    return ctrl * 200 <= n; // > 0.5 % control bytes -> binary
}

BOOL CvCanView(const char* nameUtf8)
{
    HANDLE h = CvOpenRead(nameUtf8);
    if (h == INVALID_HANDLE_VALUE)
        return FALSE; // unreadable, or a name we cannot represent: cascade
    LARGE_INTEGER sz;
    BOOL ok = FALSE;
    if (GetFileSizeEx(h, &sz))
    {
        __int64 limit = (__int64)g_viewerLimitMB * 1024 * 1024;
        if (sz.QuadPart <= limit)
        {
            BYTE head[CV_SNIFF_BYTES];
            DWORD rd = 0;
            if (ReadFile(h, head, CV_SNIFF_BYTES, &rd, NULL))
                ok = CvSniffIsText(head, rd);
        }
    }
    CloseHandle(h);
    return ok;
}

// ==========================================================================
// decoding
// ==========================================================================

static BOOL CvIsValidUtf8(const BYTE* d, size_t n)
{
    size_t i = 0;
    while (i < n)
    {
        BYTE c = d[i];
        size_t need;
        if (c < 0x80)
        {
            i++;
            continue;
        }
        else if ((c & 0xE0) == 0xC0)
            need = 1;
        else if ((c & 0xF0) == 0xE0)
            need = 2;
        else if ((c & 0xF8) == 0xF0)
            need = 3;
        else
            return FALSE;
        if (i + need >= n + 0 && i + need > n - 1)
            return FALSE;
        for (size_t k = 1; k <= need; k++)
            if ((d[i + k] & 0xC0) != 0x80)
                return FALSE;
        i += need + 1;
    }
    return TRUE;
}

static void CvAppendUtf8FromWide(const wchar_t* w, int wlen, std::string& out)
{
    if (wlen <= 0)
        return;
    int n = WideCharToMultiByte(CP_UTF8, 0, w, wlen, NULL, 0, NULL, NULL);
    if (n <= 0)
        return;
    size_t at = out.size();
    out.resize(at + n);
    WideCharToMultiByte(CP_UTF8, 0, w, wlen, &out[at], n, NULL, NULL);
}

// Decodes the raw bytes into UTF-8 with LF line ends, recording the original
// line-end style and the number of undecodable sequences.
static void CvDecode(const BYTE* d, size_t n, CvEncoding enc, UINT codePage, CvIntake& out)
{
    std::wstring wide;
    if (enc == cvEncUtf16LE || enc == cvEncUtf16BE)
    {
        size_t start = 0;
        if (n >= 2 && ((d[0] == 0xFF && d[1] == 0xFE) || (d[0] == 0xFE && d[1] == 0xFF)))
            start = 2;
        size_t units = (n - start) / 2;
        wide.resize(units);
        for (size_t i = 0; i < units; i++)
        {
            BYTE a = d[start + i * 2], b = d[start + i * 2 + 1];
            wide[i] = (enc == cvEncUtf16LE) ? (wchar_t)(a | (b << 8)) : (wchar_t)(b | (a << 8));
        }
    }
    else
    {
        size_t start = (enc == cvEncUtf8Bom) ? 3 : 0;
        UINT cp = (enc == cvEncAnsi) ? codePage : CP_UTF8;
        if (n > start)
        {
            int need = MultiByteToWideChar(cp, 0, (const char*)d + start, (int)(n - start), NULL, 0);
            if (need > 0)
            {
                wide.resize(need);
                MultiByteToWideChar(cp, 0, (const char*)d + start, (int)(n - start), &wide[0], need);
            }
        }
    }

    // Normalise line ends to LF (the page splits on LF) and remember the style.
    BOOL sawCRLF = FALSE, sawLF = FALSE, sawCR = FALSE;
    std::wstring norm;
    norm.reserve(wide.size());
    int lineLen = 0;
    out.LineCount = wide.empty() ? 0 : 1;
    for (size_t i = 0; i < wide.size(); i++)
    {
        wchar_t c = wide[i];
        if (c == L'\r')
        {
            if (i + 1 < wide.size() && wide[i + 1] == L'\n')
            {
                sawCRLF = TRUE;
                i++;
            }
            else
                sawCR = TRUE;
            norm += L'\n';
            out.LineCount++;
            out.LongestLine = (std::max)(out.LongestLine, lineLen);
            lineLen = 0;
        }
        else if (c == L'\n')
        {
            sawLF = TRUE;
            norm += L'\n';
            out.LineCount++;
            out.LongestLine = (std::max)(out.LongestLine, lineLen);
            lineLen = 0;
        }
        else
        {
            if (c == 0xFFFD)
                out.InvalidBytes++;
            norm += c;
            lineLen++;
        }
    }
    out.LongestLine = (std::max)(out.LongestLine, lineLen);
    out.TrailingNewline = !norm.empty() && norm[norm.size() - 1] == L'\n';
    if (out.TrailingNewline && out.LineCount > 0)
        out.LineCount--; // a trailing newline does not open a new line

    int styles = (sawCRLF ? 1 : 0) + (sawLF ? 1 : 0) + (sawCR ? 1 : 0);
    out.Eol = styles == 0 ? cvEolNone
              : styles > 1 ? cvEolMixed
              : sawCRLF    ? cvEolCRLF
              : sawLF      ? cvEolLF
                           : cvEolCR;

    out.Utf8.clear();
    CvAppendUtf8FromWide(norm.c_str(), (int)norm.size(), out.Utf8);
}

// ==========================================================================
// language identification (spec FR-005 / FR-006)
// ==========================================================================

static int CvLookup(const CvNameRule* table, int count, const char* key, int* rule)
{
    // The generated tables are sorted, so a binary search is safe and keeps
    // identification off the critical path even with ~750 extensions.
    int lo = 0, hi = count - 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        int c = strcmp(table[mid].Key, key);
        if (c == 0)
        {
            if (rule != NULL)
                *rule = table[mid].Rule;
            return table[mid].Lang;
        }
        if (c < 0)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

static std::string CvLowerAscii(const char* s, size_t n)
{
    std::string r(s, n);
    for (size_t i = 0; i < r.size(); i++)
        if (r[i] >= 'A' && r[i] <= 'Z')
            r[i] = (char)(r[i] - 'A' + 'a');
    return r;
}

static BOOL CvHeadContains(const char* head, size_t n, const char* needle)
{
    size_t nl = strlen(needle);
    if (nl == 0 || n < nl)
        return FALSE;
    for (size_t i = 0; i + nl <= n; i++)
        if (_strnicmp(head + i, needle, nl) == 0)
            return TRUE;
    return FALSE;
}

static BOOL CvLineStartsWith(const char* head, size_t n, const char* prefix)
{
    size_t pl = strlen(prefix);
    size_t i = 0;
    while (i < n)
    {
        size_t j = i;
        while (j < n && (head[j] == ' ' || head[j] == '\t'))
            j++;
        if (j + pl <= n && _strnicmp(head + j, prefix, pl) == 0)
            return TRUE;
        while (i < n && head[i] != '\n')
            i++;
        i++;
    }
    return FALSE;
}

// The probes behind the generated ambiguity table (tools/codeview/ambiguity.json).
// Each returns a language index, or -1 to keep the table's default.
static int CvRunAmbiguityProbe(const char* ruleId, const char* head, size_t n)
{
    if (head == NULL || n == 0)
        return -1;
    auto lang = [](const char* id) -> int
    {
        for (int i = 0; i < CvLanguageCount; i++)
            if (strcmp(CvLanguages[i].Id, id) == 0)
                return i;
        return -1;
    };
    if (strcmp(ruleId, "h-header") == 0)
    {
        if (CvHeadContains(head, n, "@interface") || CvHeadContains(head, n, "@implementation") ||
            CvHeadContains(head, n, "#import <Foundation"))
            return lang("objective-c");
        return -1; // default C++ (the table's fallback)
    }
    if (strcmp(ruleId, "m-source") == 0)
    {
        if (CvHeadContains(head, n, "@interface") || CvHeadContains(head, n, "@implementation") ||
            CvHeadContains(head, n, "#import"))
            return -1; // Objective-C, the default
        if (CvLineStartsWith(head, n, "function ") || CvHeadContains(head, n, "\n%%"))
            return lang("x-matlab");
        return -1;
    }
    if (strcmp(ruleId, "pl-script") == 0)
    {
        if (CvLineStartsWith(head, n, ":-") || CvHeadContains(head, n, ":- module("))
            return lang("prolog");
        return -1;
    }
    if (strcmp(ruleId, "v-source") == 0)
    {
        if (CvHeadContains(head, n, "Require Import") || CvHeadContains(head, n, "Theorem "))
            return lang("coq");
        if (CvLineStartsWith(head, n, "module ") || CvHeadContains(head, n, "endmodule"))
            return -1; // Verilog, the default
        if (CvLineStartsWith(head, n, "fn ") || CvHeadContains(head, n, "pub struct"))
            return lang("v");
        return -1;
    }
    if (strcmp(ruleId, "ts-source") == 0)
    {
        if (CvHeadContains(head, n, "<!DOCTYPE TS") || CvHeadContains(head, n, "<TS "))
            return lang("xml");
        return -1;
    }
    if (strcmp(ruleId, "inc-include") == 0)
    {
        if (CvHeadContains(head, n, "<?php"))
            return lang("php");
        if (CvLineStartsWith(head, n, ".include") || CvLineStartsWith(head, n, ".globl") ||
            CvLineStartsWith(head, n, "section ."))
            return lang("asm");
        if (CvLineStartsWith(head, n, "#include") || CvLineStartsWith(head, n, "#define") ||
            CvLineStartsWith(head, n, "#ifndef"))
            return lang("cpp");
        return -1;
    }
    if (strcmp(ruleId, "sql-dialect") == 0)
    {
        if (CvHeadContains(head, n, "CREATE OR REPLACE PACKAGE") || CvHeadContains(head, n, "PL/SQL"))
            return lang("plsql");
        return -1;
    }
    if (strcmp(ruleId, "cls-file") == 0)
    {
        if (CvHeadContains(head, n, "\\documentclass") || CvHeadContains(head, n, "\\NeedsTeXFormat") ||
            CvHeadContains(head, n, "\\ProvidesClass"))
            return lang("latex");
        if (CvHeadContains(head, n, "with sharing class") || CvHeadContains(head, n, "global class"))
            return lang("apex");
        return -1;
    }
    if (strcmp(ruleId, "r-source") == 0 || strcmp(ruleId, "t-test") == 0)
        return -1; // the default is right unless a stronger signal appears
    if (strcmp(ruleId, "asm-source") == 0)
    {
        if (CvHeadContains(head, n, "riscv") || CvHeadContains(head, n, ".riscv"))
            return lang("riscv");
        return -1;
    }
    return -1;
}

static int CvShebangLanguage(const char* head, size_t n)
{
    if (n < 3 || head[0] != '#' || head[1] != '!')
        return -1;
    size_t end = 2;
    while (end < n && head[end] != '\n' && head[end] != '\r')
        end++;
    std::string line = CvLowerAscii(head + 2, end - 2);
    // last path component of the interpreter, ignoring "env"
    size_t pos = 0;
    std::string interp;
    while (pos < line.size())
    {
        while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
            pos++;
        size_t start = pos;
        while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t')
            pos++;
        std::string word = line.substr(start, pos - start);
        size_t slash = word.find_last_of("/\\");
        if (slash != std::string::npos)
            word = word.substr(slash + 1);
        if (word.empty() || word == "env" || (!word.empty() && word[0] == '-'))
            continue;
        interp = word;
        break;
    }
    if (interp.empty())
        return -1;
    int lang = CvLookup(CvInterpreters, CvInterpreterCount, interp.c_str(), NULL);
    if (lang < 0 && interp.size() > 1)
    {
        // versioned interpreters: python3.12 -> python3 -> python
        std::string base = interp;
        while (!base.empty() && (isdigit((unsigned char)base[base.size() - 1]) || base[base.size() - 1] == '.'))
        {
            base.erase(base.size() - 1);
            lang = CvLookup(CvInterpreters, CvInterpreterCount, base.c_str(), NULL);
            if (lang >= 0)
                break;
        }
    }
    return lang;
}

static int CvModelineLanguage(const char* head, size_t n)
{
    // vim: ... ft=<name> / filetype=<name>; emacs: -*- mode: <name> -*-
    static const char* keys[] = {"ft=", "filetype=", "mode:"};
    for (int k = 0; k < 3; k++)
    {
        const char* key = keys[k];
        size_t kl = strlen(key);
        for (size_t i = 0; i + kl < n; i++)
        {
            if (_strnicmp(head + i, key, kl) != 0)
                continue;
            size_t j = i + kl;
            while (j < n && (head[j] == ' '))
                j++;
            size_t s = j;
            while (j < n && (isalnum((unsigned char)head[j]) || head[j] == '-' || head[j] == '+'))
                j++;
            if (j > s)
            {
                std::string name = CvLowerAscii(head + s, j - s);
                for (int L = 0; L < CvLanguageCount; L++)
                    if (strcmp(CvLanguages[L].Id, name.c_str()) == 0)
                        return L;
            }
        }
    }
    return -1;
}

static int CvSignatureLanguage(const char* head, size_t n)
{
    if (n >= 5 && _strnicmp(head, "<?xml", 5) == 0)
    {
        for (int i = 0; i < CvLanguageCount; i++)
            if (strcmp(CvLanguages[i].Id, "xml") == 0)
                return i;
    }
    if (n >= 9 && _strnicmp(head, "<!doctype", 9) == 0)
    {
        for (int i = 0; i < CvLanguageCount; i++)
            if (strcmp(CvLanguages[i].Id, "html") == 0)
                return i;
    }
    return -1;
}

int CvIdentifyLanguage(const char* nameUtf8, const char* head, size_t headLen)
{
    const char* base = nameUtf8;
    for (const char* p = nameUtf8; *p; p++)
        if (*p == '\\' || *p == '/')
            base = p + 1;
    std::string name = CvLowerAscii(base, strlen(base));

    // 1. exact file name
    int lang = CvLookup(CvExactNames, CvExactNameCount, name.c_str(), NULL);
    if (lang >= 0)
        return lang;

    // 2. longest compound suffix (".d.ts" before ".ts")
    for (size_t i = 0; i < name.size(); i++)
    {
        if (name[i] != '.')
            continue;
        int l = CvLookup(CvCompoundSuffixes, CvCompoundSuffixCount, name.c_str() + i, NULL);
        if (l >= 0)
            return l;
        break; // only the longest (leftmost dot) qualifies as a compound suffix
    }

    // 3. extension, with the ambiguity probe when the table names one
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos)
    {
        int rule = -1;
        lang = CvLookup(CvExtensions, CvExtensionCount, name.c_str() + dot, &rule);
        if (lang >= 0)
        {
            if (rule >= 0 && head != NULL && headLen > 0)
            {
                int probed = CvRunAmbiguityProbe(CvAmbiguityRules[rule].Id, head, headLen);
                if (probed >= 0)
                    return probed;
            }
            return lang;
        }
    }

    // 4. content: shebang, modeline, first-bytes signature (spec FR-005)
    if (head != NULL && headLen > 0)
    {
        lang = CvShebangLanguage(head, headLen);
        if (lang >= 0)
            return lang;
        lang = CvModelineLanguage(head, headLen);
        if (lang >= 0)
            return lang;
        lang = CvSignatureLanguage(head, headLen);
        if (lang >= 0)
            return lang;
    }
    return -1;
}

const char* CvLanguageDisplay(int language)
{
    if (language < 0 || language >= CvLanguageCount)
        return LoadStr(IDS_LANG_PLAIN);
    return CvLanguages[language].Display;
}

// ==========================================================================
// full intake
// ==========================================================================

BOOL CvLoadFile(const char* nameUtf8, int forcedEncoding, int forcedLanguage, CvIntake& out)
{
    out = CvIntake();
    HANDLE h = CvOpenRead(nameUtf8);
    if (h == INVALID_HANDLE_VALUE)
        return FALSE;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz))
    {
        CloseHandle(h);
        return FALSE;
    }
    out.FileSize = sz.QuadPart;
    __int64 limit = (__int64)g_viewerLimitMB * 1024 * 1024;
    if (out.FileSize > limit)
    {
        CloseHandle(h);
        out.Band = cvBandDeclined;
        return FALSE;
    }

    std::vector<BYTE> raw((size_t)out.FileSize);
    DWORD rd = 0;
    if (out.FileSize > 0 && (!ReadFile(h, &raw[0], (DWORD)out.FileSize, &rd, NULL) || rd == 0))
    {
        CloseHandle(h);
        return FALSE;
    }
    CloseHandle(h);
    raw.resize(rd);
    const BYTE* d = raw.empty() ? (const BYTE*)"" : &raw[0];
    size_t n = raw.size();

    if (!CvSniffIsText(d, (std::min<size_t>)(n, CV_SNIFF_BYTES)))
    {
        out.Band = cvBandDeclined;
        return FALSE;
    }

    // --- encoding (spec FR-024) ---
    UINT codePage = CP_ACP;
    if (forcedEncoding >= 0)
    {
        out.Encoding = (CvEncoding)forcedEncoding;
        if (out.Encoding == cvEncAnsi)
            codePage = CP_ACP;
    }
    else if (n >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF)
        out.Encoding = cvEncUtf8Bom;
    else if (n >= 2 && d[0] == 0xFF && d[1] == 0xFE)
        out.Encoding = cvEncUtf16LE;
    else if (n >= 2 && d[0] == 0xFE && d[1] == 0xFF)
        out.Encoding = cvEncUtf16BE;
    else
    {
        BOOL be = FALSE;
        if (CvLooksUtf16NoBom(d, (std::min<size_t>)(n, CV_SNIFF_BYTES), &be))
            out.Encoding = be ? cvEncUtf16BE : cvEncUtf16LE;
        else if (CvIsValidUtf8(d, n))
            out.Encoding = cvEncUtf8;
        else
            out.Encoding = cvEncAnsi;
    }

    CvDecode(d, n, out.Encoding, codePage, out);

    // --- language ---
    if (forcedLanguage >= 0)
    {
        out.Language = forcedLanguage;
        out.LanguageForced = TRUE;
    }
    else
    {
        size_t headLen = (std::min<size_t>)(out.Utf8.size(), CV_SNIFF_BYTES);
        out.Language = CvIdentifyLanguage(nameUtf8, out.Utf8.c_str(), headLen);
    }

    // --- band (spec FR-026) ---
    BOOL hasGrammar = out.Language >= 0 && CvLanguages[out.Language].Grammar != NULL;
    if (!hasGrammar)
        out.Band = cvBandHighlight; // "plain" is a rendering detail, not a band:
                                    // the page simply has no grammar to apply
    if (out.FileSize > (__int64)g_highlightLimitKB * 1024)
        out.Band = cvBandPlainSize;
    else if (out.LongestLine > g_maxLineLength)
        out.Band = cvBandPlainLine;
    else
        out.Band = cvBandHighlight;
    return TRUE;
}
