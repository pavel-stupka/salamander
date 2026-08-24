# Encoding Inventory — sites per boundary, class sweeps, contract compliance, ledger

**Feature**: 068-encoding-regression-review · **Started**: 2026-08-24
**Method**: research.md R3 (three triage tiers), R4 (perspectives P1–P7).
**Reference**: [contracts/encoding-contract-checklist.md](contracts/encoding-contract-checklist.md)
(DC-01…DC-20, contracts B1–B12, ledger L01–L89). Candidate work queues:
`candidates/*.txt` (Tier-1 lines; every line must be accounted for here —
individually or inside a listed group).

**Classification vocabulary** (data-model Site): `verified-correct` /
`defective` (→ Finding) / `latent` (non-shipping configuration named) /
`out-of-scope` (reason). **Tier-2 groups**: one row per pattern-in-function
with every `file:line` listed in Location. **Evidence**: one line.

Row shape (all boundary tables):

| ID | Location | Pattern | Data | Classification | Evidence | Persp. | DC | Finding |

---

## B1 — Disk → application (listings, names, paths, link targets)

| ID | Location | Pattern | Data | Classification | Evidence | Persp. | DC | Finding |
|---|---|---|---|---|---|---|---|---|

## B2 — Application → Windows (file, shell, icon, overlay, change-monitoring, Recycle Bin, launch, drag-and-drop)

| ID | Location | Pattern | Data | Classification | Evidence | Persp. | DC | Finding |
|---|---|---|---|---|---|---|---|---|

## B3 — Language module → screen (translated UI text to sinks)

| ID | Location | Pattern | Data | Classification | Evidence | Persp. | DC | Finding |
|---|---|---|---|---|---|---|---|---|

## B4 — Composition (translated text + names / numbers / dates / plugin text)

| ID | Location | Pattern | Data | Classification | Evidence | Persp. | DC | Finding |
|---|---|---|---|---|---|---|---|---|

## B5 — Application → external (clipboard, file lists, logs, external programs, command lines, environment)

| ID | Location | Pattern | Data | Classification | Evidence | Persp. | DC | Finding |
|---|---|---|---|---|---|---|---|---|

## B6 — User input → application (rename, path, mask, command, search fields)

| ID | Location | Pattern | Data | Classification | Evidence | Persp. | DC | Finding |
|---|---|---|---|---|---|---|---|---|

## B7 — Application ↔ saved configuration (registry facade, both directions)

| ID | Location | Pattern | Data | Classification | Evidence | Persp. | DC | Finding |
|---|---|---|---|---|---|---|---|---|

## B8 — Application ↔ plugins (names/paths handed over, plugin text shown, shared services)

| ID | Location | Pattern | Data | Classification | Evidence | Persp. | DC | Finding |
|---|---|---|---|---|---|---|---|---|

## X — Cross-cutting machinery (converters, sinks, facades, measurement, probes)

| ID | Location | Pattern | Data | Classification | Evidence | Persp. | DC | Finding |
|---|---|---|---|---|---|---|---|---|

---

## Inventory summary (filled at consolidation, T017)

| Boundary | verified-correct | defective | latent | out-of-scope | Sites/groups | Perspectives |
|---|---|---|---|---|---|---|
| B1 | | | | | | |
| B2 | | | | | | |
| B3 | | | | | | |
| B4 | | | | | | |
| B5 | | | | | | |
| B6 | | | | | | |
| B7 | | | | | | |
| B8 | | | | | | |
| X | | | | | | |

---

## Defect-class sweep (FR-003 / SC-002)

| DC | Shape (short) | Queue | Owner | Status | Notes |
|---|---|---|---|---|---|
| DC-01 | ANSI API on a UTF-8 path | `candidates/dc01-ansi-fs-shell-process-registry.txt` | P1 (+P4 registry) | pending | |
| DC-02 | CP_ACP conversion on UTF-8 / wide name | `candidates/dc02-cp-acp.txt` | P1, P3 | pending | |
| DC-03 | ANSI template + UTF-8 name → message box | `candidates/dc03-05-19-loadstr-compositions.txt` | P2 | pending | |
| DC-04 | ANSI template + name → caption | (guard rule; P2 by reading) | P2 | pending | |
| DC-05 | ANSI template + UTF-8 number | `candidates/dc03-05-19-loadstr-compositions.txt` | P2 | pending | |
| DC-06 | UTF-8 value → ANSI UI sink | `candidates/dc06-ansi-ui-sinks.txt`, `dc06b-message-token-sinks.txt` | P2 (+P4 clipboard) | pending | |
| DC-07 | dead `LVN_GETDISPINFOW` | (guard rule; P2 by reading, P5 plugins) | P2, P5 | pending | |
| DC-08 | ANSI tooltip handler | `candidates/dc08-tooltips.txt` | P2 | pending | |
| DC-09 | ANSI producer → strict probe/facade | `candidates/converters.txt` (failure branches) | P1, P3 | pending | |
| DC-10 | Latin-1 byte widening | (P3 by reading) | P3 | pending | |
| DC-11 | byte offset as WCHAR offset | `candidates/converters.txt` | P3 | pending | |
| DC-12 | byte-width measure/truncate | (P3 by reading: `CTruncatedString`, `DoExpandVarString`, clamps) | P3 | pending | |
| DC-13 | raw strict probe rejects WTF-8 | `candidates/dc13-strict-probe.txt` | P3 | pending | |
| DC-14 | lossy lenient conversion at intake | (P3 by reading: `WideCharToMultiByte(CP_UTF8, 0`, `SalU8ToWDisplay*` uses) | P3 | pending | |
| DC-15 | signed-char name byte | `candidates/dc15-signed-char.txt` | P3 | pending | |
| DC-16 | ANSI dialog down-converts wide | (P3/P2 by reading: `UnicodeWnd` users) | P3 | pending | |
| DC-17 | undefined encoding of cached string | (P4 by reading: `packers.cpp`, `dialogsp.cpp`, `edtlbwnd.cpp`, `regwork.cpp` consumers) | P4 | pending | |
| DC-18 | missed twin of a fixed site | `candidates/dc18-missed-twins.txt` | P2 | pending | |
| DC-19 | U8-sink fallback masks a composition | `candidates/dc03-05-19-loadstr-compositions.txt` | P2 | pending | |
| DC-20 | converter conflates too-long / invalid | `candidates/converters.txt` (fallback branches) | P3 | pending | |

---

## Contract compliance (FR-004)

| Obligation | Contract | Verdict | Evidence / Finding | Persp. |
|---|---|---|---|---|
| B1.1 plugin-ABI names UTF-8; plugin UI converts to UTF-16 | 004 plugin-interface-vnext | pending | | P5 |
| B1.2 manifest `longPathAware`, no `activeCodePage` | 004 app-manifest | pending | | P5 |
| B2.1 program→control W message; control→program wide + convert back | 005 ui-text | pending | | P3 |
| B2.2 GDI measure/draw wide; no surrogate split | 005 ui-text | pending | | P3 |
| B2.3 invalid UTF-8 → legacy ANSI fallback (no `?`, no drop) | 005 ui-text | pending | | P3 |
| B2.4 name-validation loops unsigned | 005 ui-text | pending | | P3 |
| B3.1 no UTF-8 to `A` text API (C1) | 010 display-conversion | pending | | P2 |
| B3.2 mandatory legacy fallback (C2) | 010 | pending | | P3 |
| B3.3 WCHAR units for measure/truncate/offset (C3) | 010 | pending | | P3 |
| B3.4 convert on change not paint (C4) | 010 | pending | | P3 |
| B3.5 actioned values from stored UTF-8 (C5) | 010 | pending | | P2 |
| B3.6 registry only via `SalReg*W8` (C6) | 010 | pending | | P4 |
| B3.7 no UNICODE flip, no ACP manifest, ASCII unchanged (C7) | 010 | pending | | P3 |
| B4.1 locale text valid UTF-8 from acquisition (C-1) | 041 locale-text | pending | | P3 |
| B4.2 only U8 locale wrappers; Group-C exemptions recorded (C-2) | 041 | pending | | P3 |
| B4.3 separators ≥16-byte UTF-8, byte lengths (C-4) | 041 | pending | | P3 |
| B4.4 plugin `NumberToStr`/`PointToLocalDecimalSeparator` UTF-8, ABI unchanged (C-5) | 041 | pending | | P5 |
| B4.5 strict for operations, lenient display-only (C-6) | 041 | pending | | P3 |
| B4.6 information line: independent fields, one U+FFFD, all languages incl. Find, no split | 041 information-line | pending | | P2 |
| B5.1 composition with a name uses `LoadStrU8` (§1) | 042 composed-message | pending | | P2 |
| B5.2 forbidden to change `SalMessageBox`/`CMessageBox`/`LoadStr` semantics (§4) | 042 | pending | | P5 |
| B5.3 malformed → lenient display, never written back (§5) | 042 | pending | | P3 |
| B5.4 `NF_REQUERY` from `WM_INITDIALOG` for name-carrying list views (§1) | 042 notification-format | pending | | P2 |
| B5.5 ANSI dispinfo handler never `CP_ACP` (§3) | 042 | pending | | P2 |
| B5.6 type-to-search/sort/clipboard follow stored names (§4) | 042 | pending | | P6 |
| B6.1 `CPluginData` translated fields valid UTF-8 always; producers normalize | 052 plugin-metadata | pending | | P5 |
| B6.2 consumers use UTF-8 sinks; tolerant fallback not an excuse | 052 | pending | | P2 |
| B6.3 `UTF8_IDENT` tracks metadata identifiers; build fails without python | 052 | pending | | P7 |
| B7.1 panel paths/names UTF-8; convert + W for every API hand-off | 058 path-pipeline | pending | | P1 |
| B7.2 invalid UTF-8 → legacy fallback, never fail the operation | 058 | pending | | P1 |
| B7.3 no ANSI shell/file API on panel-derived paths | 058 | pending | | P1 |
| B7.4 offsets per encoding; `GetFileIcon` unchanged | 058 | pending | | P1, P5 |
| B8.1 classification chain UTF-8, no MAX_PATH truncation, ASCII byte-identical (C1) | 062 delete-pipeline | pending | | P1 |
| B8.2 bin vetoed only for removable/remote/CD/RAM; fail-safe (C2) | 062 | pending | | P1 |
| B8.3 link only when `IsReparseTagNameSurrogate` (C3) | 062 | pending | | P1 |
| B8.4 recycle paths wide from UTF-8 (C4) | 062 | pending | | P1 |
| B8.5 one Debug TRACE at the gate (C5) | 062 | pending | | P1 |
| B9.1 file list UTF-8 end to end; per-sink conversion (C1) | 063 filelist-text | pending | | P4 |
| B9.2 `CopyTextToClipboard` CP_ACP forever; core never passes UTF-8 (C2) | 063 | pending | | P4 |
| B9.3 tooltip text UTF-8 at rest; always drawn wide; CP_ACP fallback in `SetText` (C3) | 063 | pending | | P3 |
| B9.4 width modifiers in code points; `varPlacements` bytes (C4) | 063 | pending | | P3 |
| B9.5 facade calls on the feature path; `SalGetTempFileName` W (C5) | 063 | pending | | P1 |
| B10.1 WTF-8 encoder total; decoder strict for non-WTF-8 | 066 name-encoding-wtf8 | pending | | P3 |
| B10.2 display converter one-way | 066 | pending | | P3 |
| B10.3 validity probes via `SalU8ToW`; strict non-name sites marked reviewed | 066 | pending | | P3 |
| B10.4 byte-structural guarantees; sort never loses items | 066 | pending | | P3 |
| B10.5 plugin-shared helpers strict by design (recorded) | 066 | pending | | P5 |
| B11.1 formatters always valid UTF-8; `PrintDiskSize(u8=FALSE)` frozen | 067 number-format | pending | | P3 |
| B11.2 formatter output never composed with `LoadStr`, never to ANSI sink | 067 | pending | | P2 |
| B11.3 plugin API boundary frozen, interface 106 | 067 | pending | | P5 |
| B12.1 `plugins.h` metadata contract of record | plugins.h:2414-2423 | pending | | P5 |
| B12.2 `salunicode.h` converter contract block | salunicode.h:6-48 | pending | | P3 |

---

## Ledger re-examination (FR-005)

Full text of every row: contract checklist Part C. Disposition vocabulary:
`still-open` (→ Deferred item with fresh justification) / `closed-by-<feature>` /
`fix-candidate F<n>` / `by-design` (justification re-affirmed) /
`latent` (re-enable checklist).

| ID | Location (short) | Re-examination note | Disposition | Persp. |
|---|---|---|---|---|
| L01 | `fileswn8.cpp:125` `SetCurrentDirectory` | | pending | P1 |
| L02 | `drivelst.cpp:1481` | | pending | P1 |
| L03 | `MyGetVolumeInformation` | | pending | P1 |
| L04 | `QueryDosDevice`/`ResolveSubsts` | | pending | P1 |
| L05 | `drivelst.cpp:1110-1121,1739` volume labels | | pending | P1 |
| L06 | `mainwnd3.cpp:5324` split-bar tooltip | | pending | P2 |
| L07 | `fileswn8.cpp` recycle-bin signed-char guard | | pending | P3 |
| L08 | `snooper.cpp` ×3 too-long vs invalid | | pending | P1, P3 |
| L09 | `snooper.cpp`/`fileswn1.cpp` `MB_PRECOMPOSED` | | pending | P1 |
| L10 | `geticon.cpp:361-368` uninitialized buffer | | pending | P1 |
| L11 | `geticon.cpp` MAX_PATH icon path | | pending | P1 |
| L12 | `dialogs6.cpp:645` share name suppression | | pending | P2 |
| L13 | `mainwnd3.cpp:2842` config name suppression | | pending | P2 |
| L14 | `dialogs2.cpp:909` `.slg` name suppression | | pending | P2 |
| L15 | `fileswn5.cpp:2764` quick-rename fallback | | pending | P2 |
| L16 | `msgbox.cpp` `MSGBOXEX_PARAMS` asymmetry | | pending | P2 |
| L17 | cached packer/unpacker titles | | pending | P4 |
| L18 | global `LoadStr` conversion | | pending | P7 |
| L19 | central `WM_NOTIFYFORMAT` | | pending | P2 |
| L20 | `tserver/tablist.cpp:748` | | pending | P7 |
| L21 | non-ASCII date/time format never observed | | pending | P3 |
| L22 | size dialogs / archive browsing (041) | | pending | P2 |
| L23 | UTF-16 viewer rendering | | pending | P3 |
| L24 | viewer caret/selection approx. | | pending | P3 |
| L25 | CJK double-width | | pending | P3 |
| L26 | typing surrogates in rename | | pending | P6 |
| L27 | lossy external text channels | | pending | P4 |
| L28 | CESU-8 idempotence | | pending | P3 |
| L29 | ftp log caret byte offsets | | pending | P5 |
| L30 | `filesbx2.cpp:230,270` header titles `TextOutA` | | pending | P2 |
| L31 | `mainwnd3.cpp:5077` / `viewer3.cpp:561` tooltips | | pending | P2 |
| L32 | `mainwnd4.cpp` `fileName[MAX_PATH]` | | pending | P4 |
| L33 | Google Drive gating dead code | | pending | P1 |
| L34 | Explorer Status column | | pending | P1 |
| L35 | `ResolveLocalPathWithReparsePoints` ABI | | pending | P5 |
| L36 | `activeCodePage` manifest | | pending | P7 |
| L37 | MAX_PATH components/roots | | pending | P1 |
| L38 | ftp `dialogs6.cpp:377-379` | | pending | P5 |
| L39 | ftp `fs4.cpp:325-327`, `operats1.cpp:1210` | | pending | P5 |
| L40 | ftp `operatsb.cpp:1140` | | pending | P5 |
| L41 | dbviewer sinks | | pending | P5 |
| L42 | regedt `finddlg.cpp:410` | | pending | P5 |
| L43 | zip overwrite dialogs `WM_SETTEXT` | | pending | P5 |
| L44 | filecomp blank title | | pending | P5 |
| L45 | regedt `fs4.cpp:479` `CQuadWord` printf | | pending | P5 |
| L46 | regedt `finddlg2.cpp:932` dead dispinfow | | pending | P5 |
| L47 | 8 plugins never runtime-verified | | pending | P5 |
| L48 | `Spl*` helpers strict UTF-8 | | pending | P5 |
| L49 | plugin file-op UI long paths | | pending | P5 |
| L50 | 005 E6/E9 plugin sweep never completed | | pending | P5 |
| L51 | French `octetss` | | pending | P2 |
| L52 | ru/uk/zh `PrintDiskSize` sites | | pending | P2 |
| L53 | `addrows.py:45` + stale `sftp.slt` | | pending | P7 |
| L54 | `addrows.py:81` | | pending | P7 |
| L55 | `relayout.py:79` | | pending | P7 |
| L56 | `layout.py:196` | | pending | P7 |
| L57 | layout validator red | | pending | P7 |
| L58 | ui-overrides pins | | pending | P7 |
| L59 | `plugins1.cpp:2184` unbounded sprintf | | pending | P5 |
| L60 | `plugins1.cpp:2166` strcpy | | pending | P5 |
| L61 | sftp `dialogs.cpp:1131` | | pending | P5 |
| L62 | sftp `session.cpp:503` TOCTOU | | pending | P5 |
| L63 | sftp `session.cpp:718` | | pending | P5 |
| L64 | sftp `dialogs.cpp:54` zeroize | | pending | P5 |
| L65 | sftp `lang.rc2:128` accelerators | | pending | P5 |
| L66 | `shiconov.cpp:856-868` SEH leak | | pending | P1 |
| L67 | overlay worst case | | pending | P1 |
| L68 | migrate utility | | pending | P7 |
| L69 | `LoadLibrary("cldapi.dll")` | | pending | P1 |
| L70 | `CloudSyncPendingIndex` | | pending | P1 |
| L71 | `shexreg.h:218` IPC struct | | pending | P1 |
| L72 | 027 §Bounded truncations | | pending | P1 |
| L73 | archive path buffers | | pending | P1 |
| L74 | 027 §External limits | | pending | P1 |
| L75 | harness anomaly | | pending | P7 |
| L76 | `finddlg1.cpp:3865` clang-format | | pending | P7 |
| L77 | hot-path config quirks | | pending | P4 |
| L78 | jump-list icon | | pending | P4 |
| L79 | 042 sites not individually triggered | | pending | P2 |
| L80 | 043 sites / drag image | | pending | P2 |
| L81 | plugin dialogs never run | | pending | P5 |
| L82 | non-CE legacy code page | | pending | P3 |
| L83 | 063 GUI scenarios | | pending | P4 |
| L84 | 066 GUI walk / config round trip | | pending | P6 |
| L85 | 067 English capture | | pending | P2 |
| L86 | SHIFT+DEL smoke | | pending | P1 |
| L87 | 056 encoding perspective gap | | pending | P6 |
| L88 | 058 screenshots | | pending | P1 |
| L89 | 041 plugins / locale partials | | pending | P5 |
