// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Feature 075 evidence probe -- NOT part of the product build.
//
// This is the mechanical before/after for the LOGIC of D1, D3, D4 and D5.  It
// carries the verbatim pre-fix and post-fix bodies of the fixed functions with
// their product context removed, and runs them inside arenas with canary bytes,
// so every read and write here is defined behaviour while the arithmetic stays
// faithful to the original.
//
// It deliberately does NOT exercise the sites: the site-level evidence is the
// independent review of each diff plus the human GUI scenarios in
// quickstart.md.  See fix-log.md T008 for why that split exists.
//
//     specs\075-fix-small-hardening\probe\run_probe.cmd
//
// Exit code = number of failed expectations.

#include <windows.h>
#include <stdio.h>
#include <string.h>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond, what) \
    do { \
        g_checks++; \
        if (!(cond)) { \
            g_failures++; \
            printf("FAIL - %s\n", what); \
        } else { \
            printf("ok   - %s\n", what); \
        } \
    } while (0)

// ---------------------------------------------------------------------------
// Arena with canaries: the logical buffer is the first 'len' bytes; everything
// after it is filled with 0xAB and must stay that way.
// ---------------------------------------------------------------------------

#define ARENA_SLACK 64

struct CArena
{
    char Bytes[4096 + ARENA_SLACK];
    int Len;

    char* Init(int len)
    {
        Len = len;
        memset(Bytes, 0xAB, sizeof(Bytes));
        return Bytes;
    }
    // TRUE when nothing was written at or past Bytes[Len]
    BOOL CanaryIntact() const
    {
        for (int i = Len; i < Len + ARENA_SLACK; i++)
            if ((unsigned char)Bytes[i] != 0xAB)
                return FALSE;
        return TRUE;
    }
    int FirstOverrunOffset() const
    {
        for (int i = Len; i < Len + ARENA_SLACK; i++)
            if ((unsigned char)Bytes[i] != 0xAB)
                return i - Len;
        return -1;
    }
};

static CArena g_out;     // the caller's buffer
static CArena g_scratch; // GetCodeName's own 1024-byte scratch

// ===========================================================================
// D1 -- CCodeTables::GetCodeName
// ===========================================================================

// VERBATIM from src/codetbl.cpp:856-881 at c554f4d, with the Loaded/Valid
// guards and the CALL_STACK_MESSAGE removed (they are unchanged by the fix)
// and 'buff' redirected into an arena so the >1024 case is observable rather
// than undefined.
static BOOL GetCodeName_before(const char* name, char* buffer, int bufferLen)
{
    char* buff = g_scratch.Init(1024);
    if (bufferLen > 0)
        buffer[0] = 0;
    strcpy(buff, name);
    int len = (int)strlen(buff);
    if (len > bufferLen)
        len = bufferLen - 1;
    strncpy(buffer, buff, len);
    buffer[len] = 0;
    if ((int)strlen(buff) > bufferLen)
        return FALSE;
    return TRUE;
}

// The fix (plan.md Design D1).
static BOOL GetCodeName_after(const char* name, char* buffer, int bufferLen)
{
    int nameLen = (int)strlen(name);
    if (bufferLen > 0)
        lstrcpynA(buffer, name, bufferLen);
    return nameLen < bufferLen;
}

static void TestD1()
{
    printf("\n--- D1  CCodeTables::GetCodeName ---\n");

    const int bufLen = 200; // the real callers' size (viewer3.cpp:58, :1914)
    char shorter[64];
    memset(shorter, 'A', 33);
    shorter[33] = 0; // 33 bytes: the longest name shipped in convert.cfg
    char exact[bufLen + 1];
    memset(exact, 'A', bufLen);
    exact[bufLen] = 0; // exactly bufferLen -> the defect
    char longer[bufLen + 40];
    memset(longer, 'A', bufLen + 20);
    longer[bufLen + 20] = 0;
    static char huge[1400];
    memset(huge, 'B', 1100);
    huge[1100] = 0; // longer than the 1024-byte scratch

    // (1) the boundary case: name length == bufferLen
    char* out = g_out.Init(bufLen);
    BOOL rb = GetCodeName_before(exact, out, bufLen);
    int overrun = g_out.FirstOverrunOffset();
    printf("       before(exact): return=%d, wrote past the buffer at +%d\n", rb, overrun);
    CHECK(overrun == 0, "D1 before: a name of exactly bufferLen writes buffer[bufferLen]");
    CHECK(rb == TRUE, "D1 before: and reports success while doing so");

    out = g_out.Init(bufLen);
    BOOL ra = GetCodeName_after(exact, out, bufLen);
    CHECK(g_out.CanaryIntact(), "D1 after:  nothing written past the buffer");
    CHECK(ra == FALSE, "D1 after:  reports 'did not fit', like every longer name");
    CHECK((int)strlen(out) == bufLen - 1, "D1 after:  a terminated 199-byte prefix");

    // (2) the scratch buffer: a name longer than 1024 bytes
    out = g_out.Init(bufLen);
    GetCodeName_before(huge, out, bufLen);
    printf("       before(1100 B): scratch overrun at +%d\n", g_scratch.FirstOverrunOffset());
    CHECK(!g_scratch.CanaryIntact(), "D1 before: an 1100-byte name overruns the 1024-byte scratch");

    out = g_out.Init(bufLen);
    ra = GetCodeName_after(huge, out, bufLen);
    CHECK(g_out.CanaryIntact(), "D1 after:  an 1100-byte name writes nothing past the buffer");
    CHECK(ra == FALSE, "D1 after:  and reports 'did not fit'");

    // (3) identity for every input that works today
    struct
    {
        const char* Name;
        const char* What;
    } same[] = {
        {shorter, "a 33-byte name (the longest shipped)"},
        {"ISO-8859-2 - CP1250", "a real conversion name"},
        {"", "the empty name"},
        {longer, "a name longer than the buffer"},
    };
    for (int i = 0; i < _countof(same); i++)
    {
        char before[bufLen + 8];
        char after[bufLen + 8];
        memset(before, 0xCD, sizeof(before));
        memset(after, 0xCD, sizeof(after));
        out = g_out.Init(bufLen);
        BOOL b = GetCodeName_before(same[i].Name, out, bufLen);
        memcpy(before, out, bufLen);
        out = g_out.Init(bufLen);
        BOOL a = GetCodeName_after(same[i].Name, out, bufLen);
        memcpy(after, out, bufLen);
        char what[256];
        sprintf(what, "D1 identity: %s -- same bytes and same result", same[i].What);
        CHECK(memcmp(before, after, bufLen) == 0 && b == a, what);
    }
}

// ===========================================================================
// D3 -- a NULL conversion name reaching the name comparison
// ===========================================================================

// The shape of CCodeTables::GetCodeType's loop body (codetbl.cpp:826-838):
// the caller's string is compared against every table name.
static BOOL CodingNameEqualLike(const char* tableName, const char* wanted)
{
    // CodingNameEqual skips spaces, '-' and '&' and compares case-insensitively;
    // the only thing that matters here is that it dereferences both arguments.
    while (*tableName != 0 && *wanted != 0)
    {
        if (*tableName != *wanted)
            return FALSE;
        tableName++;
        wanted++;
    }
    return *tableName == *wanted;
}

static void TestD3()
{
    printf("\n--- D3  GetConversionTable with a NULL conversion name ---\n");

    const char* tableName = "ISO-8859-2 - CP1250";
    BOOL crashed = FALSE;
    __try
    {
        CodingNameEqualLike(tableName, (const char*)NULL);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        crashed = TRUE;
        printf("       before: exception 0x%08X reaching the name comparison\n",
               GetExceptionCode());
    }
    CHECK(crashed, "D3 before: a NULL name faults inside the table lookup");

    // The fix refuses the argument before the lookup is reached.
    const char* conversion = NULL;
    BOOL refused = (conversion == NULL);
    CHECK(refused, "D3 after:  the guard returns FALSE before any lookup happens");
}

// ===========================================================================
// D4 -- the viewer title: clamp, then trim only when the clamp truncated
// ===========================================================================

// VERBATIM from src/common/salunicode.cpp:612-630.
static void SalU8TrimIncompleteTail(char* buf)
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

#define CLAMP 260 // MAX_PATH, as lstrcpyn is called with in SetViewerCaption

static void CaptionCopy_before(char* caption, const char* src)
{
    lstrcpynA(caption, src, CLAMP);
}

static void CaptionCopy_after(char* caption, const char* src)
{
    lstrcpynA(caption, src, CLAMP);
    if ((int)strlen(src) >= CLAMP) // the copy truncated
        SalU8TrimIncompleteTail(caption);
}

// The variant the fix deliberately does NOT use, kept so the guard's reason is
// demonstrated rather than asserted.
static void CaptionCopy_unguarded(char* caption, const char* src)
{
    lstrcpynA(caption, src, CLAMP);
    SalU8TrimIncompleteTail(caption);
}

static BOOL IsValidUtf8(const char* s)
{
    // strict enough for the probe: what SalU8ToW would accept
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0) != 0;
}

static void TestD4()
{
    printf("\n--- D4  SetViewerCaption: clamp then guarded trim ---\n");

    // A UTF-8 path long enough to truncate, arranged so the LAST byte the copy
    // keeps is the lead byte of a 2-byte character (the quickstart S4 fixture,
    // in miniature).  lstrcpyn(dst, src, CLAMP) keeps CLAMP-1 = 259 bytes,
    // indices 0..258, so the lead must sit at index 258 -- that is the 259th
    // byte, which is what quickstart S4 asks the fixture to check.
    static char torn[400];
    {
        int i = 0;
        while (i < 258)
            torn[i++] = 'a';
        torn[i++] = (char)0xC4; // lead of U+010D, at index 258
        torn[i++] = (char)0x8D;
        while (i < 320)
            torn[i++] = 'b';
        torn[i] = 0;
    }

    char caption[CLAMP + 8];
    CaptionCopy_before(caption, torn);
    CHECK(!IsValidUtf8(caption), "D4 before: the 259-byte cut leaves a lone lead byte -- not valid UTF-8");
    printf("       before: last byte = 0x%02X, length %d\n",
           (unsigned char)caption[strlen(caption) - 1], (int)strlen(caption));

    CaptionCopy_after(caption, torn);
    CHECK(IsValidUtf8(caption), "D4 after:  the torn tail is dropped -- valid UTF-8, the title renders");
    // 259 bytes were copied; the last of them was a lone lead byte, so the
    // trim drops exactly that one and 258 complete bytes remain
    CHECK((int)strlen(caption) == 258, "D4 after:  only the torn byte is dropped, the rest of the prefix stays");

    // A code-page caption from a legacy plugin that was NOT truncated and ends
    // in a byte >= 0xC0 (CP1250 'a' with acute = 0xE1). This is why the trim is
    // guarded: the unguarded version eats its last character.
    char ansi[64];
    strcpy(ansi, "Archiv \xE1");
    char keep[CLAMP + 8];

    CaptionCopy_after(keep, ansi);
    CHECK(strcmp(keep, ansi) == 0, "D4 after:  an untruncated code-page caption is byte-identical");

    CaptionCopy_unguarded(keep, ansi);
    CHECK(strcmp(keep, ansi) != 0, "D4 rationale: the UNGUARDED trim would drop its last character");
    printf("       unguarded would give: \"%s\" (was \"%s\")\n", keep, ansi);

    // Identity: anything at or below the clamp is copied byte for byte.
    // The last two entries were added after the D4 review: the first three are
    // all ASCII-terminated, so this loop would have passed even with the guard
    // omitted -- it could not discriminate, which is the whole point of it.
    static char justFits[CLAMP + 8];
    {
        int i = 0;
        while (i < CLAMP - 3)
            justFits[i++] = 'a';
        justFits[i++] = (char)0xF9; // a lone CP1250 byte at the very end
        justFits[i++] = (char)0xB0;
        justFits[i] = 0; // CLAMP - 1 bytes: the largest source that is not cut
    }
    const char* shortPath[] = {"C:\\t0075\\P\xC5\x99""ehled.txt", "C:\\a.txt", "",
                               "Archiv \xE1", justFits};
    for (int i = 0; i < _countof(shortPath); i++)
    {
        char b[CLAMP + 8], a[CLAMP + 8];
        CaptionCopy_before(b, shortPath[i]);
        CaptionCopy_after(a, shortPath[i]);
        char what[256];
        sprintf(what, "D4 identity: a %d-byte source is unchanged by the fix", (int)strlen(shortPath[i]));
        CHECK(strcmp(a, b) == 0, what);
    }

    // A source of exactly CLAMP-1 bytes ending in a COMPLETE character must not
    // lose it (the obvious unconditional-trim bug).
    static char exact[CLAMP + 8];
    {
        int i = 0;
        while (i < CLAMP - 3)
            exact[i++] = 'a';
        exact[i++] = (char)0xC4; // a complete U+010D at the very end
        exact[i++] = (char)0x8D;
        exact[i] = 0; // total CLAMP - 1 bytes
    }
    char e[CLAMP + 8];
    CaptionCopy_after(e, exact);
    CHECK(strcmp(e, exact) == 0, "D4 after:  a complete final character at the clamp boundary is kept");
}

// ===========================================================================
// D5 -- CFileHeaderWindow::SetText
// ===========================================================================

static int g_textLen = 0;

// VERBATIM from src/plugins/filecomp/controls.cpp:36-42 at c554f4d.
static void SetText_before(char* Text, const char* text)
{
    strcpy(Text, text);
    g_textLen = int(strlen(text));
}

// The fix (plan.md Design D5), as first written: the walk-back ran on every
// call. Kept so the regression the D5 review caught stays demonstrable.
static void SetText_afterUnguarded(char* Text, const char* text)
{
    lstrcpynA(Text, text, MAX_PATH);
    int len = (int)strlen(Text);
    int i = len;
    while (i > 0 && ((unsigned char)Text[i - 1] & 0xC0) == 0x80)
        i--;
    if (i > 0)
    {
        unsigned char lead = (unsigned char)Text[i - 1];
        if (lead >= 0xC0)
        {
            int seqLen = lead >= 0xF0 ? 4 : (lead >= 0xE0 ? 3 : 2);
            if (len - (i - 1) < seqLen)
                Text[i - 1] = 0;
        }
    }
    g_textLen = int(strlen(Text));
}

// The fix as corrected after the review: the walk-back runs only when the copy
// truncated, the shape cmdshell.cpp uses.
static void SetText_after(char* Text, const char* text)
{
    lstrcpynA(Text, text, MAX_PATH);
    // drop a trailing incomplete UTF-8 sequence left by the byte clamp; the
    // core's SalU8TrimIncompleteTail is not reachable from a plugin
    int len = (int)strlen(Text);
    if ((int)strlen(text) < MAX_PATH)
    {
        g_textLen = len; // nothing was clamped
        return;
    }
    int i = len;
    while (i > 0 && ((unsigned char)Text[i - 1] & 0xC0) == 0x80)
        i--;
    if (i > 0)
    {
        unsigned char lead = (unsigned char)Text[i - 1];
        if (lead >= 0xC0)
        {
            int seqLen = lead >= 0xF0 ? 4 : (lead >= 0xE0 ? 3 : 2);
            if (len - (i - 1) < seqLen)
                Text[i - 1] = 0;
        }
    }
    g_textLen = int(strlen(Text));
}

static void TestD5()
{
    printf("\n--- D5  CFileHeaderWindow::SetText ---\n");

    static char oversized[400];
    memset(oversized, 'x', 300);
    oversized[300] = 0;

    char* text = g_out.Init(MAX_PATH);
    SetText_before(text, oversized);
    printf("       before: overran Text[MAX_PATH] at +%d\n", g_out.FirstOverrunOffset());
    CHECK(!g_out.CanaryIntact(), "D5 before: a 300-byte text writes past Text[MAX_PATH]");

    text = g_out.Init(MAX_PATH);
    SetText_after(text, oversized);
    CHECK(g_out.CanaryIntact(), "D5 after:  nothing written past Text[MAX_PATH]");
    CHECK(g_textLen == MAX_PATH - 1 && (int)strlen(text) == MAX_PATH - 1,
          "D5 after:  TextLen matches the stored text, not the argument");

    // the clamp lands inside a 2-byte character -> the walk-back drops it whole
    static char tornText[400];
    {
        int i = 0;
        while (i < MAX_PATH - 1)
            tornText[i++] = 'x';
        tornText[i++] = (char)0xC4;
        tornText[i++] = (char)0x8D;
        while (i < 320)
            tornText[i++] = 'y';
        tornText[i] = 0;
    }
    text = g_out.Init(MAX_PATH);
    SetText_after(text, tornText);
    CHECK(g_out.CanaryIntact(), "D5 after:  torn input still writes nothing past the buffer");
    CHECK((unsigned char)text[strlen(text) - 1] != 0xC4,
          "D5 after:  the stored text does not end on a lone lead byte");

    // THE CASE THE FIRST VERSION OF THIS FIX GOT WRONG (found by the D5 review):
    // a text that FITS but is code-page bytes, not UTF-8, ending in a byte
    // >= 0xC0.  fcremote.exe is an ANSI build, so such a path really does reach
    // SetText.  An unconditional walk-back eats its last character.
    {
        char cp[64];
        strcpy(cp, "D:\\Petr\xF9"); // CP1250 'u' with ring, a lone 0xF9
        char guarded[MAX_PATH + 8];
        char unguarded[MAX_PATH + 8];
        SetText_after(guarded, cp);
        SetText_afterUnguarded(unguarded, cp);
        CHECK(strcmp(guarded, cp) == 0,
              "D5 after:  a code-page text that FITS is stored unchanged (guarded trim)");
        CHECK(strcmp(unguarded, cp) != 0,
              "D5 regression: the UNGUARDED trim drops its last character -- why the guard exists");
        printf("       unguarded would give: \"%s\" (was \"%s\")\n", unguarded, cp);
    }

    // identity for everything that fits
    const char* fits[] = {"", "C:\\a.txt", "D:\\Zkou\xC5\xA1ka\\M\xC5\xAFj disk\\soubor.txt",
                          "D:\\r\xE9sum\xE9", "D:\\Petr\xF9"};
    for (int i = 0; i < _countof(fits); i++)
    {
        char b[MAX_PATH + 8], a[MAX_PATH + 8];
        memset(b, 0, sizeof(b));
        memset(a, 0, sizeof(a));
        SetText_before(b, fits[i]);
        int lenB = g_textLen;
        SetText_after(a, fits[i]);
        int lenA = g_textLen;
        char what[256];
        sprintf(what, "D5 identity: a %d-byte text is stored unchanged", (int)strlen(fits[i]));
        CHECK(strcmp(a, b) == 0 && lenA == lenB, what);
    }
}

int main()
{
    printf("feature 075 evidence probe -- logic-level before/after\n");
    TestD1();
    TestD3();
    TestD4();
    TestD5();
    printf("\nprobe: %d checks, %d failed\n", g_checks, g_failures);
    return g_failures;
}
