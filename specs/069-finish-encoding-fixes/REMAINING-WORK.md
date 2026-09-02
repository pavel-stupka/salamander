# Feature 069 — Remaining Work (handoff)

**Written**: 2026-08-24 · **Branch**: `069-finish-encoding-fixes` ·
**Predecessor handoff**: `specs/068-encoding-regression-review/REMAINING-WORK.md`

Feature 069 closed the *contained* remainder of the 068 review. This file is
what is left, and it is self-contained: nothing here needs the 069 conversation.

---

## 0. What 069 finished (so it is not re-done)

| | |
|---|---|
| Items in scope | 34 from 068 section 1 + D01–D05 |
| **Fixed** | **31** findings + D01, D02, D03, D04, D05 |
| Verify-closed | 3 — F-P1-03 (X06/X07), F-P2-10 (X02, a duplicate of F-P6-02), the jump-list half of F-P1-25 (X03) |
| Fix groups | 11, each its own commit, each independently regression-reviewed |
| Reviews | 6 batches; **4 REJECTED** first — twelve regressions the fixes themselves introduced, six severe and one destructive. Each correction was itself reviewed, and every such review found something; the last ended in a **revert** ([regression-X16-X22.md](findings/regression-X16-X22.md)) |
| Tests | `saltests` 1257 → **1301**, 0 failed |
| Guard | strict `TOTAL: 0` with **10** rules (`acp-title-seed` added, `signed-char-name-byte` retired in favour of `acp-byte-table-on-name`); draft 183 → 149 |
| Plugin ABI | untouched — interface 106, no forwarder change, `src/plugins/shared/` diff is comments only |

**Do not re-open**: the fix list is in `closing-report.md` with a per-item
disposition, the reviewer's verdict and the check for each.

---

## 0b. Deferred out of 069 by its own reviews

| Item | Why it is not in 069 | What it needs |
|---|---|---|
| **F-P1-05, the archive *listing* display encoding** (`pack1.cpp`, both sites) | Three attempts, three defects: a **fatal listing abort** (an archive with a long non-ASCII path stopped opening at all), then a **tree split** (the panel showing one folder twice with the files divided between them), then a narrower tree split. The cause is structural: the decision can only be made per item, but `CSalamanderDirectory::FindDir` matches directory components by **bytes**, so any per-item fallback splits the tree. | The listing must move **as a whole**: every name and path in one archive in one encoding, with `AddFile`/`AddDir`'s `MAX_PATH - 5` limit raised or measured in the target encoding first. Not a contained fix. The list-file and unpack halves of F-P1-05 **are** fixed and do not depend on this. |

---

## 1. The five systemic clusters — still the real remainder

Unchanged from the 068 handoff except where 069 sharpened them. Each is its own
feature; none has a minimal fix.

| # | Cluster | Findings | What 069 learned about it |
|---|---|---|---|
| **B-1** | **ANSI dialog windows** — 88 of 90 dialog constructions | F-P3-05, F-P6-03, F-P6-06, F-P2-12, F-P3-04 | Now has a **first candidate with its surface enumerated**: the command line. `research.md` R2 lists exactly what moves with it — the word-break callback ABI (`EDITWORDBREAKPROCA` → W), the `WM_CHAR` unit, and all five selection-offset sites (`EM_SETSEL`, `EM_GETSEL`, `EM_CHARFROMPOS`, `EM_POSFROMCHAR` and the save/restore pair). 069 fixed the *insert* so the control is internally consistent; what remains is names outside the active code page, which insert as `?`. Also here: the browse-dialog sub-case of F-P1-24 (the ANSI common dialog loses a non-code-page path before the application sees it). |
| **B-2** | **Code-page byte tables behind all name comparison** | F-P3-06, F-P5-02 | The guard rule for it now exists and is named: `acp-byte-table-on-name`, **33 report-only hits** — that is the work list. |
| **B-3** | **`GetErrorText` returns UTF-8, undocumented** | F-P5-12 + the core half of F-P2-01/02/03/06 | 069 deliberately left every composition whose other half is `GetErrorText` alone, so the cluster is intact and its boundary is clean. A naive sweep still regresses FTP. |
| **B-4** | **`AlterFileName` byte-folds names** | F-P5-13 | Untouched. Still the highest-risk fix in the review (it also drives Change Case, which renames on disk). |
| **B-5** | **Plugin-facing services are ANSI and frozen** | F-P5-03, F-P5-07, F-P5-01, F-P5-10, F-P5-11 | 069 removed **two** items from this cluster by proving them safe: F-P1-21's plugin-viewer temp file (the plugin receives only the file-name string, which does not change) and **D02** (the plugin *sends* better-formed text to a tolerant `Sal*U8` sink, and English bytes are identical). The rest stands. Also here: the `CCodeTablesData::Name` invariant — the names are handed to plugins by `EnumConversionTables` and accepted back by `GetConversionTable`, and `dbviewer`/`filecomp` persist them, so they cannot be re-encoded without an interface decision. |

---

## 2. Sites 069 deliberately did not convert, each with its reason

These are **not** oversights. Each was analysed, and converting it would have
been a regression or a half-fix.

1. **`icncache.cpp:796`** (F-P1-23, the file-type icon location). Its value is
   read one line earlier through the *old* ANSI `SalRegQueryValueEx` wrapper and
   flows into the icon cache. Converting only the expansion leaves the mixed
   chain that got two of 068's fixes rejected. Fix it together with that
   registry read and every consumer of `iconLocation` → the remaining-facade
   migration.
2. **`shellsup.cpp:1025` and `:1440`** (DROPFAKE / CLIPFAKE). **Reverted on
   review.** The fake directory's name is published into shared memory
   (`SalShExtSharedMemView->DragDropFakeDirName`, a `char[MAX_PATH]` ABI field in
   `src/shexreg.h`) and compared by the **ANSI-built shell extension**
   (`src/shellext/copyhook.c:202,252`), which receives `pszSrcFile` from
   `ICopyHookA` in the code page. Creating the directory successfully while the
   hook cannot recognise it is *worse* than failing: the shell then really
   copies the empty `DROPFAKE` folder to the drop target. Fix it together with
   `copyhook.c` — a cross-binary change (32-bit and 64-bit shell extensions).
3. **`shellib.cpp`, six `IShellFolder`/`STRRET` sites** (F-P1-27's second half).
   Each needs its own consumer analysis; the verifier scoped the finding's
   confirmed consequence to the shared-folder marker and `GetUNCPath`, both
   fixed.
4. **`fileswn0.cpp:333` / `fileswn2.cpp:157`** keep the ANSI `IShellLink`, so
   their `GetPath` still returns a code-page target. `shellsup.cpp` — the site
   the finding named — moved to `IShellLinkW`; these two are the same shape at
   sites the finding did not name. Small and safe; do them with any future
   shell-boundary pass.
5. **`execute.cpp:1213`** (`$(DOSPath)` for a user-menu item) is the same
   expansion as the fixed `editwnd.cpp` site and is still ANSI.
6. **Reporting a failed archive-edit copy.** F-P1-20 now checks
   `SHFileOperationW`'s result and traces it, but does not *show* it: there is
   no translated string for "the copy failed" and adding one touches all eight
   shipped languages. The silent-failure **cause** is fixed.
7. **`IDS_VIEWERTITLE` at nine sites** (`viewer2.cpp` ×8, `viewer3.cpp:1670`)
   still use ANSI `LoadStr`. They are homogeneous and render correctly; the
   `missed-twin` guard lists them (+9) as the work list for a future
   viewer-title pass. Converting correct code would have been opportunistic
   cleanup.
8. **`worker.cpp:7058`** — F-P1-21's "link operation" site is inside a
   `/* … */` design sketch. Dead code; out of scope by charter.
9. **`mainwnd3.cpp` FMExt `szName`** — inside `#if (_MSC_VER < 1700)`, dead
   under the shipped toolset (`wfext.h` is not in any installed Windows Kit).
   Reverted on review because an uncompiled change is untested code.

---

## 3. New defects found while doing the work (none is in an 068 finding)

> **CLOSED by feature 075** (2026-09-02, branch `075-fix-small-hardening`).
> All five were fixed, each in its own commit, each independently reviewed —
> one was **rejected on review and reworked**. Record and evidence:
> [`specs/075-fix-small-hardening/fix-log.md`](../075-fix-small-hardening/fix-log.md).
> The list below is kept as written, with each item's disposition appended, so
> the reasoning that produced it stays readable.
>
> | # | Fixed as | Commit | Review |
> |---|---|---|---|
> | 1 `codetbl.cpp:873` | D1 — and it was **two** overflows, not one: the same function also copied an unbounded `convert.cfg` name into a 1024-byte stack scratch | `8102dd8` | ACCEPTED |
> | 2 `viewer3.cpp:3291` | D2 | `ff1c684` | ACCEPTED |
> | 3 `zip.cpp:3292` | D3 | `2230787` | ACCEPTED |
> | 4 `viewer3.cpp:30/35` | D4 | `e197a11` | ACCEPTED |
> | 5 `filecomp/controls.cpp:24,39` | D5 | `1ffdf3b` | REJECTED → reworked → ACCEPTED |
>
> The "**Fix this first**" instruction on item 1 is retired — it was followed.
> Two site references had drifted (item 1 is at `:874`, item 2 at `:3300`), which
> is the third time this handoff's line numbers have aged; feature 075 re-ran the
> A0 check before touching anything, as the protocol requires.

Recorded rather than fixed, because FR-001 forbids a change without a finding
behind it. **The first one is a real out-of-bounds write.**

1. **`src/codetbl.cpp:873` — one-byte buffer overflow.**
   `if (len > bufferLen) len = bufferLen - 1;` should be `>=`: a conversion name
   of exactly `bufferLen` bytes writes `buffer[bufferLen]`. Callers pass
   `codeName[200]` (`viewer3.cpp:58`) and `DefaultConvert[200]`
   (`viewer3.cpp:1904`). Unreachable with the shipped names (longest is 33
   bytes) but it is an out-of-bounds write. **Fix this first.**
2. **`src/viewer3.cpp:3291`** — `GetCodeType`'s return value ignored, so
   `defCodeType` is used uninitialised when the tables are not loaded.
3. **`src/zip.cpp:3292`** — `GetConversionTable` does not NULL-check
   `conversion` (pre-existing; no new exposure).
4. **`src/viewer3.cpp:30/35`** — `lstrcpyn(caption, FileName, MAX_PATH)` can cut
   a path longer than 259 bytes mid-character, which drops the whole caption to
   the legacy draw. The F-P4-02 fixes therefore do not help very long non-ASCII
   paths.
5. **`src/plugins/filecomp/controls.cpp:24,39`** — unbounded `strcpy(Text,
   text)`, safe today only because source and destination are both `[MAX_PATH]`.
6. **`src/editwnd.cpp:577`** — `EM_SETSEL` receives UTF-8 byte offsets while the
   control holds code-page characters. Already mismatched before 069 and
   unchanged by it; it belongs with the B-1 command-line work.

---

## 4. Still owed: the on-screen sweep (maintainer)

Everything automatable is green. This part needs a person.

- **Binaries**: `build\tandemcommander\Debug_x64\` and `Release_x64\`;
  **pre-fix reference for side-by-side** = `Release_x64_prefix069\` (347 files,
  preserved deliberately — do not delete it before the sweep).
- **Fixtures**: recreated by the script in `quickstart.md` ("Fixtures");
  `D:\Zkouška\Můj disk\`, `…\Árvíztűrő tükörfúrógép\`, `…\surrogate\`,
  `%TEMP%\salamander-test\perf` (100,000 files, already created).
- **What to run**: the 068 sweep **W1–W20** in the Czech UI and then the
  Hungarian UI — these are the surfaces earlier features repaired and the point
  is to prove 069 did not disturb them — followed by **V-01…V-24** in
  `quickstart.md`, which are this feature's own per-fix scenarios.
- **G7 English spot-check**: W1–W6 and W13 in the English UI against
  `Release_x64_prefix069`.
- **Start here** (the three highest-consequence fixes): V-01 the command line
  (`Přehled.txt`, Ctrl+Enter), V-09 help and `config.reg` under an accented
  install path, V-11 the cloud entries.
- A sweep failure is a finding: back through fix → independent review → gates.

---

## 5. How to continue (the protocol that caught four regressions)

`contracts/fix-protocol.md` is binding and it earned its keep: of four review
batches, **two were rejected**, and both rejections were regressions the fixes
themselves introduced — a progress title blanked in five languages, and a
half-converted chain that made the shell copy a stray folder. Neither would have
been caught by the build, the 1,289 tests or the static guard.

The parts that mattered most, in order:

1. **Check the site is still defective at HEAD first.** Three of the 34 items
   were already fixed, and five site references in the findings were stale.
2. **Enumerate the consumers yourself, before writing anything.** The C5
   classification (`findings/c5-consumer-classification.md`) is the model: 37
   producers, only 3 of which may move.
3. **Never blank text, never skip an operation.** Both rejections violated this.
4. **A latent conversion must be a provable no-op** — not "probably harmless".
   The seven plugin-loading messages looked latent and were a degradation.
5. **Independent review by an agent that did not write the fix**, using the
   charter. It must enumerate consumers itself.
