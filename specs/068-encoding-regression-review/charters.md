# Review Charters — perspectives P1–P7, Verifier, Regression reviewer

**Feature**: 068-encoding-regression-review · research.md R2/R4/R5 ·
data-model.md · contracts/encoding-contract-checklist.md (the "checklist").

## Common rules (every perspective)

1. **Read-only on the product.** You never modify anything under `src/`,
   `tools/`, `translations/`. You write exactly one file:
   `specs/068-encoding-regression-review/findings/P<n>.md` (created by you).
   The consolidation step merges it into `inventory.md` and `review-report.md`.
2. **Ground truth is the code**, not comments: `src/consts.h` still carries
   pre-004 comments on facade functions that now live in
   `src/common/salfileio.cpp` (research R7 C-k). Read the implementation.
3. **The core is built without `UNICODE`** — every un-suffixed Win32 text API
   (`SetWindowText(`, `CreateFile(`, `RegOpenKeyEx(` …) is the ANSI entry
   point. `HANDLES(CreateFile(…))` is still an ANSI call.
4. **House machinery** (what "correct" looks like): converters
   `SalU8ToW`/`SalU8ToWAlloc` (strict WTF-8 decoder, returns 0/NULL on
   malformed **and** on too-small buffer), `SalWToU8`/`Alloc` (total),
   `SalLegacyToU8Alloc` (valid UTF-8 kept, else CP_ACP→UTF-8),
   `SalU8ToWDisplay*` (lenient, display only) — `src/common/salunicode.h`;
   sinks `SalSetWindowTextU8`, `SalSetDlgItemTextU8`, `SalGetWindowTextU8`,
   `SalComboAddStringU8`, `SalListBoxAddStringU8`, `SalInsertMenuItemU8`,
   `SalListViewSetItemTextU8`, `SalStatusSetTextU8` (`src/common/winlib.h`
   :326-352; invalid UTF-8 falls back to the raw `A` call), `CStaticText::
   SetText` (CP_ACP ladder), `CMessageBox` (all-or-nothing wide path),
   `LoadStrU8` (UTF-8) vs `LoadStr` (ANSI), `NumberToStr`/`PrintDiskSize`
   (UTF-8 separator; `PrintDiskSize(…, TRUE)` whole-string UTF-8),
   `SalGetLocaleInfoU8`/`SalGetDateFormatU8`/`SalGetTimeFormatU8`, facades
   `SalCreateFile`/`SalDeleteFile`/`SalFindFirstFile`/… (`src/common/
   salfileio.h`), `SalPathToWExtAlloc` (`src/common/salpath.h`),
   `SalRegQueryValueExW8`/`SalRegSetValueExW8` (`src/salamdr6.cpp:2298+`),
   `CopyTextToClipboardU8`/`W` (`src/salamdr4.cpp:1190/1144`).
5. **Every Site gets one of four classifications** with one evidence line:
   `verified-correct` (say why: W path / facade / ASCII by construction /
   consistent ANSI chain with a producer that cannot carry non-ACP text),
   `defective` (→ a Finding), `latent` (name the non-shipping configuration:
   disabled language ru/uk/zh, a separator no shipped locale uses, an ACP
   that is not 1250), `out-of-scope` (vendored, dead code, developer tooling,
   diagnostic-only text — say which).
6. **Grouping**: sites that are one pattern in one function/purpose (e.g. "the
   14 `DeleteFile` calls on cache temp paths in `cache.cpp`") are one row
   with **every** `file:line` in Location and one evidence line. Every line
   of your candidate files must be accounted for — individually or inside a
   group. Lines that are false positives of the grep (declarations,
   comments, `W` variants, unrelated identifiers) go in a short "Dismissed"
   list with the reason.
7. **A Finding needs a failure scenario** — surface, locale/UI language, what
   the user sees or which operation fails, with the concrete data path
   (`file:line` chain from the UTF-8/ANSI source to the sink). No scenario →
   it is a **Note**, not a Finding. You raise; you never verdict.
8. **Seeds are questions, not answers.** Confirm or refute each seed assigned
   to you with evidence; a refuted seed is recorded as such.
9. **Ledger rows** assigned to you: re-examine each against the current code
   and write a one-line re-examination note plus a proposed disposition
   (`still-open` / `closed-by-<feature>` / `fix-candidate` / `by-design` /
   `latent`).
10. **Scope of fixes is not your concern** — classify plugin-internal sites
    honestly as defective when they are; the scope test (FR-012/FR-015)
    happens later.

## Output file format — `findings/P<n>.md`

```markdown
# P<n> — <name>

## Coverage
- files read line-level: …
- candidate lists processed: … (lines accounted: N of M)

## Inventory rows
### B<k> — <boundary>            (repeat per boundary you own; use X for cross-cutting)
| ID | Location | Pattern | Data | Classification | Evidence | Persp. | DC | Finding |
|---|---|---|---|---|---|---|---|---|
| S-B2-P1-001 | src/cache.cpp:120,131,… | DeleteFile (A) | temp path from SalGetTempFileName (UTF-8) | defective | UTF-8 path to ANSI API; non-ASCII %TEMP% ⇒ ERROR_FILE_NOT_FOUND | P1 | DC-01 | F-P1-01 |

## Dismissed grep lines
- src/foo.cpp:123 — declaration, not a call

## Findings
### F-P1-01 — <one-line claim>
- Sites: S-B2-P1-001
- Defect class: DC-01
- Failure scenario: <surface>; <locale/UI language>; <what the user sees / which operation fails>
- Data path: src/salamdr3.cpp:216 (SalGetTempFileName → UTF-8) → src/cache.cpp:120 DeleteFile(A)
- Evidence: <file:line quotes, 1–3 lines>
- Suggested minimal fix (optional, one line)

## Seeds
- S1: CONFIRMED / REFUTED — evidence
## Ledger rows
| ID | Re-examination note | Proposed disposition |
## Contract verdicts            (only if you own contract obligations)
| Obligation | Verdict (compliant / deviation → F-id) | Evidence |
## Notes
- observations without a failure scenario (documentation defects, stale comments, style)
```

IDs: Sites `S-B<k>-P<n>-<seq>`, Findings `F-P<n>-<seq>`. Consolidation renumbers.

---

## P1 — File-system, shell & launch boundary

**Boundaries**: B1, B2, B5 (launch/command lines). **Classes**: DC-01, DC-02
(A→W direction), DC-09 (ANSI producer → strict facade). **Queues**:
`candidates/dc01-ansi-fs-shell-process-registry.txt` (all rows except the
`Reg*` rows, which P4 owns), `candidates/dc02-cp-acp.txt`. **Files**
(line-level for the candidate lines and their data flow): `src/pack1.cpp`,
`pack2.cpp`, `pack3.cpp`, `salamdr1.cpp`, `salamdr2.cpp`, `salamdr3.cpp`,
`cache.cpp`, `fileswn6.cpp`, `fileswn8.cpp`, `plugins2.cpp`, `drivelst.cpp`,
`shellib.cpp`, `shiconov.cpp`, `snooper.cpp`, `geticon.cpp`, `mainwnd3.cpp`,
`mainwnd4.cpp`, `editwnd.cpp`, `execute.cpp`, `worker.cpp`, `salmoncl.cpp`,
`shellsup.cpp`, `src/common/handles.cpp/.h` (debug shims — one group).

**Method**: for each candidate call, trace the argument back to its producer:
UTF-8 (panel path `GetPath()`, `CFileData::Name`, `SalGetTempFileName`
output, configuration value, any `LoadStrU8`/`NumberToStr` value, a
`SalWToU8` result) ⇒ **defective** unless proven ASCII-by-construction
(drive roots `"C:\"`, module names, fixed literals); ANSI producer (an `A`
API result, `LoadStr`) consumed consistently by an ANSI consumer ⇒
verified-correct-consistent **but a Finding if the value can carry non-ACP
text** (volume labels, share names) or later meets a strict facade (DC-09).
Note `SalGetDriveTypeU8` (062) exists — every remaining `GetDriveType(` is a
DC-18 twin candidate too.

**Seeds** (research R7): C-a `cache.cpp` `DeleteFile` ×14 on temp paths;
C-b `pack1/2/3` FS calls and the `CharToOem`/`OemToChar` cluster on packer
command lines; C-c `GetModuleFileName` ×40 (non-ASCII install dir — what
breaks: `.slg` enumeration, plugin loading, help path?); C-d
`GetShortPathName` ×21, `SetCurrentDirectory` ×21, `GetDriveType` ×26; C-e
the 3 `FindFirstChangeNotification` tokens after 058. Also: `ShellExecute`
×4, `SHFileOperation` ×3, `GetOpenFileName`/`GetSaveFileName` (core callers —
the plugin-exposed ones are P5's S4), `CreateProcess` ×7,
`Set/GetEnvironmentVariable` ×12, `ExpandEnvironmentStrings` ×2,
`GetTempPath` ×2, `LoadLibrary` ×24 (plugin/DLL paths under a non-ASCII
install dir), `ExtractIconEx` ×5, `SHGetFileInfo` ×8, `SHGetPathFromIDList`
×6, `SHGetFolderPath` ×7.

**Ledger**: L01–L05, L08–L11, L33, L34, L37, L66, L67, L69–L74, L86, L88.
**Contracts**: B7.1–B7.4 (058), B8.1–B8.5 (062), B9.5 (063).

## P2 — UI text sinks & composition

**Boundaries**: B3, B4. **Classes**: DC-03, DC-04, DC-05, DC-06, DC-07,
DC-08, DC-18, DC-19. **Queues**: `candidates/dc03-05-19-loadstr-compositions.txt`,
`dc06-ansi-ui-sinks.txt`, `dc06b-message-token-sinks.txt`, `dc08-tooltips.txt`,
`dc18-missed-twins.txt`, `suppressions.txt`. **Files**: `src/dialogs*.cpp`,
`fileswn*.cpp`, `gui.cpp`, `finddlg*.cpp`, `pwdmngr.cpp`, `shellsup.cpp`,
`mainwnd*.cpp`, `viewer3.cpp`, `stswnd.cpp`, `msgbox.cpp`, `menu*.cpp`,
`toolbar*.cpp`, `filesbx2.cpp`, `codetbl.cpp`, `plugins2.cpp`.

**Method**: (a) every printf-family composition with a `LoadStr(` format:
does any argument carry UTF-8 (name, path, `NumberToStr`/`PrintDiskSize`,
`LoadStrU8`, `GetErrorText`, plugin text) and where does the result go
(`SalMessageBox` = all-or-nothing; `Sal*U8` sink = falls back to raw `A`;
`CTruncatedString`; a static text)? A composition that mixes is defective
even when the sink "tolerates" it (DC-19). Also check the **reverse mix**:
a `LoadStrU8` template with an ANSI-only argument (`LoadStr`-derived
strings, `A`-API results). (b) every ANSI UI sink call: what value reaches
it — UTF-8 (defective), translated `LoadStr` text (verified-correct-ANSI
chain, unless the same control elsewhere receives UTF-8), ASCII literal;
message-token sites (`WM_SETTEXT`, `CB_ADDSTRING`, `LVITEM`…) follow the
window's Unicode flag — dialogs here are ANSI unless `UnicodeWnd`. (c) the
DC-18 list: for each id that has both `LoadStrU8(IDS_X)` and `LoadStr(IDS_X)`
callers, is the remaining `LoadStr` site composed with UTF-8 (defective
twin) or genuinely ANSI-only (verified, say why)? `IDS_QUESTION` (43 sites)
and `IDS_ERRORTITLE` are captions — decide once, list all. (d) tooltips:
`TTN_NEEDTEXT`/`TOOLINFO` without `W` — what text is composed? (e) the four
suppressions (L12–L15): re-check each premise; L14 claims a `FindFirstFile`
result is ANSI — since 004/066 `SalFindFirstFile` returns WTF-8, is the
premise still true at that site (which enumeration API does it use)?

**Ledger**: L06, L12–L16, L19, L22, L30, L31, L51, L52, L79, L80, L85.
**Contracts**: B3.1, B3.5 (010), B4.6 (041 information-line), B5.1,
B5.4, B5.5 (042), B6.2 (052), B11.2 (067).

## P3 — Converter & measurement machinery (cross-cutting)

**Boundary**: X. **Classes**: DC-10, DC-11, DC-12, DC-13, DC-14, DC-15,
DC-16, DC-20 (+ DC-02/DC-09 at the machinery level). **Queues**:
`candidates/converters.txt`, `dc13-strict-probe.txt`, `dc15-signed-char.txt`.
**Files** (line-level in full): `src/common/salunicode.cpp/.h`,
`salfileio.cpp/.h`, `salpath.cpp/.h`, `salclip.cpp`, `winlib.cpp` (1030-1250),
`src/gui.cpp` (`CStaticText`, tooltip text, `CButton`), `tooltip.cpp`,
`stswnd.cpp`, `msgbox.cpp`, `salamdr4.cpp` (`CTruncatedString`, plurals,
clipboard), `salamdr2.cpp` (`LoadStr*`, `DoExpandVarString`), `salamdr1.cpp`
(`InitLocales`, `NumberToStr`, `PointToLocalDecimalSeparator`), `salamdr6.cpp`
(`PrintDiskSize`, registry facade), `fileswn0.cpp`, `fileswn4.cpp`,
`fileswn5.cpp` (quick rename), `sort.cpp`.

**Method**: for every `SalU8ToW*`/`SalWToU8*` call in `converters.txt`:
buffer sizing (≤3 bytes per WCHAR, ≤1 WCHAR per byte + terminator), the
failure branch (what happens on 0/NULL: legacy fallback? silent use of an
indeterminate buffer — DC-20/L08? different behavior than a lossless
fallback — DC-09?), offset bookkeeping (byte vs WCHAR — DC-11), truncation
points (mid-sequence cuts — DC-12), lenient conversions on operational
values (DC-14), raw probes (DC-13), byte widening (DC-10), signed-char tests
and the `IsAlpha`/`IsNotAlphaNorNum`/`LowerCase`/`UpperCase` byte tables and
their consumers on names (DC-15 — seed C-f), dialogs using wide setters
without `UnicodeWnd` (DC-16: enumerate `UnicodeWnd` users vs. dialogs with
name fields). Seed C-g: the raw `MB_ERR_INVALID_CHARS` site(s). L07: the
`fileswn8.cpp` recycle-bin guard. L21: how a non-ASCII date/time format
would flow (`SalGetDateFormatU8` callers and their buffers).

**Ledger**: L07, L08 (with P1), L21, L23–L25, L28, L82.
**Contracts**: B2.1–B2.4 (005), B3.2–B3.4, B3.7 (010), B4.1–B4.3, B4.5
(041), B5.3 (042), B9.3, B9.4 (063), B10.1–B10.4 (066), B11.1 (067), B12.2.

## P4 — Configuration, clipboard & external channels

**Boundaries**: B5, B7. **Classes**: DC-06 (clipboard), DC-17, DC-01 (registry
rows). **Queues**: the `Reg*` rows of `candidates/dc01-*.txt`,
`candidates/registry-old-wrappers.txt`, plus your own grep for
`CopyTextToClipboard(` callers, `TRACE_`/log writers of names, command-line
composition (`execute.cpp`, `mainwnd4.cpp` user menu, `pack*.cpp` are P1's for
the API call — you own what text is *composed* into the command line),
`Set/GetEnvironmentVariable` values, bug-report writer. **Files**:
`src/mainwnd2.cpp` (config load/save), `icncache.cpp`, `regwork.cpp`,
`drivelst.cpp` (registry parts), `shiconov.cpp` (registry parts),
`salmoncl.cpp`, `bugreprt.cpp`, `salamdr4.cpp` (clipboard), `salamdr6.cpp`
(facade), `mainwnd4.cpp`, `packers.cpp`, `dialogsp.cpp`, `edtlbwnd.cpp`,
`config*.cpp` if present.

**Method**: registry — which values carry names/paths/free text vs. ASCII
keys/numbers; every raw `RegQueryValueEx`/`RegSetValueEx`/old `SalRegQueryValue(Ex)`
that can carry a name is a DC-01 candidate (Contract B3.6 says the facade is
mandatory); the facade itself is P3's. DC-17: every persisted translated or
user-editable string outside `CPluginData` — custom packer/unpacker/archiver
titles (`packers.cpp:734` `DupStr`; seed S5/L17), viewer/editor
associations, user-menu items, hot-path names (047), history rings, filter
masks — producer encoding vs. consumer sink. Clipboard: every remaining
`CopyTextToClipboard(` (ANSI) caller — is the text UTF-8 (defective) or
genuinely ANSI? Logs/command lines: names composed into `TRACE_*`, bug
reports, external editor/viewer command lines, user-menu commands — which
encoding reaches the external process (lossy allowed per 066 for text
channels, **not** for operational channels: command lines must carry the
true units — check `SalCreateProcess`/`SalShellExecuteEx` usage).

**Ledger**: L17, L27, L32, L77, L78, L83.
**Contracts**: B3.6 (010), B9.1, B9.2 (063).

## P5 — Plugin boundary

**Boundary**: B8. **Classes**: DC-05, DC-06, DC-07, DC-13 at the boundary;
FR-012 pre-classification. **Seeds**: S1–S10 (research R7). **Files**:
`src/plugins/shared/spl_gen.h`, `spl_base.h`, `spl_fs.h`, `spl_arc.h`,
`spl_view.h`, `spl_gui.h`, `spl_menu.h`, `spl_com.h`, `spl_vers.h`,
`splunicode.h`, `winliblt.cpp`; `src/zip.cpp` (`CSalamanderGeneral`
implementation — every `char*` text/name/path method), `src/plugins1.cpp`,
`plugins2.cpp`, `plugins3.cpp`, `src/packers.cpp`, `src/pluglegacy.h/.cpp`,
`src/plugins.h:2414-2423`; plugin sites: `src/plugins/mdview/viewer.cpp:257,
577,801`, `src/plugins/sftp/operats.cpp:24-43`, `src/plugins/ftp/ftputils.cpp:
3349-3436`, and the ledger sites L38–L46; per enabled plugin (`plugins.cfg`)
only the places it consumes names/paths/numbers from the core or hands text
back — not its internals.

**Method**: for every core→plugin service: what encoding does it accept/
return today (UTF-8 by contract / ANSI / tolerant probe) and is that
documented in the SDK header? For every plugin→core intake: is it
normalized (`SalLegacyToU8Alloc`), assumed UTF-8, or treated as ANSI, and
what do enabled plugins actually pass (check 2–3 representative plugins per
intake: e.g. `GetCurrentPath` in ftp/sftp/regedt, `AddCustomPacker` titles
in the packers that register them, `SetActionShowHint`, `SalMessageBox`
with plugin `LoadStr` text)? For S1–S10 answer CONFIRMED/REFUTED with the
concrete plugin-visible failure (which plugin, which action, which
language). For each plugin-internal defective site (L38–L46 and new ones)
pre-classify FR-012: user-visible in a shipped configuration? local to the
plugin? enumerable regression surface (list the consumers)?

**Ledger**: L29, L35, L38–L50, L59–L65, L81, L89.
**Contracts**: B1.1, B1.2 (004), B4.4 (041), B5.2 (042), B6.1 (052),
B7.4 (058 `GetFileIcon`), B10.5 (066), B11.3 (067), B12.1.

## P6 — User input & the unreleased delta

**Boundary**: B6 + line-level review of `delta-manifest.md`. **Files**:
`src/common/winlib.cpp` (`CTransferInfo::EditLine`/`EditLineW`),
`src/fileswn0.cpp` (quick search `WM_CHAR`), `editwnd.cpp` (command line),
`finddlg*.cpp` (masks, search text), `dialogs4.cpp` (hot-path entry),
`fileswn5.cpp` (in-place rename), the rename/new-folder/change-directory
dialogs (`dialogs3.cpp`), `filter*.cpp`/`masks.cpp` if present; then
**every hunk** of `git diff v0.1.4..HEAD -- src tools` (features 065–067)
and `git diff v0.1.1..v0.1.2 -- src` (052–055, encoding lens only).

**Method**: input — how typed text enters (`WM_GETTEXT` A vs `GetWindowTextW`,
`WM_CHAR` on ANSI vs Unicode windows), where it is stored (UTF-8?), and how it
is compared/matched against stored UTF-8 names (masks: byte-wise on UTF-8 —
case-folding via byte tables? DC-15/S2 shape in core); IME/dead-key input of
non-ACP characters. Delta — a general regression lens, not only encoding:
lifetimes and `free()` on all paths, buffer sizes (WTF-8 3 bytes/unit),
failure branches, thread affinity, the 066 converter changes' effect on
every caller (`SalU8ToW` now accepts `ED A0 80..ED BF BF` — any caller that
assumed rejection?), the 067 `u8` parameter default at every caller, the
`viewer3.cpp` wide tooltip, the `zip.cpp:6566` change, the mdview WebView2
keeper/host changes (065) for path handling only.

**Ledger**: L26, L84, L87.
**Contracts**: B5.6 (042 type-to-search/sort/clipboard follow stored names).

## P7 — Guard & test designer (produces rules and tests, no product findings)

**Inputs**: `tools/check_encoding.py` (with the `DRAFT_RULES` added in T008),
`candidates/guard-draft.txt`, `src/saltests/saltests.cpp`
(`TestEncodingReview068`), the other perspectives' findings as they arrive.
**Deliverables** (written to `findings/P7.md` and applied in T021/T028 by the
main context): for each of DC-01, DC-02 (A→W), DC-08, DC-13, DC-14, DC-15,
DC-18 the rule definition (regex/window/exemptions), its false-positive
analysis on the current tree (which hits are legitimate and how they get
annotated), and the proof plan (which pre-fix site it must flag); for
classes only testable at runtime, the saltests property check (in the style
of `TestNumberCompositionEncoding`, `saltests.cpp:953`). Also: audit the
guard's own blind spots (headers never scanned; `wide_fallback()`
suppressing DC-16's legacy branch; the `UTF8_IDENT` list vs. identifiers the
inventory shows carrying UTF-8) and propose minimal, precise widenings —
the build must stay at 0 findings.
**Ledger**: L18, L20, L36, L53–L58, L68, L75, L76.
**Contract**: B6.3.

---

## Verifier (one fresh agent per findings batch)

You receive: a batch of Findings (`F-…` records with sites, class, scenario,
data path) and read access to the repository. **Your charter is to refute
them.** For each Finding:

- Reproduce the data path in the code: open every `file:line` in the chain,
  confirm the value really is UTF-8 (or really is ANSI) at each step, confirm
  the sink really behaves as claimed (read the sink's implementation, not its
  name), confirm the scenario is reachable in a **shipped** configuration
  (8 enabled languages, Czech/Hungarian translations for non-ASCII words —
  check `translations/<lang>/<module>.slt` for the actual string when the
  claim depends on a word being non-ASCII; the 3 disabled languages do not
  count).
- Verdict **CONFIRMED** only with: the exact failure scenario (surface,
  language/locale, what the user sees / which operation fails) and the
  `file:line` evidence chain. Verdict **REFUTED** with the evidence why it
  cannot happen (value provably ASCII, sink actually wide, path unreachable,
  configuration not shipped → say `latent` instead of refuted when the only
  obstacle is a non-shipping configuration).
- Never accept "looks mixed" or "probably UTF-8": trace it.
- Output: `findings/verdicts-<batch>.md` with one block per Finding:
  `F-id · CONFIRMED|REFUTED|LATENT · scenario · evidence · notes`.
- Read-only on the product.

## Regression reviewer (one fresh agent per fix or per-file fix batch)

You receive: the diff (`git diff` of the fix), the Fix record (finding,
affected-surface list the fixer enumerated), and read access. **Your charter
is to find a regression.** Steps:

1. Re-enumerate the consumers of every changed symbol/resource/control
   yourself (`rg` the identifiers; do not trust the fixer's list) and
   compare; missing consumers are a defect of the record.
2. For each affected surface: **unchanged** (prove the bytes/behavior are the
   same for every input that worked before — ASCII, English UI, valid-ACP
   plugin input) / **corrected** (the defect's scenario now renders right) /
   **regressed** (any input that worked before now differs — including
   error-handling paths, buffer sizes with 3-byte WTF-8 units, `free()` on
   all paths, thread context).
3. Byte-identity: English UI / ASCII output identical (ASCII `LoadStrU8 ==
   LoadStr`; W call on ASCII == A call); plugin-facing services identical
   (`src/zip.cpp` forwarders, `src/plugins/shared/*.h` diff documentation-only,
   `LAST_VERSION_OF_SALAMANDER` unchanged).
4. Previously validated behavior: which 066/067 (and 058/062/063) quickstart
   scenarios touch the changed code; state whether the change can alter them.
5. Per-item path? (listing, sorting, icon/overlay reading, per-name
   conversion) — then the Fix record must carry the timing numbers; flag if
   missing.
6. Verdict **ACCEPTED** (no regressed surface; record complete) or
   **REJECTED** (name the regressed surface or the missing evidence).
- Output: `findings/regression-<fix-id>.md`. Read-only on the product.
