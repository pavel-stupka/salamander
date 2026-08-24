// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <windows.h>

#include "salunicode.h"

//*****************************************************************************
//
// WTF-8 fallback codec (feature 066)
//
// Windows file names are arbitrary 16-bit unit sequences and may contain
// unpaired surrogates, which strict UTF-8 cannot represent - the strict
// WinAPI conversions below fail for them, and before feature 066 that
// failure cost the file its operational identity (the enumeration fallback
// substituted U+FFFD, so delete/copy/move targeted a name that does not
// exist). The fallbacks here extend the pair to WTF-8: each unpaired
// surrogate U+D800..U+DFFF travels as its 3-byte sequence ED A0 80..ED BF BF.
// For valid Unicode input the output stays byte-identical to strict UTF-8
// (the WinAPI fast path runs first and the encoder mirrors it), and the
// decoder keeps rejecting every OTHER malformed input - the feature-004/063
// "valid UTF-8, else ANSI" heuristics depend on that failure. Binding
// contract: specs/066-fix-surrogate-filenames/contracts/name-encoding-wtf8.md
//

// TRUE when the string contains a surrogate unit without its valid partner
// (the only input the strict W->UTF-8 conversion can reject)
static BOOL SalWHasLoneSurrogate(const WCHAR* s, int len)
{
    for (int i = 0; len < 0 ? s[i] != 0 : i < len; i++)
    {
        WCHAR c = s[i];
        if (c >= 0xD800 && c <= 0xDBFF)
        {
            // len < 0: s[i] != 0, so s[i + 1] is readable (worst case the terminator)
            WCHAR next = (len < 0 || i + 1 < len) ? s[i + 1] : 0;
            if (next >= 0xDC00 && next <= 0xDFFF)
                i++; // valid pair, encoded as one 4-byte sequence
            else
                return TRUE;
        }
        else if (c >= 0xDC00 && c <= 0xDFFF)
            return TRUE;
    }
    return FALSE;
}

// encodes exactly 'units' UTF-16 units as WTF-8; returns the byte count, or
// -1 when 'buf' is too small (buf == NULL just measures)
static int SalWToWtf8Units(const WCHAR* src, int units, char* buf, int bufSize)
{
    int out = 0;
    for (int i = 0; i < units; i++)
    {
        DWORD cp = src[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < units &&
            src[i + 1] >= 0xDC00 && src[i + 1] <= 0xDFFF)
        { // valid pair -> one supplementary code point (identical to UTF-8)
            cp = 0x10000 + ((cp - 0xD800) << 10) + (src[i + 1] - 0xDC00);
            i++;
        }
        int seq = cp < 0x80 ? 1 : cp < 0x800 ? 2
                              : cp < 0x10000 ? 3 // BMP incl. a LONE surrogate (the WTF-8 extension)
                                             : 4;
        if (buf != NULL)
        {
            if (out + seq > bufSize)
                return -1;
            switch (seq)
            {
            case 1:
                buf[out] = (char)cp;
                break;
            case 2:
                buf[out] = (char)(0xC0 | (cp >> 6));
                buf[out + 1] = (char)(0x80 | (cp & 0x3F));
                break;
            case 3:
                buf[out] = (char)(0xE0 | (cp >> 12));
                buf[out + 1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                buf[out + 2] = (char)(0x80 | (cp & 0x3F));
                break;
            case 4:
                buf[out] = (char)(0xF0 | (cp >> 18));
                buf[out + 1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                buf[out + 2] = (char)(0x80 | ((cp >> 6) & 0x3F));
                buf[out + 3] = (char)(0x80 | (cp & 0x3F));
                break;
            }
        }
        out += seq;
    }
    return out;
}

// WTF-8 encoding fallback with SalWToU8's exact call semantics (terminator
// counting, too-small-buffer -> empty string + 0)
static int SalWToU8Wtf8(const WCHAR* src, int srcLen, char* buf, int bufSize)
{
    int units = srcLen < 0 ? (int)wcslen(src) + 1 : srcLen; // -1: convert the terminator too (WinAPI parity)
    if (buf == NULL)
    {
        int needed = SalWToWtf8Units(src, units, NULL, 0);
        return srcLen < 0 ? needed : needed + 1; // count includes the terminator (WinAPI/SalWToU8 parity)
    }
    int written = SalWToWtf8Units(src, units, buf, bufSize);
    if (written < 0)
    {
        if (bufSize > 0)
            buf[0] = 0;
        return 0;
    }
    if (srcLen >= 0)
    {
        if (written >= bufSize)
        {
            buf[0] = 0;
            return 0;
        }
        buf[written] = 0;
        written++;
    }
    return written;
}

// decodes 'bytes' bytes of WTF-8 (strict UTF-8 whose 3-byte sequences may
// additionally decode to a lone surrogate); returns the unit count, -1 on
// any other malformed input, -2 when 'buf' is too small
static int SalWtf8ToWBytes(const char* src, int bytes, WCHAR* buf, int bufSize)
{
    const unsigned char* s = (const unsigned char*)src;
    int out = 0;
    int i = 0;
    while (i < bytes)
    {
        DWORD cp;
        int seq;
        unsigned char b = s[i];
        if (b < 0x80)
        {
            cp = b;
            seq = 1;
        }
        else if (b >= 0xC2 && b <= 0xDF)
        {
            cp = b & 0x1F;
            seq = 2;
        }
        else if (b >= 0xE0 && b <= 0xEF)
        {
            cp = b & 0x0F;
            seq = 3;
        }
        else if (b >= 0xF0 && b <= 0xF4)
        {
            cp = b & 0x07;
            seq = 4;
        }
        else
            return -1; // stray continuation, overlong C0/C1, or F5..FF
        if (i + seq > bytes)
            return -1; // truncated sequence
        for (int k = 1; k < seq; k++)
        {
            unsigned char c = s[i + k];
            if (c < 0x80 || c > 0xBF)
                return -1;
            cp = (cp << 6) | (c & 0x3F);
        }
        // minimality and range checks match strict UTF-8; the ONLY extension
        // is that a 3-byte sequence may decode to a surrogate (WTF-8)
        if ((seq == 2 && cp < 0x80) ||
            (seq == 3 && cp < 0x800) ||
            (seq == 4 && (cp < 0x10000 || cp > 0x10FFFF)))
            return -1;
        if (cp >= 0x10000)
        {
            if (buf != NULL)
            {
                if (out + 2 > bufSize)
                    return -2;
                buf[out] = (WCHAR)(0xD800 + ((cp - 0x10000) >> 10));
                buf[out + 1] = (WCHAR)(0xDC00 + ((cp - 0x10000) & 0x3FF));
            }
            out += 2;
        }
        else
        {
            if (buf != NULL)
            {
                if (out + 1 > bufSize)
                    return -2;
                buf[out] = (WCHAR)cp;
            }
            out++;
        }
        i += seq;
    }
    return out;
}

// WTF-8 decoding fallback with SalU8ToW's exact call semantics; malformed
// non-WTF-8 input keeps failing (returns 0) - callers' ANSI heuristics rely on it
static int SalU8ToWWtf8(const char* src, int srcLen, WCHAR* buf, int bufSize)
{
    int bytes = srcLen < 0 ? (int)strlen(src) + 1 : srcLen; // -1: decode the terminator too (WinAPI parity)
    int res = SalWtf8ToWBytes(src, bytes, buf, buf == NULL ? 0 : bufSize);
    if (res < 0)
    {
        if (buf != NULL && bufSize > 0)
            buf[0] = 0;
        return 0;
    }
    if (srcLen >= 0)
    {
        if (buf != NULL)
        {
            if (res >= bufSize)
            {
                buf[0] = 0;
                return 0;
            }
            buf[res] = 0;
        }
        res++;
    }
    return res;
}

//*****************************************************************************
//
// SalU8ToW
//

int SalU8ToW(const char* src, int srcLen, WCHAR* buf, int bufSize)
{
    if (src == NULL)
    {
        if (buf != NULL && bufSize > 0)
            buf[0] = 0;
        return 0;
    }
    int res = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, srcLen,
                                  buf, buf == NULL ? 0 : bufSize);
    if (res == 0 && srcLen != 0)
    {
        // strict UTF-8 refused: accept WTF-8 (feature 066) - lone-surrogate
        // sequences decode to their unit, every other malformed input still
        // fails here exactly as before
        return SalU8ToWWtf8(src, srcLen, buf, bufSize);
    }
    if (res > 0 && srcLen >= 0)
    {
        // input was not null-terminated: report/write the terminator ourselves
        if (buf != NULL)
        {
            if (res >= bufSize)
            {
                buf[0] = 0;
                return 0;
            }
            buf[res] = 0;
        }
        res++;
    }
    if (res == 0 && buf != NULL && bufSize > 0)
        buf[0] = 0;
    return res;
}

//*****************************************************************************
//
// SalWToU8
//

int SalWToU8(const WCHAR* src, int srcLen, char* buf, int bufSize)
{
    if (src == NULL)
    {
        if (buf != NULL && bufSize > 0)
            buf[0] = 0;
        return 0;
    }
    int res = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, src, srcLen,
                                  buf, buf == NULL ? 0 : bufSize, NULL, NULL);
    if (res == 0 && SalWHasLoneSurrogate(src, srcLen))
    {
        // strict UTF-8 cannot carry an unpaired surrogate: encode as WTF-8
        // (feature 066) so every on-disk name round-trips losslessly
        return SalWToU8Wtf8(src, srcLen, buf, bufSize);
    }
    if (res > 0 && srcLen >= 0)
    {
        if (buf != NULL)
        {
            if (res >= bufSize)
            {
                buf[0] = 0;
                return 0;
            }
            buf[res] = 0;
        }
        res++;
    }
    if (res == 0 && buf != NULL && bufSize > 0)
        buf[0] = 0;
    return res;
}

//*****************************************************************************
//
// SalU8ToWAlloc / SalWToU8Alloc
//

WCHAR* SalU8ToWAlloc(const char* src, int srcLen)
{
    int size = SalU8ToW(src, srcLen, NULL, 0);
    if (size == 0)
        return NULL;
    WCHAR* buf = (WCHAR*)malloc(size * sizeof(WCHAR));
    if (buf == NULL)
        return NULL;
    if (SalU8ToW(src, srcLen, buf, size) == 0)
    {
        free(buf);
        return NULL;
    }
    return buf;
}

char* SalWToU8Alloc(const WCHAR* src, int srcLen)
{
    int size = SalWToU8(src, srcLen, NULL, 0);
    if (size == 0)
        return NULL;
    char* buf = (char*)malloc(size);
    if (buf == NULL)
        return NULL;
    if (SalWToU8(src, srcLen, buf, size) == 0)
    {
        free(buf);
        return NULL;
    }
    return buf;
}

//*****************************************************************************
//
// SalLegacyToU8Alloc
//

char* SalLegacyToU8Alloc(const char* src, int maxBytes)
{
    if (src == NULL)
        return NULL;

    char* u8;
    if (SalU8ToW(src, -1, NULL, 0) > 0) // WTF-8-aware probe (feature 066)
    {                                   // already valid UTF-8/WTF-8 (ASCII included) - keep the bytes unchanged
        int len = (int)strlen(src);
        u8 = (char*)malloc(len + 1);
        if (u8 == NULL)
            return NULL;
        memcpy(u8, src, len + 1);
    }
    else
    { // transitional tolerance: a not-yet-migrated producer passed ANSI bytes
        // (the same heuristic the registry facade applies on write, see
        // SalRegSetValueExW8)
        int wlen = MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);
        if (wlen <= 0)
            return NULL;
        WCHAR* w = (WCHAR*)malloc(wlen * sizeof(WCHAR));
        if (w == NULL)
            return NULL;
        MultiByteToWideChar(CP_ACP, 0, src, -1, w, wlen);
        u8 = SalWToU8Alloc(w, -1);
        free(w);
        if (u8 == NULL)
            return NULL;
    }

    if (maxBytes >= 0 && (int)strlen(u8) > maxBytes)
    { // clamp only at a UTF-8 sequence boundary so the result stays valid UTF-8
        int cut = maxBytes;
        while (cut > 0 && (u8[cut] & 0xC0) == 0x80)
            cut--;
        u8[cut] = 0;
    }
    return u8;
}

//*****************************************************************************
//
// SalWToACPLossless
//

BOOL SalWToACPLossless(const WCHAR* src, int srcLen, char* buf, int bufSize)
{
    if (src == NULL || buf == NULL || bufSize <= 0)
        return FALSE;
    BOOL usedDefault = FALSE;
    int res = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, src, srcLen,
                                  buf, bufSize, NULL, &usedDefault);
    if (res == 0 || usedDefault)
    {
        buf[0] = 0;
        return FALSE;
    }
    if (srcLen >= 0)
    {
        if (res >= bufSize)
        {
            buf[0] = 0;
            return FALSE;
        }
        buf[res] = 0;
    }
    return TRUE;
}

//*****************************************************************************
//
// SalNormalizeNFC
//

int SalNormalizeNFC(const WCHAR* src, int srcLen, WCHAR* buf, int bufSize)
{
    if (src == NULL)
        return 0;
    if (buf == NULL)
    {
        int est = NormalizeString(NormalizationC, src, srcLen, NULL, 0);
        if (est <= 0)
            return 0;
        if (srcLen >= 0)
            est++; // room for the terminator we add ourselves
        return est;
    }
    int res = NormalizeString(NormalizationC, src, srcLen, buf, bufSize);
    if (res <= 0)
    {
        if (bufSize > 0)
            buf[0] = 0;
        return 0;
    }
    if (srcLen >= 0)
    {
        if (res >= bufSize)
        {
            buf[0] = 0;
            return 0;
        }
        buf[res] = 0;
        res++;
    }
    return res;
}

WCHAR* SalNormalizeNFCAlloc(const WCHAR* src, int srcLen)
{
    if (src == NULL)
        return 0;
    int size = SalNormalizeNFC(src, srcLen, NULL, 0);
    if (size <= 0)
        return NULL;
    for (;;)
    {
        WCHAR* buf = (WCHAR*)malloc(size * sizeof(WCHAR));
        if (buf == NULL)
            return NULL;
        int res = NormalizeString(NormalizationC, src, srcLen, buf, size);
        if (res > 0)
        {
            if (srcLen >= 0)
            {
                if (res >= size)
                {
                    free(buf);
                    size = res + 1;
                    continue;
                }
                buf[res] = 0;
            }
            return buf;
        }
        free(buf);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return NULL;
        size = -res; // NormalizeString returns the needed size negated
        if (srcLen >= 0)
            size++;
    }
}

//*****************************************************************************
//
// SalIsASCII
//

BOOL SalIsASCII(const char* s, int len)
{
    if (s == NULL)
        return TRUE;
    if (len < 0)
    {
        for (; *s != 0; s++)
            if ((unsigned char)*s >= 0x80)
                return FALSE;
    }
    else
    {
        for (int i = 0; i < len; i++)
            if ((unsigned char)s[i] >= 0x80)
                return FALSE;
    }
    return TRUE;
}

//*****************************************************************************
//
// SalU8Next / SalU8CharCount
//

const char* SalU8Next(const char* s)
{
    if (*s != 0)
    {
        s++;
        while ((*s & 0xC0) == 0x80) // skip continuation bytes
            s++;
    }
    return s;
}

void SalU8TrimIncompleteTail(char* buf)
{
    if (buf == NULL)
        return;
    int len = (int)strlen(buf);
    int i = len;
    while (i > 0 && ((unsigned char)buf[i - 1] & 0xC0) == 0x80)
        i--; // walk back over the continuation bytes
    if (i > 0)
    {
        unsigned char lead = (unsigned char)buf[i - 1];
        if (lead >= 0xC0) // a lead byte: check whether its sequence is complete
        {
            int seqLen = lead >= 0xF0 ? 4 : (lead >= 0xE0 ? 3 : 2);
            if (len - (i - 1) < seqLen) // fewer bytes present than promised
                buf[i - 1] = 0;         // the sequence was cut: drop it whole
        }
    }
}

int SalU8CharCount(const char* s, int len)
{
    if (len < 0)
        len = (int)strlen(s);
    int count = 0;
    int i;
    for (i = 0; i < len; i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80)
            count++;
    return count;
}

//*****************************************************************************
//
// helpers: convert a UTF-8 name to its NFC UTF-16 form (transient)
//

static WCHAR* SalU8ToNFCAlloc(const char* u8, int len)
{
    WCHAR* w = SalU8ToWAlloc(u8, len);
    if (w == NULL)
        return NULL;
    WCHAR* nfc = SalNormalizeNFCAlloc(w, -1);
    free(w);
    return nfc;
}

//*****************************************************************************
//
// SalNameEquivalent
//

BOOL SalNameEquivalent(const char* u8a, const char* u8b)
{
    if (u8a == NULL || u8b == NULL)
        return u8a == u8b;
    if (strcmp(u8a, u8b) == 0)
        return TRUE; // identical bytes are always equivalent
    if (SalIsASCII(u8a) && SalIsASCII(u8b))
        return FALSE; // different ASCII bytes cannot be equivalent
    WCHAR* na = SalU8ToNFCAlloc(u8a, -1);
    WCHAR* nb = SalU8ToNFCAlloc(u8b, -1);
    BOOL eq = na != NULL && nb != NULL && wcscmp(na, nb) == 0;
    if (na != NULL)
        free(na);
    if (nb != NULL)
        free(nb);
    return eq;
}

//*****************************************************************************
//
// SalCompareNamesUTF8
//

int SalCompareNamesUTF8(const char* u8a, int aLen, const char* u8b, int bLen, BOOL ignoreCase)
{
    if (u8a == NULL || u8b == NULL)
        return (u8a == NULL) - (u8b == NULL);
    WCHAR* na = SalU8ToNFCAlloc(u8a, aLen);
    WCHAR* nb = SalU8ToNFCAlloc(u8b, bLen);
    int res;
    if (na == NULL || nb == NULL)
    { // unconvertible input: deterministic byte-wise fallback, items must not vanish from sort
        int la = aLen < 0 ? (int)strlen(u8a) : aLen;
        int lb = bLen < 0 ? (int)strlen(u8b) : bLen;
        res = memcmp(u8a, u8b, la < lb ? la : lb);
        if (res == 0)
            res = la - lb;
    }
    else
    {
        res = CompareStringEx(LOCALE_NAME_USER_DEFAULT,
                              ignoreCase ? LINGUISTIC_IGNORECASE : 0,
                              na, -1, nb, -1, NULL, NULL, 0);
        res = res == 0 ? strcmp(u8a, u8b) /* API failure fallback */ : res - CSTR_EQUAL;
    }
    if (na != NULL)
        free(na);
    if (nb != NULL)
        free(nb);
    return res;
}

//*****************************************************************************
//
// SalNameEqualCI
//

BOOL SalNameEqualCI(const char* u8a, int aLen, const char* u8b, int bLen)
{
    if (u8a == NULL || u8b == NULL)
        return u8a == u8b;
    if (SalIsASCII(u8a, aLen) && SalIsASCII(u8b, bLen))
    {
        int la = aLen < 0 ? (int)strlen(u8a) : aLen;
        int lb = bLen < 0 ? (int)strlen(u8b) : bLen;
        if (la != lb)
            return FALSE;
        return _strnicmp(u8a, u8b, la) == 0;
    }
    return SalCompareNamesUTF8(u8a, aLen, u8b, bLen, TRUE) == 0;
}

//*****************************************************************************
//
// SalU8ToWDisplay
//
// Lenient counterpart of SalU8ToW, for display only. MultiByteToWideChar
// without MB_ERR_INVALID_CHARS substitutes U+FFFD for malformed sequences and
// keeps going, which is exactly the degradation a display surface wants: one
// bad byte costs one character, not the whole string.
//

int SalU8ToWDisplay(const char* src, int srcLen, WCHAR* buf, int bufSize)
{
    if (src == NULL)
    {
        if (buf != NULL && bufSize > 0)
            buf[0] = 0;
        return 0;
    }
    // names may be WTF-8 (feature 066): the strict decoder maps a
    // lone-surrogate sequence to its true unit, which paints like Explorer
    // (the font's notdef glyph); only input that is not WTF-8 falls through
    // to the lenient substitution below
    int strict = SalU8ToW(src, srcLen, buf, bufSize);
    if (strict > 0)
        return strict;
    int res = MultiByteToWideChar(CP_UTF8, 0, src, srcLen,
                                  buf, buf == NULL ? 0 : bufSize);
    if (res > 0 && srcLen >= 0)
    {
        // input was not null-terminated: report/write the terminator ourselves
        if (buf != NULL)
        {
            if (res >= bufSize)
            {
                buf[0] = 0;
                return 0;
            }
            buf[res] = 0;
        }
        res++;
    }
    if (res == 0 && buf != NULL && bufSize > 0)
        buf[0] = 0;
    return res;
}

WCHAR* SalU8ToWDisplayAlloc(const char* src, int srcLen)
{
    if (src == NULL)
        return NULL;
    int need = SalU8ToWDisplay(src, srcLen, NULL, 0);
    if (need <= 0)
        return NULL;
    if (srcLen >= 0)
        need++; // room for the terminator we add ourselves
    WCHAR* buf = (WCHAR*)malloc(need * sizeof(WCHAR));
    if (buf == NULL)
        return NULL;
    if (SalU8ToWDisplay(src, srcLen, buf, need) == 0)
    {
        free(buf);
        return NULL;
    }
    return buf;
}

//*****************************************************************************
//
// Locale text as UTF-8
//
// Each wrapper calls the W variant and transcodes to UTF-8. The returned byte
// count matches what the A variant would have returned for an ASCII result, so
// callers that test "== 0" or subtract 1 for the length keep working.
//

// converts a wide result into the caller's UTF-8 buffer; 'wideLen' counts
// WCHARs INCLUDING the terminating null, as the locale APIs report it
static int LocaleWideToU8(const WCHAR* wide, int wideLen, char* u8Buf, int u8BufSize)
{
    if (wideLen <= 0)
    {
        if (u8Buf != NULL && u8BufSize > 0)
            u8Buf[0] = 0;
        return 0;
    }
    if (wideLen == 1) // the API returned an empty string (terminator only)
    {
        if (u8Buf == NULL)
            return 1;
        if (u8BufSize < 1)
            return 0;
        u8Buf[0] = 0;
        return 1;
    }
    // the APIs include the terminator in the count; SalWToU8 adds its own
    int res = SalWToU8(wide, wideLen - 1, u8Buf, u8BufSize);
    if (res == 0 && u8Buf != NULL && u8BufSize > 0)
        u8Buf[0] = 0;
    return res;
}

int SalGetLocaleInfoU8(LCID locale, LCTYPE lcType, char* u8Buf, int u8BufSize)
{
    WCHAR wide[256];
    int wideLen = GetLocaleInfoW(locale, lcType, wide, _countof(wide));
    return LocaleWideToU8(wide, wideLen, u8Buf, u8BufSize);
}

int SalGetDateFormatU8(LCID locale, DWORD flags, const SYSTEMTIME* date,
                       const char* u8Format, char* u8Buf, int u8BufSize)
{
    WCHAR wideFormat[128];
    const WCHAR* format = NULL;
    if (u8Format != NULL)
    {
        if (SalU8ToW(u8Format, -1, wideFormat, _countof(wideFormat)) == 0)
            return 0;
        format = wideFormat;
    }
    WCHAR wide[256];
    int wideLen = GetDateFormatW(locale, flags, date, format, wide, _countof(wide));
    return LocaleWideToU8(wide, wideLen, u8Buf, u8BufSize);
}

int SalGetTimeFormatU8(LCID locale, DWORD flags, const SYSTEMTIME* time,
                       const char* u8Format, char* u8Buf, int u8BufSize)
{
    WCHAR wideFormat[128];
    const WCHAR* format = NULL;
    if (u8Format != NULL)
    {
        if (SalU8ToW(u8Format, -1, wideFormat, _countof(wideFormat)) == 0)
            return 0;
        format = wideFormat;
    }
    WCHAR wide[256];
    int wideLen = GetTimeFormatW(locale, flags, time, format, wide, _countof(wide));
    return LocaleWideToU8(wide, wideLen, u8Buf, u8BufSize);
}
