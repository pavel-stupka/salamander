// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************
//
// salunicode.h
//
// UTF-8 <-> UTF-16 conversion and Unicode-aware file-name helpers.
//
// Internal narrow strings that carry file names and paths use UTF-8
// (feature 004-long-paths-unicode, research.md R1/R4). Conversion to
// UTF-16 happens only at OS boundaries (file APIs, GDI, window text,
// registry). Stored names are NEVER normalized or case-folded by these
// helpers; NFC normalization is applied to transient copies used for
// matching and collation only.
//
// Names containing unpaired UTF-16 surrogates (legal on NTFS) are not
// representable in strict UTF-8; since feature 066 the converter pair
// carries them as WTF-8 - a strict superset of UTF-8 encoding each lone
// surrogate as its 3-byte sequence ED A0 80..ED BF BF - so every on-disk
// name round-trips losslessly. For valid Unicode text the encoding is
// byte-identical to UTF-8. Binding contract:
// specs/066-fix-surrogate-filenames/contracts/name-encoding-wtf8.md
//

//*****************************************************************************
//
// SalU8ToW / SalWToU8
//
// Convert between WTF-8 (UTF-8 + lone-surrogate sequences, feature 066)
// and UTF-16. SalWToU8 is total: it succeeds for every input unit
// sequence. SalU8ToW stays strict for everything else: any malformed
// input other than a lone-surrogate sequence fails (returns 0) instead
// of being silently replaced - the "valid UTF-8, else ANSI" transitional
// heuristics (features 004/063) depend on that failure.
//
// Parameters
//   src: source string (need not be null-terminated when srcLen >= 0)
//   srcLen: length of src in bytes/WCHARs, or -1 for null-terminated
//   buf: destination buffer (may be NULL to query the required size)
//   bufSize: destination capacity in WCHARs/bytes
//
// Return Values
//   Number of WCHARs/bytes written including the terminating null
//   (or required size when buf == NULL); 0 on failure.
//
int SalU8ToW(const char* src, int srcLen, WCHAR* buf, int bufSize);
int SalWToU8(const WCHAR* src, int srcLen, char* buf, int bufSize);

// Allocating variants; caller releases with free(). Return NULL on failure.
WCHAR* SalU8ToWAlloc(const char* src, int srcLen = -1);
char* SalWToU8Alloc(const WCHAR* src, int srcLen = -1);

//*****************************************************************************
//
// SalLegacyToU8Alloc
//
// Normalize text of uncertain legacy origin to UTF-8 (feature 052).
//
// Returns a malloc'd copy of src: byte-identical when src already is valid
// UTF-8 (ASCII included), otherwise converted CP_ACP -> UTF-8 - the same
// transitional tolerance the registry facade applies on write
// (SalRegSetValueExW8). Use at intake boundaries whose producers still emit
// ANSI, e.g. plugin-supplied metadata (contract:
// specs/052-fix-plugin-name-encoding/contracts/plugin-metadata-encoding.md).
//
// maxBytes >= 0 clamps the result to at most maxBytes bytes (terminator
// excluded), cutting only at a UTF-8 sequence boundary; -1 = no limit.
//
// Return Values
//   malloc'd string (caller frees); NULL only for NULL input or when
//   allocation/conversion fails.
//
char* SalLegacyToU8Alloc(const char* src, int maxBytes = -1);

//*****************************************************************************
//
// SalU8ToWDisplay
//
// LENIENT UTF-8 -> UTF-16 conversion, for DISPLAY ONLY (feature 041).
//
// Unlike SalU8ToW, malformed input does not fail: each offending byte becomes
// U+FFFD and conversion continues. WTF-8 names (feature 066) decode to their
// true units first, so a lone surrogate paints as the font's notdef glyph,
// exactly like Explorer. Use this only where the result is drawn and
// then discarded.
//
// NEVER use it on a value that will be written back into a name, a path, or
// anything persisted - substituting characters there would corrupt user data,
// which is exactly what the strict variant above exists to prevent.
//
// Rationale: a display surface that composes several fields into one string
// must not let one bad field destroy the others. Strict conversion turns a
// single stray byte into a whole unreadable line; this turns it into a single
// visible replacement character.
//
// Return Values
//   Number of WCHARs written including the terminating null (or the required
//   size when buf == NULL); 0 only when src is NULL or the buffer is too small.
//
int SalU8ToWDisplay(const char* src, int srcLen, WCHAR* buf, int bufSize);

// Allocating variant; caller releases with free(). NULL only when src is NULL
// or the allocation fails - never because the input was malformed.
WCHAR* SalU8ToWDisplayAlloc(const char* src, int srcLen = -1);

//*****************************************************************************
//
// Locale text as UTF-8 (feature 041)
//
// The application's narrow strings are UTF-8. These wrappers close the last
// hole in that rule: text obtained from the user's regional settings. Each
// calls the W variant of the underlying API and converts the result to UTF-8,
// so callers get text in the same encoding as everything else they concatenate
// it with.
//
// Before feature 041 the ANSI variants were called directly, and their output
// - the Czech thousands separator is a non-breaking space, one 0xA0 byte -
// made the composed string invalid UTF-8. Every non-ASCII character in the
// information line was then rendered through the legacy byte path as mojibake.
//
// Return Values
//   Number of BYTES written including the terminating null, or 0 on failure -
//   matching the A functions these replace, so existing "== 0" checks and
//   "- 1" length arithmetic keep working (the value is a byte count in both).
//
int SalGetLocaleInfoU8(LCID locale, LCTYPE lcType, char* u8Buf, int u8BufSize);
int SalGetDateFormatU8(LCID locale, DWORD flags, const SYSTEMTIME* date,
                       const char* u8Format, char* u8Buf, int u8BufSize);
int SalGetTimeFormatU8(LCID locale, DWORD flags, const SYSTEMTIME* time,
                       const char* u8Format, char* u8Buf, int u8BufSize);

//*****************************************************************************
//
// SalWToACPLossless
//
// Convert UTF-16 to the system legacy ANSI code page WITHOUT best-fit
// mapping. Used by the legacy plugin adaptation shim (contract §2):
// a lossy result means the item must be refused, never passed altered.
//
// Return Values
//   TRUE when every character was representable; FALSE otherwise
//   (buf receives nothing usable on FALSE).
//
BOOL SalWToACPLossless(const WCHAR* src, int srcLen, char* buf, int bufSize);

//*****************************************************************************
//
// SalNormalizeNFC
//
// Normalize a UTF-16 string to Unicode Normalization Form C.
// Output is a transient value for matching/collation; never write it
// back into stored names.
//
// Return Values
//   Number of WCHARs written including the terminating null (or the
//   estimated required size when buf == NULL); 0 on failure.
//
int SalNormalizeNFC(const WCHAR* src, int srcLen, WCHAR* buf, int bufSize);
WCHAR* SalNormalizeNFCAlloc(const WCHAR* src, int srcLen = -1); // free() the result

//*****************************************************************************
//
// SalIsASCII
//
// Fast path predicate: TRUE when the buffer contains only bytes < 0x80.
// ASCII-only strings are valid UTF-8 and byte-wise comparable, so
// callers keep the legacy comparators for them (research.md R5).
//
BOOL SalIsASCII(const char* s, int len = -1);

//*****************************************************************************
//
// SalU8Next / SalU8CharCount
//
// Walk and measure UTF-8 by characters (code points). SalU8Next returns the
// pointer behind the character starting at 's' (identity on the terminator).
// SalU8CharCount counts the characters in the first 'len' bytes (-1 =
// null-terminated). Byte-oriented callers that pad or truncate visible text
// use these so a multi-byte character is never split (feature 063).
//
const char* SalU8Next(const char* s);
int SalU8CharCount(const char* s, int len = -1);

//*****************************************************************************
//
// SalNameEquivalent
//
// TRUE when two UTF-8 names are canonically equivalent (their NFC
// forms are binary-identical). Case-SENSITIVE. Byte-identical names
// are always equivalent (fast path, no conversion).
//
BOOL SalNameEquivalent(const char* u8a, const char* u8b);

//*****************************************************************************
//
// SalCompareNamesUTF8
//
// Locale collation of two UTF-8 names for panel sorting (FR-009).
// Canonically equivalent names compare equal here; callers tie-break
// with a binary comparison to keep the order deterministic.
// ASCII fast path is NOT applied here - callers decide (sort.cpp
// keeps its byte-wise comparators when both names are ASCII).
//
// Return Values
//   < 0, 0, > 0  (strcmp convention); on conversion failure falls back
//   to strcmp so sorting never loses items.
//
int SalCompareNamesUTF8(const char* u8a, int aLen, const char* u8b, int bLen, BOOL ignoreCase);

//*****************************************************************************
//
// SalNameEqualCI
//
// Case-insensitive, canonical-equivalence-insensitive equality of two
// UTF-8 names (quick search, Find, mask matching - FR-008).
//
BOOL SalNameEqualCI(const char* u8a, int aLen, const char* u8b, int bLen);
