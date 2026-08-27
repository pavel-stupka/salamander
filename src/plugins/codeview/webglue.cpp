// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// webglue.cpp - asset serving, accelerator mapping and the typed message
// channel between the host and the page.

#include "precomp.h"
#include "webglue.h"
#include "intake.h"
#include "langmap.h"
#include "schemes.h"

#include <map>

// The generated table needs this shape; keep the two in step with
// tools/codeview/build_web.py (emit_resource_tables).
struct CodeViewAsset
{
    const char* Url;
    int ResId;
    const char* Mime;
};
#include "web\assets_table.inc"

static const int g_assetCount = (int)(sizeof(g_assets) / sizeof(g_assets[0]));

// ==========================================================================
// asset serving (contracts/rendering-lockdown.md S3)
// ==========================================================================

// Resources live in the .SPL and are therefore covered by its code signature;
// nothing is ever read from disk, the profile or the network.
static BOOL CvLoadResource(int id, const BYTE** data, size_t* size)
{
    HRSRC h = FindResourceW(DLLInstance, MAKEINTRESOURCEW(id), (LPCWSTR)RT_RCDATA);
    if (h == NULL)
        return FALSE;
    HGLOBAL g = LoadResource(DLLInstance, h);
    if (g == NULL)
        return FALSE;
    const void* p = LockResource(g);
    DWORD n = SizeofResource(DLLInstance, h);
    if (p == NULL || n == 0)
        return FALSE;
    *data = (const BYTE*)p;
    *size = n;
    return TRUE;
}

static std::string CvWideToUtf8(const std::wstring& w)
{
    if (w.empty())
        return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string s((size_t)(n > 0 ? n : 0), 0);
    if (n > 0)
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}

static std::wstring CvUtf8ToWide(const char* s)
{
    if (s == NULL || *s == 0)
        return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    std::wstring w((size_t)(n > 1 ? n - 1 : 0), 0);
    if (n > 1)
        MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n);
    return w;
}

void CvConfigureHost(TcWebHostConfig& cfg, const std::string** textProvider)
{
    cfg.VirtualHost = L"codeview.invalid";
    cfg.DocumentPath = L"viewer.html";
    // The bundled highlighting engine is script (spec clarification
    // 2026-08-26); the compensating lockdown is in the shared host and the CSP.
    cfg.ScriptsEnabled = true;
    cfg.WebMessagesEnabled = true;
    cfg.TraceName = "codeview";

    cfg.Serve = [textProvider](const std::wstring& path, TcWebResponse& out) -> bool
    {
        std::string p = CvWideToUtf8(path);
        // The decoded document. Served, never inlined into HTML: the page
        // inserts it with textContent, so file content can never be markup
        // (spec FR-030).
        if (p == "text")
        {
            const std::string* text = (textProvider != NULL) ? *textProvider : NULL;
            if (text == NULL)
                return false;
            out.Data = text->empty() ? (const BYTE*)"" : (const BYTE*)text->data();
            out.Size = text->size();
            out.ContentType = L"text/plain; charset=utf-8";
            // The text changes per file; never let the engine reuse an old copy.
            out.ExtraHeaders = L"Cache-Control: no-store";
            return true;
        }
        for (int i = 0; i < g_assetCount; i++)
        {
            if (p == g_assets[i].Url)
            {
                const BYTE* d = NULL;
                size_t n = 0;
                if (!CvLoadResource(g_assets[i].ResId, &d, &n))
                    return false;
                out.Data = d;
                out.Size = n;
                out.ContentType = CvUtf8ToWide(g_assets[i].Mime);
                return true;
            }
        }
        return false; // everything else -> the host's 403
    };

    // Keyboard parity with the built-in text viewer
    // (contracts/host-page-interface.md S5). Keys we do not claim reach the
    // page; browser accelerators are already disabled by the lockdown, so
    // Ctrl+P, F5, Ctrl+S and F12 are inert without being listed here.
    cfg.Accelerator = [](UINT vk, bool ctrl, bool shift) -> int
    {
        if (vk == VK_ESCAPE)
            return CM_FILE_CLOSE;
        if (vk == VK_F3)
            return shift ? CM_EDIT_FINDPREV : CM_EDIT_FINDNEXT;
        if (vk == VK_F6)
            return shift ? CM_EDIT_FINDPREV : CM_EDIT_FINDNEXT;
        if (vk == VK_F9)
            return shift ? CM_SCHEME_PREV : CM_SCHEME_NEXT;
        if (vk == VK_F2)
            return CM_VIEW_WRAP;
        if (vk == VK_F8)
            return shift ? CM_ENCODING_PREV : CM_ENCODING_NEXT;
        if (ctrl)
        {
            // Ctrl+C / Ctrl+A are what the Edit menu advertises, so they must
            // do what the menu items do. Left to the engine they act on the
            // DOM, which holds only the materialised rows -- Ctrl+A on a long
            // file selected a fraction of it and Ctrl+C then copied that
            // fraction, the very truncation FR-021 forbids.
            if (vk == 'C')
                return CM_EDIT_COPY;
            if (vk == 'A')
                return CM_EDIT_SELALL;
            if (vk == 'F')
                return CM_EDIT_FIND;
            if (vk == 'G')
                return CM_EDIT_GOTO;
            if (vk == 'W')
                return CM_VIEW_WRAP;
            // Ctrl+wheel stays the engine's (IsZoomControlEnabled), but the
            // zoom KEYS are browser accelerators and the shared lockdown turns
            // those off (webhost.cpp put_AreBrowserAcceleratorKeysEnabled) --
            // the same reason Ctrl+0 is claimed here. Focus lives inside the
            // WebView, so the frame's accelerator table never sees them and
            // Ctrl+Plus/Minus did nothing at all (spec FR-020).
            if (vk == '0' || vk == VK_NUMPAD0)
                return CM_VIEW_ZOOMRESET;
            if (vk == VK_OEM_PLUS || vk == VK_ADD)
                return CM_VIEW_ZOOMIN;
            if (vk == VK_OEM_MINUS || vk == VK_SUBTRACT)
                return CM_VIEW_ZOOMOUT;
            if (vk == VK_NEXT)
                return CM_NEXTFILE;
            if (vk == VK_PRIOR)
                return CM_PREVFILE;
        }
        return 0;
    };
}

// ==========================================================================
// messages: host -> page
// ==========================================================================

static void CvJsonEscape(const std::wstring& s, std::wstring& out)
{
    for (size_t i = 0; i < s.size(); i++)
    {
        wchar_t c = s[i];
        switch (c)
        {
        case L'"':
            out += L"\\\"";
            break;
        case L'\\':
            out += L"\\\\";
            break;
        case L'\n':
            out += L"\\n";
            break;
        case L'\r':
            out += L"\\r";
            break;
        case L'\t':
            out += L"\\t";
            break;
        default:
            if (c < 0x20)
            {
                wchar_t buf[8];
                swprintf_s(buf, L"\\u%04x", (unsigned)c);
                out += buf;
            }
            else
                out += c;
        }
    }
}

static std::wstring CvJsonStr(const std::wstring& s)
{
    std::wstring r = L"\"";
    CvJsonEscape(s, r);
    r += L"\"";
    return r;
}

static std::wstring CvJsonStrA(const char* s)
{
    return CvJsonStr(CvUtf8ToWide(s));
}

static std::wstring CvNum(int v)
{
    wchar_t b[24];
    _itow_s(v, b, 10);
    return b;
}

// "#rrggbb" from a COLORREF; manual shifts, not GetGValue/GetBValue -- their
// (WORD) cast trips /RTCc in debug builds (same rule as webhost.cpp).
static std::wstring CvHexColor(COLORREF c)
{
    wchar_t b[8];
    swprintf_s(b, L"#%02x%02x%02x", (unsigned)(c & 0xFF), (unsigned)((c >> 8) & 0xFF),
               (unsigned)((c >> 16) & 0xFF));
    return b;
}

// {"type":"dark","bg":"#1e1e1e","fg":"#d4d4d4"} -- the host-known subset of
// the theme; the worker's full palette refines it once tokenization is ready.
static std::wstring CvJsonThemeInfo(const CvScheme* s)
{
    std::wstring m = L"{\"type\":";
    m += s->Dark ? L"\"dark\"" : L"\"light\"";
    m += L",\"bg\":\"" + CvHexColor(s->Bg) + L"\"";
    m += L",\"fg\":\"" + CvHexColor(s->Fg) + L"\"}";
    return m;
}

std::wstring CvSchemeFragment(const CvScheme* s)
{
    std::wstring f = L"bg=" + CvHexColor(s->Bg).substr(1);
    f += L"&fg=" + CvHexColor(s->Fg).substr(1);
    f += s->Dark ? L"&polarity=dark" : L"&polarity=light";
    return f;
}

std::wstring CvMsgInit(const CvIntake& intake, const CvScheme* scheme, BOOL swap)
{
    const char* grammar = NULL;
    if (intake.Language >= 0 && intake.Language < CvLanguageCount)
        grammar = CvLanguages[intake.Language].Grammar;
    BOOL highlight = (intake.Band == cvBandHighlight) && grammar != NULL;

    // LoadStrW, never LoadStr: the notice is localized, LoadStr returns ANSI
    // and this channel is wide/UTF-8 -- an ANSI Czech string would reach the
    // page as mojibake (fix-log defect 6).
    const wchar_t* reason = NULL;
    if (intake.Band == cvBandPlainSize)
        reason = SalamanderGeneral->LoadStrW(HLanguage, IDS_PLAIN_TOO_LARGE);
    else if (intake.Band == cvBandPlainLine)
        reason = SalamanderGeneral->LoadStrW(HLanguage, IDS_PLAIN_LONG_LINES);

    std::wstring m = L"{\"type\":";
    m += swap ? L"\"swapText\"" : L"\"init\"";
    m += L",\"lang\":";
    m += grammar ? CvJsonStrA(grammar) : L"null";
    m += L",\"highlight\":";
    m += highlight ? L"true" : L"false";
    m += L",\"theme\":" + CvJsonStrA(scheme->Id);
    m += L",\"themeInfo\":" + CvJsonThemeInfo(scheme);
    m += L",\"wrap\":";
    m += g_wrap ? L"true" : L"false";
    m += L",\"lineNumbers\":";
    m += g_lineNumbers ? L"true" : L"false";
    m += L",\"showWhitespace\":";
    m += g_whitespace ? L"true" : L"false";
    m += L",\"tabSize\":" + CvNum(g_tabWidth);
    m += L",\"fontFamily\":" + CvJsonStrA(g_fontFamily);
    m += L",\"fontSize\":" + CvNum(g_fontSize);
    m += L",\"maxLineLength\":" + CvNum(g_maxLineLength);
    m += L",\"trailingNewline\":";
    m += intake.TrailingNewline ? L"true" : L"false";
    if (reason != NULL)
        m += L",\"plainReason\":" + CvJsonStr(reason);
    m += L",\"v\":" + CvNum((int)GetTickCount());
    m += L"}";
    return m;
}

std::wstring CvMsgSetTheme(const CvScheme* scheme)
{
    return L"{\"type\":\"setTheme\",\"theme\":" + CvJsonStrA(scheme->Id) +
           L",\"themeInfo\":" + CvJsonThemeInfo(scheme) + L"}";
}

std::wstring CvMsgSetView()
{
    std::wstring m = L"{\"type\":\"setView\",\"wrap\":";
    m += g_wrap ? L"true" : L"false";
    m += L",\"lineNumbers\":";
    m += g_lineNumbers ? L"true" : L"false";
    m += L",\"showWhitespace\":";
    m += g_whitespace ? L"true" : L"false";
    m += L",\"tabSize\":" + CvNum(g_tabWidth);
    m += L",\"fontFamily\":" + CvJsonStrA(g_fontFamily);
    m += L",\"fontSize\":" + CvNum(g_fontSize);
    m += L"}";
    return m;
}

std::wstring CvMsgFind(const wchar_t* term, BOOL caseSensitive, BOOL wholeWord, int dir)
{
    std::wstring m = L"{\"type\":\"find\",\"term\":" + CvJsonStr(term ? term : L"");
    m += L",\"caseSensitive\":";
    m += caseSensitive ? L"true" : L"false";
    m += L",\"wholeWord\":";
    m += wholeWord ? L"true" : L"false";
    m += L",\"dir\":" + CvNum(dir) + L"}";
    return m;
}

std::wstring CvMsgGotoLine(int line, int col)
{
    return L"{\"type\":\"gotoLine\",\"line\":" + CvNum(line) + L",\"col\":" + CvNum(col) + L"}";
}

std::wstring CvMsgCommand(const wchar_t* type)
{
    return L"{\"type\":" + CvJsonStr(type ? type : L"") + L"}";
}

std::wstring CvMsgNotice(const wchar_t* text)
{
    return L"{\"type\":\"notice\",\"text\":" + CvJsonStr(text ? text : L"") + L"}";
}

// ==========================================================================
// messages: page -> host
// ==========================================================================
//
// A deliberately small, hand-written reader: the page is our own code, but the
// channel is still treated as untrusted input (contract S3) -- unknown types
// are ignored, numbers are clamped, and nothing here can name a path or a
// command.

static BOOL CvJsonFindKey(const std::wstring& j, const wchar_t* key, size_t& pos)
{
    std::wstring pat = L"\"";
    pat += key;
    pat += L"\":";
    size_t at = j.find(pat);
    if (at == std::wstring::npos)
        return FALSE;
    pos = at + pat.size();
    while (pos < j.size() && (j[pos] == L' '))
        pos++;
    return TRUE;
}

static int CvJsonInt(const std::wstring& j, const wchar_t* key, int def, int lo, int hi)
{
    size_t pos;
    if (!CvJsonFindKey(j, key, pos))
        return def;
    int sign = 1;
    if (pos < j.size() && j[pos] == L'-')
    {
        sign = -1;
        pos++;
    }
    if (pos >= j.size() || !iswdigit(j[pos]))
        return def;
    __int64 v = 0;
    while (pos < j.size() && iswdigit(j[pos]) && v < 0x7FFFFFFF)
        v = v * 10 + (j[pos++] - L'0');
    v *= sign;
    if (v < lo)
        v = lo;
    if (v > hi)
        v = hi;
    return (int)v;
}

static BOOL CvJsonBool(const std::wstring& j, const wchar_t* key)
{
    size_t pos;
    if (!CvJsonFindKey(j, key, pos))
        return FALSE;
    return j.compare(pos, 4, L"true") == 0;
}

static std::wstring CvJsonString(const std::wstring& j, const wchar_t* key, size_t maxLen)
{
    size_t pos;
    if (!CvJsonFindKey(j, key, pos))
        return std::wstring();
    if (pos >= j.size() || j[pos] != L'"')
        return std::wstring();
    pos++;
    std::wstring out;
    // Real unescaping. The lenient "skip the backslash, take the next
    // character" version turned \n into the letter 'n', which was invisible
    // while this only read short "type"/"reason" values but destroys every
    // line break of a copied selection.
    while (pos < j.size() && j[pos] != L'"' && out.size() < maxLen)
    {
        wchar_t c = j[pos++];
        if (c != L'\\')
        {
            out += c;
            continue;
        }
        if (pos >= j.size())
            break;
        wchar_t e = j[pos++];
        switch (e)
        {
        case L'n': out += L'\n'; break;
        case L'r': out += L'\r'; break;
        case L't': out += L'\t'; break;
        case L'b': out += L'\b'; break;
        case L'f': out += L'\f'; break;
        case L'u':
        {
            unsigned v = 0;
            int k = 0;
            for (; k < 4 && pos < j.size(); k++, pos++)
            {
                wchar_t h = j[pos];
                unsigned d;
                if (h >= L'0' && h <= L'9') d = (unsigned)(h - L'0');
                else if (h >= L'a' && h <= L'f') d = (unsigned)(h - L'a') + 10;
                else if (h >= L'A' && h <= L'F') d = (unsigned)(h - L'A') + 10;
                else break;
                v = v * 16 + d;
            }
            if (k == 4)
                out += (wchar_t)v; // surrogates pass through as their own units
            break;
        }
        default: out += e; break; // covers \" \\ \/
        }
    }
    return out;
}

BOOL CvParsePageMessage(const std::wstring& json, CvPageMessage& out)
{
    out = CvPageMessage();
    out.Type = CvJsonString(json, L"type", 32);
    if (out.Type.empty())
        return FALSE;
    if (out.Type == L"findResult")
    {
        out.Current = CvJsonInt(json, L"current", 0, 0, 1000000000);
        out.Total = CvJsonInt(json, L"total", 0, 0, 1000000000);
        return TRUE;
    }
    if (out.Type == L"caret")
    {
        out.Line = CvJsonInt(json, L"line", 1, 1, 1000000000);
        out.Col = CvJsonInt(json, L"col", 1, 1, 1000000000);
        return TRUE;
    }
    if (out.Type == L"contextMenu")
    {
        out.X = CvJsonInt(json, L"x", 0, 0, 100000);
        out.Y = CvJsonInt(json, L"y", 0, 0, 100000);
        out.HasSelection = CvJsonBool(json, L"hasSelection");
        return TRUE;
    }
    if (out.Type == L"rendered")
    {
        out.Lines = CvJsonInt(json, L"lines", 0, 0, 1000000000);
        return TRUE;
    }
    if (out.Type == L"copyText")
    {
        // Bounded by the virtual list: only materialised rows can be selected
        // with the mouse, so a selection cannot be document-sized. A whole-file
        // copy travels as "all":true and is built by the host from its own
        // intake instead of crossing this channel.
        out.All = CvJsonBool(json, L"all");
        if (!out.All)
            out.Text = CvJsonString(json, L"text", 4 * 1024 * 1024);
        return TRUE;
    }
    if (out.Type == L"ready" || out.Type == L"highlightDone")
        return TRUE;
    if (out.Type == L"highlightAborted")
    {
        out.Reason = CvJsonString(json, L"reason", 120);
        return TRUE;
    }
    return FALSE; // unknown type: ignored, as the contract requires
}
