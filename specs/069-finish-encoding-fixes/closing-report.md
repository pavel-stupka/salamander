# Closing report — feature 069, finish the contained encoding fixes

**Branch**: `069-finish-encoding-fixes` · **Baseline**: `64dcbb5` (post-068,
unreleased) · **Started**: 2026-08-24

This is the record spec FR-014 requires: every item of the 068 handoff's
section 1 and section 3 with its disposition, its fix, its independent
regression verdict and its check.

## Baseline (T001, T002)

| What | Value | Evidence |
|---|---|---|
| Debug full build | **SUCCEEDED**, 48 s | `build.cmd full`, Debug x64, 19 plugins, 180 language modules |
| Release full build | **SUCCEEDED** | `build.cmd full release` |
| Unit tests | **1257 checks, 0 failed** | `build\tandemcommander\Debug_x64\saltests\saltests.exe` |
| Encoding guard, strict | **TOTAL: 0** | `python tools\check_encoding.py --strict` |
| Encoding guard, draft | **TOTAL: 183** (`ansi-api-on-utf8-path` 88, `cp-acp-utf8-source` 11, `signed-char-name-byte` 42, `missed-twin` 42) | `--draft` |
| Pre-fix English reference | `build\tandemcommander\Release_x64_prefix069\`, **347 files** | robocopy of `Release_x64` before any change |

Fixtures created (T003, T004): `D:\Zkouška\Můj disk\` (Czech sweep folder with
`Přehled.txt`, `poznámky.txt`, `Účtenka.pdf`, `žluťoučký kůň.docx`,
`příloha.txt`, `Smlouva – kopie.docx`, `1 000 000.pdf` with real NBSP),
`D:\Zkouška\Árvíztűrő tükörfúrógép\bájt.txt`, `D:\Zkouška\Účetnictví\`,
`D:\Zkouška\Kopie\`, `D:\Zkouška\surrogate\Lone<U+D800>surrogate.txt`,
`D:\Zkouška\Dočasné\`, `D:\Zkouška\Šablony\`, `D:\Zkouška\Zálohy\Projekty\`.

## Verify-closed items (T007–T009)

These three were listed as remaining by the handoff but are already fixed in
the tree; **no code change** was made for them (spec FR-017).

| Item | Evidence at HEAD | Disposition |
|---|---|---|
| **F-P1-03** startup `SAL*.tmp` cleanup | `src/cache.cpp:1484-1486` `GetTempPathW` + `SalWToU8` produce a UTF-8 `tmpDir`; `:1499` `SalFindFirstFile` enumerates through the facade; `:1554` hands UTF-8 to `RemoveTemporaryDir`, which itself converts (`src/salamdr3.cpp:1026-1038`, `SalU8ToW` + `SetCurrentDirectoryW`, ANSI fallback); `:1560` `WM_USER_FOCUSFILE` receives the same UTF-8 value | **verify-closed** (X06/X07) |
| **F-P2-10** Plugins Manager "Change Drive menu" checkbox | `src/dialogs5.cpp:495` is `SalGetDlgItemTextU8`, with a comment naming *F-P6-02* — the same site found twice by two perspectives; the sibling at `:492` was converted in the same fix | **verify-closed** (X02, duplicate of F-P6-02) |
| **F-P1-25**, jump-list half | `src/jumplist.cpp:151-163` records the defect and uses `IShellLinkW`/`CreateShellLink(const char*, const char*, IShellLinkW**)`; the ANSI `IShellLink` + `VT_LPSTR` title are gone | **verify-closed** (X03 = F-P4-06); the seven `MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, …)` sites and the shortcut-target probe remain in scope |

**Consequence for the scope**: 34 section-1 items = **31 to fix** + 3
verify-closed. Recorded in `research.md` R1 and spec SC-001.

## Guard hygiene (T010)

`signed-char-name-byte` was **narrowed and renamed** to
`acp-byte-table-on-name` rather than deleted, which is what tasks.md T010
proposed. Reason: the rule had two halves and only one premise is void. The
signed-char half rests on plain `char` being signed, which `/J` makes false
(068 ledger L07); the byte-table half (`LowerCase[]`/`IsAlpha[]` indexed by a
UTF-8 name byte) is the real cluster B-2 defect, and the 068 report says the
rule is kept until its replacement lands. Deleting it would have dropped that
signal with nothing in its place.

Result: draft `TOTAL: 183 → 174`; the rule's own count `42 → 33` (the 9 removed
hits were signed-char-only), strict stays `TOTAL: 0`. Line endings (CRLF) and
the file's encoding preserved; 23 insertions, 17 deletions.

## Fixes

| Fix | Item(s) | Group | Change | Regression verdict | Check | Timing |
|---|---|---|---|---|---|---|
| — | — | — | *(filled as each group lands)* | — | — | — |

## Deferred here

| Item | Reason | Recorded for |
|---|---|---|
| *(filled as decided)* | | |

## Gates

| Gate | Result | Evidence |
|---|---|---|
| G1–G4 per group | *(per fix, below)* | |
| G1–G7 final | *(pending)* | |
| G8 on-screen sweep | *(maintainer)* | |
