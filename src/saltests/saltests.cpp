// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later

// Unit tests for the 004-long-paths-unicode foundation helpers
// (src/common/salunicode.cpp, src/common/salpath.cpp).
// Console exe; exit code = number of failed checks.

#include "precomp.h"

#include <math.h>

#include "salunicode.h"
#include "salpath.h"
#include "salfileio.h"
#include "salclip.h"
#include "themes_palette.h"

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond) \
    do \
    { \
        g_checks++; \
        if (!(cond)) \
        { \
            g_failures++; \
            printf("FAIL %s(%d): %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

// UTF-8 byte sequences used below:
//   NFC c-caron (U+010D)          = C4 8D
//   NFD c + combining caron       = 63 CC 8C
//   NFC C-caron (U+010C)          = C4 8C
//   folder emoji (U+1F4C1)        = F0 9F 93 81
#define U8_C_CARON_NFC "\xC4\x8D"
#define U8_C_CARON_NFD "c\xCC\x8C"
#define U8_CAP_C_CARON_NFC "\xC4\x8C"
#define U8_FOLDER_EMOJI "\xF0\x9F\x93\x81"

static void TestConversions()
{
    // NFC round trip
    WCHAR* w = SalU8ToWAlloc(U8_C_CARON_NFC);
    CHECK(w != NULL && wcscmp(w, L"\x010D") == 0);
    free(w);

    // NFD is preserved exactly (no silent normalization)
    w = SalU8ToWAlloc(U8_C_CARON_NFD);
    CHECK(w != NULL && wcscmp(w, L"c\x030C") == 0);
    char* u8 = SalWToU8Alloc(w);
    CHECK(u8 != NULL && strcmp(u8, U8_C_CARON_NFD) == 0);
    free(u8);
    free(w);

    // non-BMP round trip (surrogate pair)
    w = SalU8ToWAlloc(U8_FOLDER_EMOJI);
    CHECK(w != NULL && wcscmp(w, L"\xD83D\xDCC1") == 0);
    u8 = SalWToU8Alloc(w);
    CHECK(u8 != NULL && strcmp(u8, U8_FOLDER_EMOJI) == 0);
    free(u8);
    free(w);

    // invalid UTF-8 fails instead of being replaced
    CHECK(SalU8ToWAlloc("\xC4") == NULL);
    CHECK(SalU8ToWAlloc("\xFF\xFE") == NULL);

    // unpaired surrogate travels as WTF-8 (feature 066): ED A0 BD for U+D83D
    u8 = SalWToU8Alloc(L"\xD83D");
    CHECK(u8 != NULL && strcmp(u8, "\xED\xA0\xBD") == 0);
    free(u8);

    // sized (non-null-terminated) inputs get terminated output
    WCHAR wbuf[8];
    CHECK(SalU8ToW("abcdef", 3, wbuf, 8) == 4 && wcscmp(wbuf, L"abc") == 0);
    char cbuf[8];
    CHECK(SalWToU8(L"abcdef", 3, cbuf, 8) == 4 && strcmp(cbuf, "abc") == 0);
    // exact-fit failure is detected (no silent truncation)
    CHECK(SalU8ToW("abcd", 4, wbuf, 4) == 0);

    // lossless ACP conversion: ASCII passes, emoji cannot
    char acp[16];
    CHECK(SalWToACPLossless(L"abc", -1, acp, sizeof(acp)) && strcmp(acp, "abc") == 0);
    CHECK(!SalWToACPLossless(L"\xD83D\xDCC1", -1, acp, sizeof(acp)));
}

static void TestNormalization()
{
    // NFD -> NFC composition
    WCHAR buf[8];
    CHECK(SalNormalizeNFC(L"c\x030C", -1, buf, 8) > 0 && wcscmp(buf, L"\x010D") == 0);
    // NFC input is idempotent
    CHECK(SalNormalizeNFC(L"\x010D", -1, buf, 8) > 0 && wcscmp(buf, L"\x010D") == 0);
    // ASCII passthrough
    CHECK(SalNormalizeNFC(L"abc", -1, buf, 8) > 0 && wcscmp(buf, L"abc") == 0);
    WCHAR* nfc = SalNormalizeNFCAlloc(L"c\x030C"
                                      L".txt");
    CHECK(nfc != NULL && wcscmp(nfc, L"\x010D.txt") == 0);
    free(nfc);
}

static void TestMatching()
{
    CHECK(SalIsASCII("plain.txt"));
    CHECK(!SalIsASCII(U8_C_CARON_NFC ".txt"));

    // UTF-8 character walking/counting (feature 063: list padding, tooltip clamp)
    CHECK(SalU8CharCount("abc") == 3);
    CHECK(SalU8CharCount("") == 0);
    CHECK(SalU8CharCount(U8_C_CARON_NFC "a" U8_FOLDER_EMOJI) == 3); // 2+1+4 bytes, 3 chars
    CHECK(SalU8CharCount(U8_C_CARON_NFC "a", 2) == 1);              // sized: first char only
    const char* walk = U8_C_CARON_NFC "a";
    walk = SalU8Next(walk);
    CHECK(strcmp(walk, "a") == 0); // stepped over the 2-byte character
    walk = SalU8Next(walk);
    CHECK(*walk == 0);
    CHECK(SalU8Next(walk) == walk); // identity on the terminator

    // canonical equivalence (case-sensitive)
    CHECK(SalNameEquivalent(U8_C_CARON_NFC ".txt", U8_C_CARON_NFD ".txt"));
    CHECK(SalNameEquivalent("same.txt", "same.txt"));
    CHECK(!SalNameEquivalent("a.txt", "b.txt"));
    CHECK(!SalNameEquivalent(U8_CAP_C_CARON_NFC ".txt", U8_C_CARON_NFD ".txt")); // differs in case

    // case-insensitive, form-insensitive equality (FR-008)
    CHECK(SalNameEqualCI(U8_CAP_C_CARON_NFC ".TXT", -1, U8_C_CARON_NFD ".txt", -1));
    CHECK(SalNameEqualCI("ABC", -1, "abc", -1));
    CHECK(!SalNameEqualCI("abc", -1, "abd", -1));
    CHECK(SalNameEqualCI("abc", 2, "ab", -1)); // explicit lengths

    // collation: equivalent forms compare equal, order is sign-correct
    CHECK(SalCompareNamesUTF8(U8_C_CARON_NFC, -1, U8_C_CARON_NFD, -1, FALSE) == 0);
    CHECK(SalCompareNamesUTF8("a", -1, "b", -1, FALSE) < 0);
    CHECK(SalCompareNamesUTF8("b", -1, "a", -1, FALSE) > 0);
    CHECK(SalCompareNamesUTF8("A", -1, "a", -1, TRUE) == 0);
}

static void TestPathBuf()
{
    CSalPathBuf p;
    CHECK(p.IsEmpty() && p.Length() == 0 && strcmp(p.Get(), "") == 0);

    CHECK(p.Set("C:\\dir"));
    CHECK(p.AppendComponent("sub"));
    CHECK(strcmp(p.Get(), "C:\\dir\\sub") == 0);
    CHECK(p.AppendComponent("\\slashed")); // leading separators are eaten
    CHECK(strcmp(p.Get(), "C:\\dir\\sub\\slashed") == 0);

    CHECK(p.AddBackslash() && p.AddBackslash()); // idempotent
    CHECK(strcmp(p.Get(), "C:\\dir\\sub\\slashed\\") == 0);
    p.StripBackslash();
    CHECK(strcmp(p.Get(), "C:\\dir\\sub\\slashed") == 0);

    CHECK(p.CutLastComponent() && strcmp(p.Get(), "C:\\dir\\sub") == 0);
    CHECK(p.CutLastComponent() && strcmp(p.Get(), "C:\\dir") == 0);
    CHECK(p.CutLastComponent() && strcmp(p.Get(), "C:\\") == 0);
    CHECK(!p.CutLastComponent()); // at root
    p.StripBackslash();
    CHECK(strcmp(p.Get(), "C:\\") == 0); // drive root keeps its backslash

    // UNC root protection
    CHECK(p.Set("\\\\server\\share\\dir"));
    CHECK(p.CutLastComponent() && strcmp(p.Get(), "\\\\server\\share") == 0);
    CHECK(!p.CutLastComponent()); // share is part of the root

    // growth far beyond MAX_PATH
    CHECK(p.Set("C:\\"));
    for (int i = 0; i < 200; i++)
        CHECK(p.AppendComponent("component"));
    CHECK(p.Length() > 2000);
    CHECK(p.Get()[p.Length()] == 0);

    // copy semantics
    CSalPathBuf q(p);
    CHECK(q.Length() == p.Length() && strcmp(q.Get(), p.Get()) == 0);
    CSalPathBuf r;
    r = p;
    CHECK(r.Length() == p.Length() && strcmp(r.Get(), p.Get()) == 0);
}

static void TestExtendedPaths()
{
    WCHAR* w = SalPathToWExtAlloc("C:\\dir\\file.txt");
    CHECK(w != NULL && wcscmp(w, L"\\\\?\\C:\\dir\\file.txt") == 0);
    free(w);

    // dot segments collapse, forward slashes convert
    w = SalPathToWExtAlloc("C:\\a\\b\\..\\c\\.\\d");
    CHECK(w != NULL && wcscmp(w, L"\\\\?\\C:\\a\\c\\d") == 0);
    free(w);
    w = SalPathToWExtAlloc("C:/fwd/slash");
    CHECK(w != NULL && wcscmp(w, L"\\\\?\\C:\\fwd\\slash") == 0);
    free(w);

    // UNC form
    w = SalPathToWExtAlloc("\\\\server\\share\\file");
    CHECK(w != NULL && wcscmp(w, L"\\\\?\\UNC\\server\\share\\file") == 0);
    free(w);

    // drive root
    w = SalPathToWExtAlloc("C:\\");
    CHECK(w != NULL && wcscmp(w, L"\\\\?\\C:\\") == 0);
    free(w);

    // Unicode content flows through
    w = SalPathToWExtAlloc("C:\\" U8_C_CARON_NFD "\\" U8_FOLDER_EMOJI ".txt");
    CHECK(w != NULL && wcscmp(w, L"\\\\?\\C:\\c\x030C\\\xD83D\xDCC1.txt") == 0);
    free(w);

    // climbing above the root fails
    CHECK(SalPathToWExtAlloc("C:\\a\\..\\..") == NULL);

    // feature 027 pre-scan: clean paths (skip branch) and the dirty forms it
    // must still route through canonicalization produce identical output
    w = SalPathToWExtAlloc("C:\\already\\clean\\path"); // clean -> skip branch
    CHECK(w != NULL && wcscmp(w, L"\\\\?\\C:\\already\\clean\\path") == 0);
    free(w);
    w = SalPathToWExtAlloc("C:\\trailing\\"); // trailing separator must be stripped
    CHECK(w != NULL && wcscmp(w, L"\\\\?\\C:\\trailing") == 0);
    free(w);
    w = SalPathToWExtAlloc("C:\\double\\\\sep"); // doubled separator must collapse
    CHECK(w != NULL && wcscmp(w, L"\\\\?\\C:\\double\\sep") == 0);
    free(w);
    w = SalPathToWExtAlloc("C:\\a\\.\\b"); // single-dot component must drop
    CHECK(w != NULL && wcscmp(w, L"\\\\?\\C:\\a\\b") == 0);
    free(w);
    w = SalPathToWExtAlloc("C:\\dotted.name\\file..ext"); // dots inside names are NOT components -> clean
    CHECK(w != NULL && wcscmp(w, L"\\\\?\\C:\\dotted.name\\file..ext") == 0);
    free(w);

    // already-extended input passes through
    w = SalPathToWExtAlloc("\\\\?\\C:\\x");
    CHECK(w != NULL && wcscmp(w, L"\\\\?\\C:\\x") == 0);
    free(w);

    // a long (>260) path is accepted, an absurd one (>32767) is rejected
    CSalPathBuf lp;
    CHECK(lp.Set("C:\\"));
    for (int i = 0; i < 60; i++)
        CHECK(lp.AppendComponent("component-eighteen"));
    CHECK(lp.Length() > 1000);
    w = SalPathToWExtAlloc(lp.Get());
    CHECK(w != NULL && wcsncmp(w, L"\\\\?\\C:\\", 7) == 0 && wcslen(w) > 1000);
    free(w);
    for (int i = 0; i < 1800; i++)
        lp.AppendComponent("component-eighteen");
    CHECK(SalPathToWExtAlloc(lp.Get()) == NULL);

    // relative input resolves against the current directory
    w = SalPathToWExtAlloc("relative.txt");
    CHECK(w != NULL && wcsncmp(w, L"\\\\?\\", 4) == 0 && wcsstr(w, L"relative.txt") != NULL);
    free(w);

    // display-form round trip strips the prefix
    char* u8 = SalPathFromWAlloc(L"\\\\?\\C:\\dir\\x");
    CHECK(u8 != NULL && strcmp(u8, "C:\\dir\\x") == 0);
    free(u8);
    u8 = SalPathFromWAlloc(L"\\\\?\\UNC\\server\\share\\x");
    CHECK(u8 != NULL && strcmp(u8, "\\\\server\\share\\x") == 0);
    free(u8);
}

// end-to-end: create, enumerate, rename and delete files at a path
// deeper than the legacy 260-char limit and with an NFD Unicode name
static void TestFileIO()
{
    char tmp[MAX_PATH];
    DWORD n = GetTempPathA(sizeof(tmp), tmp);
    if (n == 0 || n >= sizeof(tmp))
    {
        printf("skipping TestFileIO (no temp path)\n");
        return;
    }

    CSalPathBuf base;
    CHECK(base.Set(tmp));
    CHECK(base.AppendComponent("saltests-deep"));
    CHECK(SalCreateDirectory(base.Get(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    CSalPathBuf dir(base);
    while (dir.Length() < 300) // push well past MAX_PATH
    {
        CHECK(dir.AppendComponent("component-eighteen"));
        CHECK(SalCreateDirectory(dir.Get(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    }
    CHECK(dir.Length() > 300);

    // file with an NFD name at the deep path
    CSalPathBuf file(dir);
    CHECK(file.AppendComponent(U8_C_CARON_NFD "-deep.txt"));
    HANDLE h = SalCreateFile(file.Get(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD written;
        CHECK(WriteFile(h, "data", 4, &written, NULL) && written == 4);
        CloseHandle(h);
    }

    // attributes work at depth
    CHECK(SalGetFileAttributes(file.Get()) != INVALID_FILE_ATTRIBUTES);
    WIN32_FILE_ATTRIBUTE_DATA fad;
    CHECK(SalGetFileAttributesEx(file.Get(), &fad) && fad.nFileSizeLow == 4);

    // enumeration returns the exact NFD name (no normalization)
    CSalPathBuf pattern(dir);
    CHECK(pattern.AppendComponent("*"));
    WIN32_FIND_DATAW fd;
    HANDLE find = SalFindFirstFile(pattern.Get(), &fd);
    CHECK(find != INVALID_HANDLE_VALUE);
    BOOL seen = FALSE;
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wcscmp(fd.cFileName, L"c\x030C-deep.txt") == 0)
                seen = TRUE;
        } while (SalFindNextFile(find, &fd));
        FindClose(find);
    }
    CHECK(seen);

    // rename + copy + delete at depth
    CSalPathBuf file2(dir);
    CHECK(file2.AppendComponent("renamed-" U8_C_CARON_NFC ".txt"));
    CHECK(SalMoveFile(file.Get(), file2.Get()));
    CSalPathBuf file3(dir);
    CHECK(file3.AppendComponent("copy.txt"));
    CHECK(SalCopyFile(file2.Get(), file3.Get(), TRUE));
    CHECK(SalDeleteFile(file2.Get()));
    CHECK(SalDeleteFile(file3.Get()));

    // tear down the deep tree
    while (dir.Length() > base.Length())
    {
        CHECK(SalRemoveDirectory(dir.Get()));
        CHECK(dir.CutLastComponent());
    }
    CHECK(SalRemoveDirectory(base.Get()));
}

// UTF-8 "ěščř" (2 bytes per char)
#define U8_ESCR "\xC4\x9B\xC5\xA1\xC4\x8D\xC5\x99"

// ---------------------------------------------------------------------------
// Feature 031: byte-length invariants of legal-length name components.
// The defect class: a component's CHARACTER count is legal (<= 255) but its
// UTF-8 BYTE length exceeds legacy MAX_PATH-sized buffers. The reported crash
// was a 215-char Czech-diacritics directory name = 330 UTF-8 bytes smashing
// a char[MAX_PATH + 4] in the panel paint path.

// the user's exact repro-name unit (43 chars):
// "ýášřtščýáíf buaweýáh čáíhšáífšfhčíáéfšh dnf"
static const WCHAR REPRO_UNIT_W[] =
    L"\x00FD\x00E1\x0161\x0159t\x0161\x010D\x00FD\x00E1\x00ED"
    L"f "
    L"buawe\x00FD\x00E1h "
    L"\x010D\x00E1\x00EDh\x0161\x00E1\x00ED"
    L"f\x0161"
    L"fh\x010D\x00ED\x00E1\x00E9"
    L"f\x0161h dnf";

// builds the full 215-char repro name (5x the unit) into 'w' (>= 216 WCHARs)
static void BuildReproNameW(WCHAR* w)
{
    w[0] = 0;
    for (int i = 0; i < 5; i++)
        wcscat(w, REPRO_UNIT_W);
}

static void TestLongComponentNames()
{
    // the repro-name unit is exactly 43 chars, the full name 215 chars
    CHECK(wcslen(REPRO_UNIT_W) == 43);
    WCHAR reproW[256];
    BuildReproNameW(reproW);
    CHECK(wcslen(reproW) == 215);

    // 215 diacritics chars -> 330 UTF-8 bytes: legal component length whose
    // byte length exceeds the legacy MAX_PATH+4 buffers (the defect class),
    // yet fits the established SAL_FIND_NAME_U8 bound with the DWORD
    // terminator used by the paint path
    char* u8 = SalWToU8Alloc(reproW);
    CHECK(u8 != NULL);
    if (u8 != NULL)
    {
        size_t len = strlen(u8);
        CHECK(len == 330);
        CHECK(len > MAX_PATH + 4);              // overflows the pre-031 buffers
        CHECK(len + 4 <= SAL_FIND_NAME_U8 + 4); // fits the 031 buffers incl. DWORD terminator
        WCHAR* back = SalU8ToWAlloc(u8);        // byte-exact round trip
        CHECK(back != NULL && wcscmp(back, reproW) == 0);
        free(back);
        free(u8);
    }

    // worst case: 255 x U+4E2D (3-byte UTF-8) = 765 bytes; DWORD-terminated
    // copies need 769 bytes and must fit SAL_FIND_NAME_U8 + 4
    WCHAR w255[256];
    for (int i = 0; i < 255; i++)
        w255[i] = 0x4E2D;
    w255[255] = 0;
    u8 = SalWToU8Alloc(w255);
    CHECK(u8 != NULL);
    if (u8 != NULL)
    {
        CHECK(strlen(u8) == 3 * 255);
        CHECK(strlen(u8) + 4 <= SAL_FIND_NAME_U8 + 4);
        free(u8);
    }

    // 255 UTF-16 units of surrogate pairs (127 pairs = 254 units): 4 UTF-8
    // bytes per pair -> 508 bytes, inside the same bound
    WCHAR wsurr[256];
    for (int i = 0; i < 127; i++)
    {
        wsurr[2 * i] = 0xD83D;     // U+1F4C1 high surrogate
        wsurr[2 * i + 1] = 0xDCC1; // U+1F4C1 low surrogate
    }
    wsurr[254] = 0;
    u8 = SalWToU8Alloc(wsurr);
    CHECK(u8 != NULL);
    if (u8 != NULL)
    {
        CHECK(strlen(u8) == 4 * 127);
        CHECK(strlen(u8) + 4 <= SAL_FIND_NAME_U8 + 4);
        free(u8);
    }

    // SalConvertFindDataW: a maximum-length component converts completely and
    // round-trips byte-exactly into the enumeration-sized buffer
    WIN32_FIND_DATAW fdw;
    memset(&fdw, 0, sizeof(fdw));
    wcscpy(fdw.cFileName, w255); // 255 chars, the OS component maximum
    char nameU8[SAL_FIND_NAME_U8];
    char dosNameU8[3 * 14 + 2];
    SalConvertFindDataW(&fdw, NULL, nameU8, sizeof(nameU8), dosNameU8, sizeof(dosNameU8));
    CHECK(strlen(nameU8) == 3 * 255);
    WCHAR* back = SalU8ToWAlloc(nameU8);
    CHECK(back != NULL && wcscmp(back, w255) == 0);
    free(back);
    CHECK(dosNameU8[0] == 0); // empty alternate name stays empty

    // the repro name converts through the same route
    wcscpy(fdw.cFileName, reproW);
    SalConvertFindDataW(&fdw, NULL, nameU8, sizeof(nameU8), NULL, 0);
    CHECK(strlen(nameU8) == 330);

    // fail-safe: a too-small target yields an EMPTY string -- never a
    // silently truncated name that could act as a different identity
    char tooSmall[64];
    SalConvertFindDataW(&fdw, NULL, tooSmall, sizeof(tooSmall), NULL, 0);
    CHECK(tooSmall[0] == 0);

    // on-disk: create the exact repro directory name, enumerate its parent,
    // and require the byte-exact 330-byte name back (the crash scenario data)
    char tmp[MAX_PATH];
    DWORD n = GetTempPathA(sizeof(tmp), tmp);
    if (n == 0 || n >= sizeof(tmp))
    {
        printf("skipping TestLongComponentNames disk part (no temp path)\n");
        return;
    }
    CSalPathBuf base;
    CHECK(base.Set(tmp));
    CHECK(base.AppendComponent("saltests-deep"));
    CHECK(SalCreateDirectory(base.Get(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    char* reproU8 = SalWToU8Alloc(reproW);
    CHECK(reproU8 != NULL);
    if (reproU8 != NULL)
    {
        CSalPathBuf dir(base);
        CHECK(dir.AppendComponent(reproU8));
        CHECK(SalCreateDirectory(dir.Get(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS);

        CSalPathBuf pattern(base);
        CHECK(pattern.AppendComponent("*"));
        WIN32_FIND_DATAW fd;
        HANDLE find = SalFindFirstFile(pattern.Get(), &fd);
        CHECK(find != INVALID_HANDLE_VALUE);
        BOOL seen = FALSE;
        if (find != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (wcscmp(fd.cFileName, reproW) == 0)
                {
                    seen = TRUE;
                    char foundU8[SAL_FIND_NAME_U8];
                    SalConvertFindDataW(&fd, NULL, foundU8, sizeof(foundU8), NULL, 0);
                    CHECK(strlen(foundU8) == 330);
                    CHECK(strcmp(foundU8, reproU8) == 0);
                }
            } while (SalFindNextFile(find, &fd));
            FindClose(find);
        }
        CHECK(seen);

        CHECK(SalRemoveDirectory(dir.Get()));
        free(reproU8);
    }
    CHECK(SalRemoveDirectory(base.Get()));
}

static void TestDropFiles()
{
    // --- build a wide CF_HDROP block from two >MAX_PATH Czech-diacritics paths
    char longA[600];
    char longB[600];
    strcpy(longA, "C:\\salamander-test\\" U8_ESCR);
    while (strlen(longA) < 560)
        strcat(longA, "\\dir-" U8_ESCR);
    strcpy(longB, longA);
    strcat(longB, "\\soubor-" U8_ESCR ".txt");
    const char* paths[2] = {longA, longB};

    HGLOBAL h = SalBuildWideDropFiles(paths, 2);
    CHECK(h != NULL);
    if (h != NULL)
    {
        SIZE_T size = GlobalSize(h);
        DROPFILES* df = (DROPFILES*)GlobalLock(h);
        CHECK(df != NULL);
        if (df != NULL)
        {
            CHECK(df->fWide);
            CHECK(df->pFiles == sizeof(DROPFILES));

            // scan reports both paths and the exact longest length
            WCHAR* wideA = SalU8ToWAlloc(longA);
            WCHAR* wideB = SalU8ToWAlloc(longB);
            CHECK(wideA != NULL && wideB != NULL);
            int longest = 0;
            CHECK(SalScanDropFiles(df, size, &longest) == 2);
            if (wideA != NULL && wideB != NULL)
            {
                CHECK(longest == (int)wcslen(wideB));
                CHECK((int)wcslen(wideB) > MAX_PATH); // the scenario actually exceeds the legacy limit

                // content round-trip: both wide strings are stored verbatim
                const WCHAR* s = (const WCHAR*)((const BYTE*)df + df->pFiles);
                CHECK(wcscmp(s, wideA) == 0);
                s += wcslen(s) + 1;
                CHECK(wcscmp(s, wideB) == 0);
                s += wcslen(s) + 1;
                CHECK(*s == 0); // double-NUL terminated

                // malformed blocks are rejected, never over-read (exact content
                // size -- GlobalSize may round the allocation up)
                SIZE_T exactSize = sizeof(DROPFILES) +
                                   (wcslen(wideA) + 1 + wcslen(wideB) + 1 + 1) * sizeof(WCHAR);
                CHECK(SalScanDropFiles(df, sizeof(DROPFILES) - 1, NULL) == -1);         // truncated header
                CHECK(SalScanDropFiles(df, exactSize - 2 * sizeof(WCHAR), NULL) == -1); // missing double-NUL
            }
            free(wideA);
            free(wideB);

            GlobalUnlock(h);
        }
        GlobalFree(h);
    }

    // --- ANSI (fWide=0) blocks are scanned too (foreign legacy producers)
    {
        const char list[] = "C:\\aa\0C:\\bbb\0";
        BYTE block[sizeof(DROPFILES) + sizeof(list)];
        memset(block, 0, sizeof(block));
        DROPFILES* df = (DROPFILES*)block;
        df->pFiles = sizeof(DROPFILES);
        df->fWide = FALSE;
        memcpy(block + sizeof(DROPFILES), list, sizeof(list));
        int longest = 0;
        CHECK(SalScanDropFiles(df, sizeof(block), &longest) == 2);
        CHECK(longest == 6); // "C:\bbb"
    }

    // --- degenerate inputs
    CHECK(SalBuildWideDropFiles(NULL, 1) == NULL);
    CHECK(SalBuildWideDropFiles(paths, 0) == NULL);
    const char* invalid[1] = {"\xC4"}; // invalid UTF-8: caller must fall back to the legacy route
    CHECK(SalBuildWideDropFiles(invalid, 1) == NULL);
    CHECK(SalScanDropFiles(NULL, 1000, NULL) == -1);
}

// ---------------------------------------------------------------------------
// Feature 028: Dark theme palette tests (src/common/themes_palette.h)
// WCAG 2.x contrast: standard text >= 4.5:1, disabled/secondary >= 3:1 (SC-005)

static double SrgbChannel(int c)
{
    double s = c / 255.0;
    return s <= 0.03928 ? s / 12.92 : pow((s + 0.055) / 1.055, 2.4);
}

static double Luminance(COLORREF c)
{
    return 0.2126 * SrgbChannel(GetRValue(c)) +
           0.7152 * SrgbChannel(GetGValue(c)) +
           0.0722 * SrgbChannel(GetBValue(c));
}

static double ContrastRatio(COLORREF a, COLORREF b)
{
    double la = Luminance(a) + 0.05;
    double lb = Luminance(b) + 0.05;
    return la > lb ? la / lb : lb / la;
}

// positional views of the palette data (order = list order in the header)
enum DarkPanelIdx
{
#define TP_ENUM(name, r, g, b) DP_##name,
    THEME_DARK_PANEL_COLORS(TP_ENUM)
#undef TP_ENUM
        DP_COUNT
};

enum DarkViewerIdx
{
#define TV_ENUM(name, r, g, b) DV_##name,
    THEME_DARK_VIEWER_COLORS(TV_ENUM)
#undef TV_ENUM
        DV_COUNT
};

static void TestDarkThemePalette()
{
    // --- chrome palette: build the LUT the app uses
    COLORREF chrome[64];
    BOOL chromeSet[64] = {0};
    for (int i = 0; i < 64; i++)
        chrome[i] = 0;
#define TC_FILL(idx, r, g, b) \
    chrome[idx] = RGB(r, g, b); \
    chromeSet[idx] = TRUE;
    THEME_DARK_SYSCOLORS(TC_FILL)
#undef TC_FILL

    // every COLOR_* index the application draws with must be mapped
    // (COLOR_3DFACE==COLOR_BTNFACE and COLOR_3DSHADOW==COLOR_BTNSHADOW share values)
    const int drawnIndexes[] = {
        COLOR_WINDOW, COLOR_WINDOWTEXT, COLOR_WINDOWFRAME, COLOR_BTNFACE,
        COLOR_BTNTEXT, COLOR_BTNSHADOW, COLOR_BTNHIGHLIGHT, COLOR_3DLIGHT,
        COLOR_3DDKSHADOW, COLOR_HIGHLIGHT, COLOR_HIGHLIGHTTEXT, COLOR_GRAYTEXT,
        COLOR_HOTLIGHT, COLOR_INFOTEXT, COLOR_INFOBK, COLOR_CAPTIONTEXT,
        COLOR_ACTIVECAPTION, COLOR_INACTIVECAPTION, COLOR_INACTIVECAPTIONTEXT,
        COLOR_SCROLLBAR, COLOR_MENU, COLOR_MENUTEXT, COLOR_3DFACE, COLOR_3DSHADOW};
    for (int i = 0; i < (int)(sizeof(drawnIndexes) / sizeof(drawnIndexes[0])); i++)
        CHECK(chromeSet[drawnIndexes[i]]);

    // chrome text/background pairs (>= 4.5:1; disabled text >= 3:1)
    CHECK(ContrastRatio(chrome[COLOR_WINDOWTEXT], chrome[COLOR_WINDOW]) >= 4.5);
    CHECK(ContrastRatio(chrome[COLOR_BTNTEXT], chrome[COLOR_BTNFACE]) >= 4.5);
    CHECK(ContrastRatio(chrome[COLOR_MENUTEXT], chrome[COLOR_MENU]) >= 4.5);
    CHECK(ContrastRatio(chrome[COLOR_HIGHLIGHTTEXT], chrome[COLOR_HIGHLIGHT]) >= 4.5);
    CHECK(ContrastRatio(chrome[COLOR_INFOTEXT], chrome[COLOR_INFOBK]) >= 4.5);
    CHECK(ContrastRatio(chrome[COLOR_CAPTIONTEXT], chrome[COLOR_ACTIVECAPTION]) >= 4.5);
    CHECK(ContrastRatio(chrome[COLOR_INACTIVECAPTIONTEXT], chrome[COLOR_INACTIVECAPTION]) >= 4.5);
    CHECK(ContrastRatio(chrome[COLOR_HOTLIGHT], chrome[COLOR_WINDOW]) >= 4.5);
    CHECK(ContrastRatio(chrome[COLOR_HOTLIGHT], chrome[COLOR_BTNFACE]) >= 4.5);
    CHECK(ContrastRatio(chrome[COLOR_GRAYTEXT], chrome[COLOR_BTNFACE]) >= 3.0);
    CHECK(ContrastRatio(chrome[COLOR_GRAYTEXT], chrome[COLOR_WINDOW]) >= 3.0);

    // feature 049: input/content surfaces sit LIGHTER than the dialog face
    // (Windows 11 dark convention; kills the "black hole" field look)
    CHECK(Luminance(chrome[COLOR_WINDOW]) > Luminance(chrome[COLOR_BTNFACE]));

    // feature 049: the hyperlink color must stay readable on the About
    // dialog's branded navy background (TC_COLOR_NAVY in src/logo.cpp)
    CHECK(ContrastRatio(chrome[COLOR_HOTLIGHT], RGB(0x0A, 0x14, 0x24)) >= 4.5);

    // --- panel palette: exact index count (positional integrity vs consts.h
    // is additionally static_assert-ed inside the application build)
    CHECK(DP_COUNT == 34);
    CHECK(DV_COUNT == 4);

    COLORREF panel[DP_COUNT];
#define TP_FILL(name, r, g, b) panel[DP_##name] = RGB(r, g, b);
    THEME_DARK_PANEL_COLORS(TP_FILL)
#undef TP_FILL

    // panel item text over its backgrounds (all item states, SC-005)
    CHECK(ContrastRatio(panel[DP_ITEM_FG_NORMAL], panel[DP_ITEM_BK_NORMAL]) >= 4.5);
    CHECK(ContrastRatio(panel[DP_ITEM_FG_SELECTED], panel[DP_ITEM_BK_SELECTED]) >= 4.5);
    CHECK(ContrastRatio(panel[DP_ITEM_FG_FOCUSED], panel[DP_ITEM_BK_FOCUSED]) >= 4.5);
    CHECK(ContrastRatio(panel[DP_ITEM_FG_FOCSEL], panel[DP_ITEM_BK_FOCSEL]) >= 4.5);
    CHECK(ContrastRatio(panel[DP_ITEM_FG_HIGHLIGHT], panel[DP_ITEM_BK_HIGHLIGHT]) >= 4.5);
    CHECK(ContrastRatio(panel[DP_HOT_PANEL], panel[DP_ITEM_BK_NORMAL]) >= 4.5);
    CHECK(ContrastRatio(panel[DP_ACTIVE_CAPTION_FG], panel[DP_ACTIVE_CAPTION_BK]) >= 4.5);
    CHECK(ContrastRatio(panel[DP_INACTIVE_CAPTION_FG], panel[DP_INACTIVE_CAPTION_BK]) >= 4.5);
    CHECK(ContrastRatio(panel[DP_HOT_ACTIVE], panel[DP_ACTIVE_CAPTION_BK]) >= 4.5);
    CHECK(ContrastRatio(panel[DP_HOT_INACTIVE], panel[DP_INACTIVE_CAPTION_BK]) >= 4.5);
    CHECK(ContrastRatio(panel[DP_PROGRESS_FG_NORMAL], panel[DP_PROGRESS_BK_NORMAL]) >= 4.5);
    CHECK(ContrastRatio(panel[DP_PROGRESS_FG_SELECTED], panel[DP_PROGRESS_BK_SELECTED]) >= 4.5);

    COLORREF viewer[DV_COUNT];
#define TV_FILL(name, r, g, b) viewer[DV_##name] = RGB(r, g, b);
    THEME_DARK_VIEWER_COLORS(TV_FILL)
#undef TV_FILL
    CHECK(ContrastRatio(viewer[DV_VIEWER_FG_NORMAL], viewer[DV_VIEWER_BK_NORMAL]) >= 4.5);
    CHECK(ContrastRatio(viewer[DV_VIEWER_FG_SELECTED], viewer[DV_VIEWER_BK_SELECTED]) >= 4.5);

    // all surfaces are truly dark (backgrounds darker than mid-gray)
    CHECK(Luminance(chrome[COLOR_WINDOW]) < 0.1);
    CHECK(Luminance(chrome[COLOR_BTNFACE]) < 0.1);
    CHECK(Luminance(panel[DP_ITEM_BK_NORMAL]) < 0.1);
    CHECK(Luminance(viewer[DV_VIEWER_BK_NORMAL]) < 0.1);
}

// ---------------------------------------------------------------------------
// Feature 044: dark Find-window surfaces (status bar, separators, disabled
// edit/toolbar text, progress bar) draw with these palette pairs (SC-002)

static void TestFindDarkModeSurfaces()
{
    COLORREF chrome[64];
    for (int i = 0; i < 64; i++)
        chrome[i] = 0;
#define TC_FILL(idx, r, g, b) chrome[idx] = RGB(r, g, b);
    THEME_DARK_SYSCOLORS(TC_FILL)
#undef TC_FILL

    // status bar text / "Found Items" label / header labels on the dark face
    CHECK(ContrastRatio(chrome[COLOR_BTNTEXT], chrome[COLOR_BTNFACE]) >= 4.5);
    // disabled edit text ("No Advanced Options") and disabled toolbar captions
    CHECK(ContrastRatio(chrome[COLOR_GRAYTEXT], chrome[COLOR_BTNFACE]) >= 3.0);
    // etched separators: a visible dark bevel pair, both halves darker than
    // the light-theme lines they replace (255/160)
    CHECK(chrome[COLOR_3DDKSHADOW] != chrome[COLOR_3DLIGHT]);
    CHECK(Luminance(chrome[COLOR_3DDKSHADOW]) < 0.1);
    CHECK(Luminance(chrome[COLOR_3DLIGHT]) < 0.1);
    // progress bar: accent bar visible on its dark track
    CHECK(ContrastRatio(chrome[COLOR_HIGHLIGHT], chrome[COLOR_BTNSHADOW]) >= 1.5);
    CHECK(Luminance(chrome[COLOR_BTNSHADOW]) < 0.1);
}

// ---------------------------------------------------------------------------
// Feature 029: dark adaptation of toolbar glyph colors
// (ThemeDarkAdaptColor in src/common/themes_palette.h; SC-002: adapted
// neutral strokes must reach >= 3:1 contrast on the dark COLOR_BTNFACE)

static void TestDarkIconColorAdaptation()
{
    const COLORREF darkBtnFace = RGB(45, 45, 45); // THEME_DARK_SYSCOLORS COLOR_BTNFACE
    int r, g, b;

    // pure black (typical outline) becomes the lightest adapted gray
    r = g = b = 0;
    ThemeDarkAdaptColor(&r, &g, &b);
    CHECK(r == 220 && g == 220 && b == 220);

    // neutral sweep [0,140): output stays neutral, lands in (140,220],
    // is monotonically non-increasing, and clears 3:1 on the dark toolbar
    int prev = 220;
    for (int v = 0; v < 140; v++)
    {
        r = g = b = v;
        ThemeDarkAdaptColor(&r, &g, &b);
        CHECK(r == g && g == b);
        CHECK(r > 140 && r <= 220);
        CHECK(r <= prev);
        prev = r;
        CHECK(ContrastRatio(RGB(r, g, b), darkBtnFace) >= 3.0);
    }

    // neutrals at/above 140 and white are left untouched
    for (int v = 140; v <= 255; v += 5)
    {
        r = g = b = v;
        ThemeDarkAdaptColor(&r, &g, &b);
        CHECK(r == v && g == v && b == v);
    }
    r = g = b = 255;
    ThemeDarkAdaptColor(&r, &g, &b);
    CHECK(r == 255 && g == 255 && b == 255);

    // dark saturated color: max channel scales to 170, hue (ratios) kept
    r = 100, g = 0, b = 0;
    ThemeDarkAdaptColor(&r, &g, &b);
    CHECK(r == 170 && g == 0 && b == 0);
    r = 60, g = 30, b = 0; // 2:1 red:green ratio must survive
    ThemeDarkAdaptColor(&r, &g, &b);
    CHECK(r == 170 && g == 85 && b == 0);
    r = 0, g = 0, b = 100; // dark blue accent brightens toward the same hue
    ThemeDarkAdaptColor(&r, &g, &b);
    CHECK(r == 0 && g == 0 && b == 170);

    // bright saturated accents are left untouched (colored icons stay colored)
    r = 255, g = 201, b = 14; // folder yellow
    ThemeDarkAdaptColor(&r, &g, &b);
    CHECK(r == 255 && g == 201 && b == 14);
    r = 0, g = 0, b = 255;
    ThemeDarkAdaptColor(&r, &g, &b);
    CHECK(r == 0 && g == 0 && b == 255);
    r = 200, g = 60, b = 60;
    ThemeDarkAdaptColor(&r, &g, &b);
    CHECK(r == 200 && g == 60 && b == 60);

    // deterministic: same input always produces the same output
    int r2 = 17, g2 = 17, b2 = 17;
    r = 17, g = 17, b = 17;
    ThemeDarkAdaptColor(&r, &g, &b);
    ThemeDarkAdaptColor(&r2, &g2, &b2);
    CHECK(r == r2 && g == g2 && b == b2);
}

// Feature 042: the file-name display-encoding defect class.
//
// Both reported defects were call-site defects, not helper defects, so these
// tests are the regression floor rather than the guard -- tools/check_encoding.py
// is what actually catches a recurrence. What is asserted here is the property
// every repaired call site depends on: a message composed from a localized
// template and a file name survives only when BOTH halves are UTF-8, and one
// legacy-codepage ingredient costs the whole message its wide rendering path.
static void TestComposedMessageEncoding()
{
    // The name as the file system holds it: "emoji-<U+1F642>-dir - Copy<U+011B>"
    const char* u8Name = "emoji-\xF0\x9F\x99\x82-dir - Copy\xC4\x9B";

    // (1) all-UTF-8 composition -> valid UTF-8 -> the wide path is available
    char composed[512];
    _snprintf_s(composed, _TRUNCATE, "Slozka obsahuje: %s", u8Name);
    WCHAR wide[512];
    CHECK(SalU8ToW(composed, -1, wide, _countof(wide)) != 0);
    CHECK(wcsstr(wide, L"emoji-") != NULL);
    CHECK(wcsstr(wide, L"\xD83D\xDE42") != NULL); // the surrogate pair survived
    CHECK(wcsstr(wide, L"\x011B") != NULL);       // e-caron survived

    // (2) mixed composition: one legacy-codepage byte in the template.
    //     0xE1 alone is 'a-acute' in CP1250 and is not valid UTF-8, which is
    //     exactly how a localized LoadStr() template poisoned the message.
    //     Strict conversion must REFUSE the whole string -- that refusal is the
    //     reported defect: CMessageBox then drew everything the legacy way and
    //     the name became mojibake.
    char mixed[512];
    _snprintf_s(mixed, _TRUNCATE, "Slo\xE1ka obsahuje: %s", u8Name);
    CHECK(SalU8ToW(mixed, -1, wide, _countof(wide)) == 0);

    // (3) the lenient display conversion never loses the whole string: the bad
    //     byte costs exactly one U+FFFD and the name beside it stays intact.
    CHECK(SalU8ToWDisplay(mixed, -1, wide, _countof(wide)) != 0);
    CHECK(wcsstr(wide, L"\xD83D\xDE42") != NULL);
    CHECK(wcsstr(wide, L"\x011B") != NULL);
    int replacements = 0;
    for (const WCHAR* p = wide; *p != 0; p++)
        if (*p == 0xFFFD)
            replacements++;
    CHECK(replacements == 1);

    // (4) a name outside the machine's legacy codepage must never be routed
    //     through it. This reproduces the Report 1 symptom directly: every
    //     UTF-16 unit that CP_ACP cannot represent becomes '?', so one emoji
    //     costs two. The assertion documents WHY FR-002 forbids that route.
    CHECK(SalU8ToW(u8Name, -1, wide, _countof(wide)) != 0);
    char lossy[512];
    BOOL usedDefault = FALSE;
    int n = WideCharToMultiByte(CP_ACP, 0, wide, -1, lossy, _countof(lossy), "?", &usedDefault);
    if (n > 0)
    {
        CHECK(usedDefault);                 // the codepage could not hold it
        CHECK(strstr(lossy, "??") != NULL); // two '?' for the one emoji
    }

    // (5) truncation must never split a surrogate pair in half
    // (buffer named 'tiny', not 'small' - the Windows headers define 'small' as char)
    WCHAR tiny[16];
    int written = SalU8ToWDisplay(u8Name, -1, tiny, _countof(tiny));
    if (written > 0)
    {
        WCHAR last = tiny[written - 2]; // before the terminator
        CHECK(!(last >= 0xD800 && last <= 0xDBFF));
    }
}

// Feature 043: a UTF-8 value must never be handed to a byte-oriented display
// call. Three surfaces were reported (the language picker, the configuration
// language field, the F2/F5/F6 caption) and all three shared one shape, so what
// is asserted here is the shape rather than the three instances.
static void TestUiTextEncoding()
{
    // (1) Locale display names are UTF-8 and must survive a round trip. This is
    //     what the language picker shows; it read "Cestina (Cesko)" as mojibake
    //     because the value went to the ANSI ListView_SetItemText.
    char locale[256];
    if (SalGetLocaleInfoU8(MAKELCID(MAKELANGID(LANG_CZECH, SUBLANG_DEFAULT), SORT_DEFAULT),
                           LOCALE_SLANGUAGE, locale, sizeof(locale)) != 0)
    {
        WCHAR wide[256];
        CHECK(SalU8ToW(locale, -1, wide, _countof(wide)) != 0); // valid UTF-8
        char back[256];
        CHECK(SalWToU8(wide, -1, back, sizeof(back)) != 0);
        CHECK(strcmp(locale, back) == 0); // lossless round trip
    }

    // (2) A caption composed from a UTF-8 template and a UTF-8 name stays valid
    //     UTF-8, so the wide drawing path is available. With an ANSI template
    //     the same caption is rejected and the NAME becomes mojibake while the
    //     localized words survive - which is exactly what users reported.
    const char* u8Name = "\xD0\xA2\xD0\xB5\xD1\x81\xD1\x82-\xC4\x9B\xC5\xA1"; // "Test-es" in Cyrillic + Czech
    char caption[512];
    _snprintf_s(caption, _TRUNCATE, "Prejmenovat adresar \"%s\" na", u8Name);
    WCHAR wide[512];
    CHECK(SalU8ToW(caption, -1, wide, _countof(wide)) != 0);
    CHECK(wcsstr(wide, L"\x0422\x0435\x0441\x0442") != NULL); // the Cyrillic survived
    CHECK(wcsstr(wide, L"\x011B\x0161") != NULL);             // the Czech survived

    //     the same caption with ONE legacy-code-page byte in the template is
    //     refused wholesale - the defect, asserted so it cannot come back
    char mixed[512];
    // the hex escapes are split so the letter after them is not swallowed into
    // the escape (\xF8e would parse as one very large character value)
    _snprintf_s(mixed, _TRUNCATE, "P\xF8"
                                  "ejmenovat adres\xE1"
                                  "r \"%s\" na",
                u8Name);
    CHECK(SalU8ToW(mixed, -1, wide, _countof(wide)) == 0);

    // (3) A number carrying the locale thousands separator is UTF-8 too. In
    //     Czech that separator is a non-breaking space (0xC2 0xA0), so a number
    //     sent to a byte-oriented field rendered as "1<A>234".
    char sep[16];
    if (SalGetLocaleInfoU8(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, sep, sizeof(sep)) != 0)
    {
        if (!SalIsASCII(sep)) // only meaningful where the separator is non-ASCII
        {
            char number[64];
            _snprintf_s(number, _TRUNCATE, "1%s234%s567", sep, sep);
            CHECK(SalU8ToW(number, -1, wide, _countof(wide)) != 0);
        }
    }

    // (4) Truncating a caption must never split a character or a surrogate pair.
    WCHAR tiny[12];
    int written = SalU8ToWDisplay(caption, -1, tiny, _countof(tiny));
    if (written > 0)
    {
        WCHAR last = tiny[written - 2];
        CHECK(!(last >= 0xD800 && last <= 0xDBFF));
    }
}

// Feature 052: the plugin metadata encoding contract. CPluginData's translated
// strings hold UTF-8 from every producer: plugin-supplied ANSI is normalized
// through SalLegacyToU8Alloc at the intake boundaries, and persisted values
// cross the registry facade as UTF-8 (stored UTF-16, returned UTF-8). The
// facade itself (SalRegSetValueExW8/SalRegQueryValueExW8, salamdr6.cpp) is not
// linked into this exe, so what is asserted is the conversion property both
// sides share plus the normalization helper; tools/check_encoding.py guards
// the call sites. The reported defect: the cached name of a not-loaded plugin
// (UTF-8 from the registry) went to the ANSI ListView_SetItemText and rendered
// as "HromadnA(c) ..." mojibake, while a loaded plugin's name (ANSI back then)
// rendered correctly - the same field carried two encodings.
static void TestPluginMetadataEncoding()
{
    // (1) ASCII passes through byte-identical (valid UTF-8 already)
    char* s = SalLegacyToU8Alloc("Disk Map 1.12");
    CHECK(s != NULL && strcmp(s, "Disk Map 1.12") == 0);
    free(s);

    // (2) valid UTF-8 is kept unchanged - the registry-read producer
    //     ("Hromadné přejmenování", the name from the bug report)
    const char* u8Name = "Hromadn\xC3\xA9 p\xC5\x99"
                         "ejmenov\xC3\xA1n\xC3\xAD";
    s = SalLegacyToU8Alloc(u8Name);
    CHECK(s != NULL && strcmp(s, u8Name) == 0);
    free(s);

    // (3) legacy ANSI is converted - the LoadStringA producer. Exact bytes can
    //     be asserted only under CP1250 (the conversion goes through CP_ACP).
    if (GetACP() == 1250)
    {
        const char* ansiName = "Hromadn\xE9 p\xF8"
                               "ejmenov\xE1n\xED";
        s = SalLegacyToU8Alloc(ansiName);
        CHECK(s != NULL && strcmp(s, u8Name) == 0);
        free(s);
    }

    // (4) whatever the codepage, the result is valid UTF-8 - the field must
    //     never carry mixed/legacy bytes to a consumer
    s = SalLegacyToU8Alloc("n\xE1zev \xF8"
                           "ol");
    CHECK(s != NULL);
    if (s != NULL)
    {
        WCHAR wide[64];
        CHECK(SalU8ToW(s, -1, wide, _countof(wide)) != 0);
        free(s);
    }

    // (5) the persistence round trip the registry facade performs (UTF-8 ->
    //     UTF-16 REG_SZ at rest -> UTF-8) is lossless for valid UTF-8 metadata
    WCHAR* w = SalU8ToWAlloc(u8Name);
    CHECK(w != NULL);
    if (w != NULL)
    {
        char* back = SalWToU8Alloc(w);
        CHECK(back != NULL && strcmp(back, u8Name) == 0);
        free(back);
        free(w);
    }

    // (6) clamping cuts only at a UTF-8 sequence boundary: "aé" (61 C3 A9)
    //     limited to 2 bytes drops the whole sequence, never leaves a dangling
    //     lead byte
    s = SalLegacyToU8Alloc("a\xC3\xA9", 2);
    CHECK(s != NULL && strcmp(s, "a") == 0);
    free(s);
    s = SalLegacyToU8Alloc("a\xC3\xA9", 3);
    CHECK(s != NULL && strcmp(s, "a\xC3\xA9") == 0);
    free(s);

    // (7) NULL stays NULL (callers treat it as "keep the previous value")
    CHECK(SalLegacyToU8Alloc(NULL) == NULL);
}

// WTF-8 byte sequences (feature 066): a lone surrogate U+D800..U+DFFF encodes
// as ED A0 80 .. ED BF BF
#define WTF8_D800 "\xED\xA0\x80"
#define WTF8_D801 "\xED\xA0\x81"
#define WTF8_DC00 "\xED\xB0\x80"
#define WTF8_REPRO "Lone" WTF8_D800 "surrogate.txt" // the reported repro name

static void TestWtf8()
{
    // (1) every class of lone surrogate round-trips W -> WTF-8 -> W
    //     (block boundaries + mid-range samples)
    static const WCHAR lone[] = {0xD800, 0xD83D, 0xDBFF, 0xDC00, 0xDDDD, 0xDFFF};
    for (int i = 0; i < _countof(lone); i++)
    {
        WCHAR in[2] = {lone[i], 0};
        char* u8 = SalWToU8Alloc(in);
        CHECK(u8 != NULL && strlen(u8) == 3);
        if (u8 != NULL)
        {
            WCHAR* back = SalU8ToWAlloc(u8);
            CHECK(back != NULL && wcscmp(back, in) == 0);
            free(back);
            free(u8);
        }
    }

    // (2) the reported repro name converts to the exact WTF-8 bytes and back
    const WCHAR* reproW = L"Lone\xD800surrogate.txt";
    char* u8 = SalWToU8Alloc(reproW);
    CHECK(u8 != NULL && strcmp(u8, WTF8_REPRO) == 0);
    if (u8 != NULL)
    {
        WCHAR* back = SalU8ToWAlloc(u8);
        CHECK(back != NULL && wcscmp(back, reproW) == 0);
        free(back);
        free(u8);
    }

    // (3) valid parts stay byte-identical to strict UTF-8 (a valid pair is
    //     one 4-byte sequence, never CESU-8) even next to a lone surrogate
    const WCHAR mixedW[] = {0x010D, 0xD800, 0xD83D, 0xDCC1, 0x0041, 0};
    u8 = SalWToU8Alloc(mixedW);
    CHECK(u8 != NULL && strcmp(u8, "\xC4\x8D" WTF8_D800 "\xF0\x9F\x93\x81"
                                   "A") == 0);
    if (u8 != NULL)
    {
        WCHAR* back = SalU8ToWAlloc(u8);
        CHECK(back != NULL && wcscmp(back, mixedW) == 0);
        free(back);
        free(u8);
    }

    // (4) decoder strictness is preserved for every OTHER malformed input -
    //     the "valid UTF-8, else ANSI" heuristics depend on these failing
    CHECK(SalU8ToWAlloc("\xC0\x80") == NULL);         // overlong 2-byte
    CHECK(SalU8ToWAlloc("\xE0\x80\x80") == NULL);     // overlong 3-byte
    CHECK(SalU8ToWAlloc("\xF0\x80\x80\x80") == NULL); // overlong 4-byte
    CHECK(SalU8ToWAlloc("\xED\xA0") == NULL);         // truncated surrogate sequence
    CHECK(SalU8ToWAlloc("\xED\xA0"
                        "A") == NULL);                // bad continuation byte
    CHECK(SalU8ToWAlloc("\x80") == NULL);             // stray continuation
    CHECK(SalU8ToWAlloc("\xF5\x80\x80\x80") == NULL); // lead above U+10FFFF
    CHECK(SalU8ToWAlloc("\xF4\x90\x80\x80") == NULL); // value above U+10FFFF
    CHECK(SalU8ToWAlloc("\xC4") == NULL);             // truncated 2-byte

    // (5) sized variants keep the terminator counting and the
    //     too-small-buffer -> empty-string fail-safe
    char cbuf[8];
    CHECK(SalWToU8(L"\xD800", 1, cbuf, 8) == 4 && strcmp(cbuf, WTF8_D800) == 0);
    CHECK(SalWToU8(L"\xD800", 1, cbuf, 3) == 0 && cbuf[0] == 0); // no room for the terminator
    WCHAR wbuf[8];
    CHECK(SalU8ToW(WTF8_D800, 3, wbuf, 8) == 2 && wbuf[0] == 0xD800 && wbuf[1] == 0);
    CHECK(SalU8ToW(WTF8_D800, 3, wbuf, 1) == 0 && wbuf[0] == 0); // exact-fit failure

    // (6) SalConvertFindDataW carries the true identity - the feature-066
    //     defect was exactly this intake substituting U+FFFD
    WIN32_FIND_DATAW fdw;
    memset(&fdw, 0, sizeof(fdw));
    wcscpy(fdw.cFileName, L"Lone\xD800surrogate.txt");
    char nameU8[SAL_FIND_NAME_U8];
    SalConvertFindDataW(&fdw, NULL, nameU8, sizeof(nameU8), NULL, 0);
    CHECK(strcmp(nameU8, WTF8_REPRO) == 0);
    WCHAR* back = SalU8ToWAlloc(nameU8);
    CHECK(back != NULL && wcscmp(back, fdw.cFileName) == 0);
    free(back);

    // (7) look-alike names (differing only in the lone surrogate) stay
    //     distinct and deterministically ordered; the comparison helpers
    //     must not crash on non-normalizable input (NormalizeString rejects
    //     unpaired surrogates -> byte-wise fallback)
    const char* twinA = "twin" WTF8_D800 ".txt";
    const char* twinB = "twin" WTF8_D801 ".txt";
    CHECK(!SalNameEquivalent(twinA, twinB));
    CHECK(SalNameEquivalent(twinA, twinA));
    CHECK(!SalNameEqualCI(twinA, -1, twinB, -1));
    CHECK(SalNameEqualCI(twinA, -1, twinA, -1));
    int ab = SalCompareNamesUTF8(twinA, -1, twinB, -1, TRUE);
    int ba = SalCompareNamesUTF8(twinB, -1, twinA, -1, TRUE);
    CHECK(ab != 0 && ba != 0 && (ab < 0) != (ba < 0));

    // (8) display decodes WTF-8 to the true unit (paints like Explorer);
    //     non-WTF-8 junk keeps the lenient replacement degradation
    WCHAR disp[32];
    CHECK(SalU8ToWDisplay("Lone" WTF8_D800 "s", -1, disp, _countof(disp)) > 0);
    CHECK(disp[4] == 0xD800 && disp[5] == L's');
    CHECK(SalU8ToWDisplay("a\xFF"
                          "b",
                          -1, disp, _countof(disp)) > 0);
    CHECK(disp[0] == L'a' && disp[1] == 0xFFFD);

    // (9) byte-structural helpers treat a WTF-8 sequence as one character
    CHECK(SalU8CharCount("Lone" WTF8_D800 "s", -1) == 6);
    const char* p = WTF8_D800 "s";
    CHECK(SalU8Next(p) == p + 3);

    // (10) the registry facade's data shape: a sized buffer with embedded
    //      terminators (REG_MULTI_SZ) converts as WTF-8 unit for unit
    const char multi[] = "a\0" WTF8_D800 "\0"; // "a", lone surrogate, double NUL
    WCHAR wmulti[8];
    int wl = SalU8ToW(multi, (int)sizeof(multi), wmulti, _countof(wmulti));
    CHECK(wl == 6 && wmulti[0] == L'a' && wmulti[1] == 0 &&
          wmulti[2] == 0xD800 && wmulti[3] == 0 && wmulti[4] == 0);
}

static void TestWtf8FileOps()
{
    char tmp[MAX_PATH];
    DWORD n = GetTempPathA(sizeof(tmp), tmp);
    if (n == 0 || n >= sizeof(tmp))
    {
        printf("skipping TestWtf8FileOps (no temp path)\n");
        return;
    }

    CSalPathBuf base;
    CHECK(base.Set(tmp));
    CHECK(base.AppendComponent("saltests-wtf8"));
    CHECK(SalCreateDirectory(base.Get(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS);

    // create the reported repro file through the facade (WTF-8 path -> the
    // true wide name lands on disk)
    CSalPathBuf file(base);
    CHECK(file.AppendComponent(WTF8_REPRO));
    HANDLE h = SalCreateFile(file.Get(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD written;
        CHECK(WriteFile(h, "066", 3, &written, NULL) && written == 3);
        CloseHandle(h);
    }

    // ground truth: enumeration sees the real U+D800 unit and the intake
    // conversion preserves the identity byte for byte
    WIN32_FIND_DATAW fd;
    CSalPathBuf pattern(base);
    CHECK(pattern.AppendComponent("*"));
    HANDLE find = SalFindFirstFile(pattern.Get(), &fd);
    CHECK(find != INVALID_HANDLE_VALUE);
    BOOL seen = FALSE;
    char nameU8[SAL_FIND_NAME_U8];
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wcscmp(fd.cFileName, L"Lone\xD800surrogate.txt") == 0)
            {
                seen = TRUE;
                SalConvertFindDataW(&fd, NULL, nameU8, sizeof(nameU8), NULL, 0);
                CHECK(strcmp(nameU8, WTF8_REPRO) == 0);
            }
        } while (SalFindNextFile(find, &fd));
        FindClose(find);
    }
    CHECK(seen);

    // attributes, copy, move, delete all address the true file; the copy and
    // move DESTINATION names carry lone surrogates too (name fidelity)
    CHECK(SalGetFileAttributes(file.Get()) != INVALID_FILE_ATTRIBUTES);
    CSalPathBuf copy(base);
    CHECK(copy.AppendComponent("copy" WTF8_DC00 ".txt")); // lone LOW surrogate
    CHECK(SalCopyFile(file.Get(), copy.Get(), TRUE));
    WIN32_FILE_ATTRIBUTE_DATA fad;
    CHECK(SalGetFileAttributesEx(copy.Get(), &fad) && fad.nFileSizeLow == 3);
    CSalPathBuf moved(base);
    CHECK(moved.AppendComponent("moved" WTF8_D800 ".txt"));
    CHECK(SalMoveFile(copy.Get(), moved.Get()));
    CHECK(SalGetFileAttributes(copy.Get()) == INVALID_FILE_ATTRIBUTES); // source gone
    CHECK(SalDeleteFile(moved.Get()));
    CHECK(SalDeleteFile(file.Get()));

    // a DIRECTORY with a surrogate name works as an ancestor path component
    CSalPathBuf sub(base);
    CHECK(sub.AppendComponent("dir" WTF8_D800 "sub"));
    CHECK(SalCreateDirectory(sub.Get(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    CSalPathBuf child(sub);
    CHECK(child.AppendComponent("child.txt"));
    h = SalCreateFile(child.Get(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);
    CHECK(SalDeleteFile(child.Get()));
    CHECK(SalRemoveDirectory(sub.Get()));
    CHECK(SalRemoveDirectory(base.Get()));
}

int main()
{
    TestConversions();
    TestNormalization();
    TestMatching();
    TestPathBuf();
    TestExtendedPaths();
    TestFileIO();
    TestDropFiles();
    TestLongComponentNames();
    TestDarkThemePalette();
    TestFindDarkModeSurfaces();
    TestDarkIconColorAdaptation();
    TestComposedMessageEncoding();
    TestUiTextEncoding();
    TestPluginMetadataEncoding();
    TestWtf8();
    TestWtf8FileOps();

    printf("saltests: %d checks, %d failed\n", g_checks, g_failures);
    return g_failures;
}
