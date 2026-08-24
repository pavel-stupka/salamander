# Verdicts — batch V6 (P5, the plugin boundary)

Verifier charter: refute. A finding survives only where the data path
reproduces in the actual code, in a **shipped** configuration
(19 plugins `on` in `plugins.cfg`; 8 enabled languages — ru/uk/zh are `off`).

Established facts used throughout: core built **without `UNICODE`** and with
`/J`; interface 104 makes every `char*` name/path crossing the boundary UTF-8
(WTF-8 since 066); `GetErrorText()` returns UTF-8 (`src/salamdr2.cpp:206-232`);
feature 067 deliberately froze the plugin-facing bytes of `PrintDiskSize` /
`NumberToStr` / `ExpandPluralBytesFilesDirs` / `PointToLocalDecimalSeparator`.

---

## F-P5-01 · CONFIRMED

**Scenario.** Plugin **regedt** (`regedt=on`), any UI language, any ACP.
Panel on `reg:\HKEY_CURRENT_USER\Software\…`; a key or value whose name
contains non-ASCII characters (e.g. `Můj klíč`, `Přezdívka`). Right-click the
item → **Copy Name** / **Copy Full Name** (both in the FS context menu,
`fs3.cpp:867-881`), or right-click the panel path → **Copy Path**
(`fs3.cpp:950-960`). Paste anywhere: `MÅ¯j klÃ­Ä` instead of `Můj klíč`.
Both clipboard formats are wrong — `CF_UNICODETEXT` because the UTF-8 bytes
are widened through `CP_ACP`, `CF_TEXT` because the raw UTF-8 bytes are
published as if they were ACP.

**Evidence (every step re-read).**
- Producer is UTF-8, not ANSI: `src/plugins/regedt/fs2.cpp:505,555`
  `file.Name = DupStrA(name)` where `name` is the `RegEnumKeyExW` /
  `RegEnumValueW` WCHAR buffer; `regedt/utils.cpp:78-91` `DupStrA` →
  `utils.h:51-56` `WStrToU8` → `WideCharToMultiByte(CP_UTF8, …)`. The header
  comment states the intent explicitly ("every name/path crossing the plugin
  interface is UTF-8 (interface 104)"), and `WStrToStr` (CP_ACP) is kept
  separately for the plugin's own A dialogs — so the two encodings are
  deliberately distinct here and the UTF-8 one is the one that reaches
  `fd->Name`.
- Same for the path variants: `regedt/fs2.cpp:64-75` `GetCurrentPath` builds
  the wide path and converts with `WStrToU8` (UTF-8), and
  `fs3.cpp:1023` appends `fd->Name` (UTF-8) with `SG->SalPathAppend`.
- Sinks: `fs3.cpp:1016`, `:1025`, `:1036` all call
  `SG->CopyTextToClipboard(...)`.
- The service is genuinely the ACP one: `src/zip.cpp:2418-2428`
  `CSalamanderGeneral::CopyTextToClipboard` → `::CopyTextToClipboard`
  (`src/salamdr4.cpp:1246`) → `CopyHTextToClipboard` (`:1290`) →
  `AddUnicodeToClipboard` (`:1020`) → `:1027`
  `MultiByteToWideChar(CP_ACP, 0, str, textLen, NULL, 0)` for
  `CF_UNICODETEXT`, and `:1316` `SetClipboardData(CF_TEXT, hGlobalText)`
  publishes the caller's untouched bytes.
- A correct route exists at the same ABI level: `src/salamdr4.cpp:1144`
  `CopyTextToClipboardW` exported as `CSalamanderGeneral::CopyTextToClipboardW`
  (`src/zip.cpp:2431-2440`).

**FR-012 scope.** User-visible in a shipped configuration: **yes**.
Local to the plugin: **yes** — the fix is 3 call sites in
`src/plugins/regedt/fs3.cpp` (`SplU8ToWAlloc` + `CopyTextToClipboardW`, the
pattern `dbviewer/renmain.cpp:1330` already uses for its Unicode branch); no
core change and no ABI change needed. Regression surface enumerable: the three
regedt sites only; nothing else consumes their result.

**Notes.** The P5 claim that the defect is data-dependent and
UI-language-independent is correct. P5's inventory of the other
`CopyTextToClipboard` callers in enabled plugins was spot-checked and matches:
`checksum/dialogs.cpp:1190` (hex digest, ASCII), `filecomp/viewtext.cpp:324`
and `viewwnd3.cpp:858` (raw file bytes, CP_ACP by design), `ftp` log/password
text, `pictview/dialogs.cpp:1701` (EXIF). Only the regedt trio is provably
UTF-8.

---

## F-P5-02 · CONFIRMED IN PART (mechanism real; no demonstrable user-visible failure)

**What is confirmed.** The exported case services really are raw ACP byte
tables applied to UTF-8, and the two cited call sites really do feed them a
UTF-8 name:
- `src/zip.cpp:887-896` `CSalamanderGeneral::ToLowerCase` is literally
  `*toLow = LowerCase[*toLow];` in a byte loop; `:898-907` `ToUpperCase` the
  same; `:878-885` `GetLowerAndUpperCase` hands the two raw `BYTE[256]` tables
  to the plugin.
- The tables are ACP-derived, not ASCII: `src/common/str.cpp:109-116`
  `InitializeCase()` fills them with `CharLowerA`/`CharUpperA` over 0..255.
  On CP1250, `LowerCase[0xC5] == 0xE5`, `LowerCase[0x8A] == 0x9A`, etc. — i.e.
  UTF-8 lead **and continuation** bytes are rewritten independently.
- `/J` means `char` is unsigned, so there is no negative-index UB; the byte
  really is used as 0..255.
- regedt: `src/plugins/regedt/fs3.cpp:386-392` builds `uniqueFileName`, then
  `SG->SalPathAppend(uniqueFileName, file.Name, …)` where `file.Name` is UTF-8
  (`fs2.cpp:505,555` → `utils.cpp:78-91` `DupStrA` → `WideCharToMultiByte(CP_UTF8,…)`),
  then `SG->ToLowerCase(uniqueFileName)`.
- undelete: `src/plugins/undelete/fs2.cpp:1434-1443` — same shape, and its
  names are UTF-8 too (`fs2.cpp:452` `DupStr(di->FileName->FNName)` ←
  `library/miscstr.cpp:247-258` `String<char>::NewFromUnicode` →
  `WideCharToMultiByte(CP_UTF8, …)`, with the interface-104 comment).

**What is refuted / not demonstrated — the claimed consequence.** The folded
string is a **pure lookup key**, never a path and never displayed:
`zip.cpp:5511` passes `uniqueFileName` to `DiskCache.GetName(...)`, and the
only thing the cache does with it is `strcmp` in a binary search
(`src/cache.cpp:408-443` `CCacheDirData::GetNameIndex`); the *file* on disk is
named from the separate `nameInCache` argument (`fs3.cpp:398-401`
`ReplaceUnsafeCharacters(fileName)` / `undelete/fs2.cpp:1446` `file.Name`).
So "the key stops being valid UTF-8" has no consumer that cares.
The two observable effects are both benign or unreachable:
1. The *intended* case-insensitive folding silently does not happen for
   non-ASCII names (`Ř` = `C5 98` → `E5 98`, `ř` = `C5 99` → `E5 99` — still
   two keys for one registry item, which the registry considers one name).
   Consequence: one redundant temp copy. Not user-visible.
2. A genuine collision needs two sibling items whose UTF-8 bytes differ only
   where the ACP table folds — e.g. `Ċ` (`C4 8A`) and `Ě` (`C4 9A`) both fold
   to `E4 9A`, `Č` (`C4 8C`) and `Ĝ` (`C4 9C`) both to `E4 9C`. Real, but it
   requires a Maltese/Esperanto letter as a sibling of a Czech one. I could
   not construct a plausible shipped scenario.

**Verdict rationale.** The finding is correct as a **contract defect**
(DC-15: a documented-UTF-8 value passed through an ACP byte table) and correct
that `spl_gen.h` never says these services are ACP-only. It is not correct that
it produces a user-visible failure today. Treat as a contract/documentation
fix candidate, not a shipped-product regression.

**FR-012 scope.** User-visible in a shipped configuration: **no** (see above).
Local: **no** — `ToLowerCase`/`ToUpperCase`/`GetLowerAndUpperCase` are
exported vtable methods. Enumerable surface (enabled plugins), re-checked
independently: `regedt/fs3.cpp:392` (UTF-8, folded), `undelete/fs2.cpp:1443`
(UTF-8, folded), `undelete/fs1.cpp:74` (`AssignedFSName` + `":"` — ASCII,
safe), `ftp/ftp.cpp:314` (captures the tables), `pictview/utils.cpp:71,74`
(drive letters — ASCII, safe).

**Notes.** P5's severity note about `%ls` on the *same* line is the more
reachable defect of the two and I confirm its premise:
`regedt/fs3.cpp:386-388` formats the wide `CurrentKeyName` through
`SalPrintf` → `regedt.cpp:172-180` `_vsnprintf_s(..., _TRUNCATE, ...)`, a
narrow CRT printf, so a registry key name outside the process's narrow-locale
repertoire is lossily converted or truncates the key. Unlike the case-table
issue this *can* collapse distinct keys onto one cache entry. It is a
different site/class from the one F-P5-02 asserts and should be raised
separately rather than folded into this verdict.

---

## F-P5-03 · CONFIRMED (with a corrected — and stronger — mechanism, and corrected sites)

**Scenario.** Plugin **ftp** (`ftp=on`), any UI language, **any** Windows whose
ACP is not us-ascii (i.e. every shipped one). *Connect → Server Type →
Import…* (`ftp/dialogs3.cpp:258`) or *Export…* (`:183`), *Servers → Export…*
(`dialogs2.cpp:311`), *Save log as…* (`ctrlcon2.cpp:1998`), *Test of parsers →
load raw listing* (`dialogs4.cpp:917`). Browse to any path containing a
non-ASCII character — `C:\Users\Jiří\servers.stp` is enough; it does **not**
have to be outside the ACP. The operation fails outright ("cannot open the
file" / the file is never written), even though the user picked a file that
exists.

**Evidence.**
- The service is the ANSI entry point: `src/salamdr6.cpp:1699`
  `BOOL ret = GetOpenFileName(lpofn);` and `:1720` `GetSaveFileName(lpofn);`,
  with `LPOPENFILENAME` = `OPENFILENAMEA` (core built without `UNICODE`).
  Forwarders `src/zip.cpp:4739`ff.
- The dialog therefore hands back **ACP** bytes. Every ftp call site then
  feeds those bytes to a **strict UTF-8** consumer:
  `ftp/dialogs3.cpp:270` / `dialogs4.cpp:928` / `ctrlcon2.cpp:2011` /
  `dialogs2.cpp:324` / `dialogs3.cpp:195` `FTPCreateFileU8(fileName, …)` →
  `ftp/ftputils.cpp:3349-3352` `WCHAR* w = SplU8ToWExtAlloc(fileName); if (w
  == NULL) { SetLastError(ERROR_INVALID_NAME); return INVALID_HANDLE_VALUE; }`
  → `src/plugins/shared/splunicode.h:33` `MultiByteToWideChar(CP_UTF8,
  MB_ERR_INVALID_CHARS, …)`. A CP1250 `ř` (`0xF8`) is not valid UTF-8, so the
  converter returns NULL and the operation aborts with `ERROR_INVALID_NAME`.
  Same for the `FTPSetFileAttributesU8` pre-call on the save paths.

**Corrections to the finding as written.**
1. The claimed failure mode ("returns the path best-fit-converted, on an ACP
   that cannot represent a character the path contains `?`") is the *weaker*
   half. In ftp the ACP→UTF-8 mismatch bites first and bites on every
   non-ASCII character, ACP-representable or not. P5's own example path
   (`Jiří`) is fully CP1250-representable and would have been harmless under
   the mechanism P5 described; it fails under the real one.
2. `src/salamdr6.cpp:1744` is `SafeGetSaveFileNameW` — there is **no**
   `SafeGetOpenFileNameW` anywhere in the core (`grep` over `src/` excluding
   `src/plugins/`: `consts.h:1708`, `salamdr6.cpp:1744`, one caller
   `dialogs.cpp:1840`). The internal W coverage is narrower than claimed.
3. `ftp/dialogs3.cpp:258` is **not** a private-key browse (ftp has no key
   auth); it is *Server Type → Import*, filter `IDS_SRVTYPEFILEFILTER`.
4. The caller inventory is wrong in both directions. Enabled-plugin call
   sites are 15, not 7: `dbviewer/renmain.cpp:234`,
   `filecomp/dialogs.cpp:260`, `ftp/{ctrlcon2.cpp:1998, dialogs2.cpp:311,
   dialogs3.cpp:183, dialogs3.cpp:258, dialogs4.cpp:917}`,
   `pictview/saveas.cpp:727`, `regedt/utils.cpp:621,625`,
   `renamer/utils.cpp:715,719`, `zip/{dialogs.cpp:1460, dialogs.cpp:1703,
   dialogs2.cpp:1187}`. But two of those are **not** defective:
   - `regedt/utils.cpp:621,625` are the documented A **fallback** of a
     W-first implementation — `utils.cpp:531-593` already drives
     `GetOpenFileNameW`/`GetSaveFileNameW` with a feature-010 comment naming
     exactly this defect ("SG->SafeGet*FileName is ANSI and loses names
     outside the ANSI code page").
   - `zip/dialogs.cpp:1460-1464` explicitly converts the ANSI result
     (`DlgAToU8(fileNameA, fileNameU8, …)` → `SetDlgItemTextU8`), i.e. a
     consistent ANSI chain that only loses genuinely non-ACP characters.
   `filecomp/dialogs.cpp:261` puts the result straight into an ANSI
   `SetDlgItemText` (consistent for display; its later use as a path was not
   traced here).

**FR-012 scope.** User-visible in a shipped configuration: **yes**. Local to
the plugin: **yes** per site — each of the 5 ftp sites can move to
`GetOpenFileNameW`/`GetSaveFileNameW` + `SplWToU8`, which is exactly what
`regedt/utils.cpp:531`, `checksum/dialogs.cpp:975`, `pictview/render1.cpp:232`
and `sftp/dialogs.cpp:1196` already do. Regression surface enumerable: the 5
ftp call sites and their `initDir`/`ImpExpInitDir` statics. A boundary fix
(adding `SafeGetOpenFileNameW`/`SafeGetSaveFileNameW` to `CSalamanderGeneral`)
would bump `LAST_VERSION_OF_SALAMANDER` and is out of scope, as P5 says.

**Notes.** `src/plugins/shared/spl_gen.h:3025-3032` really does document the
two methods without any encoding statement — confirmed. The SDK sample
(`demoplug/viewer.cpp:1115-1118`) already tells authors to bypass them.

---

## F-P5-04 · BY-DESIGN (contract-amendment question, not a defect)

**What is confirmed.** `src/plugins/shared/splunicode.h` really is strict in
both directions and at every entry point: `:33,38` `SplU8ToWAlloc`
(`MB_ERR_INVALID_CHARS`), `:47,52` `SplWToU8Alloc` (`WC_ERR_INVALID_CHARS`),
`:62` `SplWToU8`, `:74` `SplU8ToW`, and `:98` `SplU8ToWExtAlloc` inherits the
strictness through `SplU8ToWAlloc`. A WTF-8 lone-surrogate sequence
(`ED A0 80`…) is rejected → NULL / 0. That part of the finding reproduces
exactly.

**Why this is not a defect.** Feature 066's own contract states this behavior
as an explicit design decision, in the *Boundary notes* section of
`specs/066-fix-surrogate-filenames/contracts/name-encoding-wtf8.md:73-79`:

> **Plugin ABI**: unchanged (interface version untouched). Names crossing the
> plugin ABI as `char*` may now carry WTF-8 sequences where they previously
> carried `U+FFFD` substitutions; **plugin-shared helpers keep strict UTF-8
> converters, so plugins treat such names as they treated the broken names
> before — no new capability is promised to plugins by this feature.**

Checklist row B10.5 ("Plugin-shared helpers stay strict by design (L48)")
therefore does **not** contradict the code — it records it. P5's framing of
B10.5 as "stating the opposite" is the one part I reject: B10.5 states exactly
what the code does.

**And the change is not a regression, in either direction.** Before 066 the
same file reached the plugin as a `U+FFFD`-substituted name: `SplU8ToWAlloc`
*succeeded* and the plugin then operated on a path that does not exist
(`CreateFileW` → `ERROR_FILE_NOT_FOUND`) — or, worse, created a real file
under the substituted name. After 066 the plugin fails closed at the
converter. For every input that worked before, behavior is byte-identical
(WTF-8 == UTF-8 for all valid Unicode); for the surrogate population the
outcome went from "silently wrong" to "cleanly refused". Nothing that worked
was lost.

**Correction to the count.** 219, not 228, `Spl*` converter call sites in the
19 enabled plugins (`SplU8ToWAlloc|SplWToU8Alloc|SplWToU8(|SplU8ToW(|SplU8ToWExtAlloc`):
ftp 37, undelete 27, zip 26, pictview 19, renamer 16, checksum 12, regedt 10,
uncab 10, dbviewer 9, diskmap 9, uniso 9, filecomp 8, sftp 8, mdview 5, tar 5,
7zip 4, folders 3, peviewer 2, portables 0.

**FR-012 scope.** Not applicable — no defect to scope. If the project later
*decides* to relax the helpers, note that it is a contract amendment to B10.5
and to the 066 boundary note, it requires rebuilding every `.spl`, and it must
preserve the "reject every other malformed input" half of B10.1 (the 004/063
"valid UTF-8, else ANSI" heuristics depend on it — see the core's own
`SalU8ToW`).

**Notes.** The `filecomp/mainwnd.cpp:2043-2046` `: L""` fallback that P5 cites
here as a consequence is a genuine defect, but it is a defect of *filecomp's
failure branch*, not of the converter's strictness — the blank title also
occurs for the far more common mixed-encoding input. See F-P5-09.

---

## F-P5-05 · LATENT (invariant hole; not reachable with the shipped plugin set)

**Confirmed mechanism.** `src/plugins1.cpp:584-589` `CSalamanderConnect::
AddCustomPacker` passes `title` straight to `PackerConfig.SetPacker(...)`, and
`src/packers.cpp:734` `data->Title = DupStr(title);` is a raw copy. Same for
`AddCustomUnpacker` (`plugins1.cpp:610-616` → `CUnpackerConfig::SetUnpacker`).
No `SalLegacyToU8Alloc` on either. The contrast P5 draws is real: the *other*
metadata intakes in the same file do normalize —
`plugins1.cpp:1605,1611,1619,1625,1631` (`SetBasicPluginData`, with the
feature-052 comment quoted verbatim), `:1244` (`ChDrvMenuFSItemName`), `:1919`
(menu item names). A full grep of `SalLegacyToU8Alloc` over `src/*.cpp` +
`src/common/*.cpp` returns exactly those sites plus the two `gui.cpp` tooltip
intakes — the packer/unpacker titles are the only metadata intake left out.

**Why LATENT.** I independently re-enumerated every `AddCustomPacker` /
`AddCustomUnpacker` caller in the tree and every one that ships passes an
ASCII string literal: `7zip/7zip.cpp:612,613` `"7-Zip (Plugin)"`,
`tar/tardll.cpp:279` `"TAR (Plugin)"`, `uncab/uncab.cpp:344` `"UnCAB
(Plugin)"`, `uniso/uniso.cpp:400` `"UnISO (Plugin)"`, `zip/main.cpp:512,513`
`"ZIP (Plugin)"`. The remaining callers — `unchm/unchm.cpp:246`,
`unmime/unmime.cpp:211`, `unole/unole2.cpp:174`, `unrar/unrar.cpp:270` — are
in plugins that are `off` in `plugins.cfg` (and are ASCII literals anyway).
There is no shipped path by which a non-ASCII title reaches
`CPackerConfigData::Title`. P5's own "latent" pre-classification is correct.

**FR-012 scope.** Not user-visible in a shipped configuration. Local: yes —
two lines in `src/plugins1.cpp`. Enumerable surface: the two
`CSalamanderConnect` methods; consumers are `CPackerConfig`/`CUnpackerConfig`
persistence and the Pack/Unpack dialog combos (P4's area). Note the caveat P5
adds is right: the `defaultExtension` / `masks` arguments are ASCII by nature
and must not be touched.

---

## F-P5-06 · CONFIRMED as a documentation gap — but it is a **Note, not a Finding**

**Confirmed.** Re-read the header: `src/plugins/shared/spl_fs.h:242-244`
`GetCurrentPath`, `:246-250` `GetFullName`, `:252-261` `GetFullFSPath`,
`:263-265` `GetRootPath`, `:322-332` `ListCurrentPath` all describe buffer
sizes and semantics in Czech and state no encoding for any `char*` path.
`src/plugins/shared/spl_com.h:204-210` (`CFileData::Name`) is indeed the only
place in the SDK that says UTF-8, and it names the 004 contract document.
The second half also holds: there is no normalizing intake on any FS path —
the complete `SalLegacyToU8Alloc` inventory in the core is
`plugins1.cpp:1244,1605,1611,1619,1625,1631,1919` (metadata) and
`gui.cpp:896,2020` (tooltips); none of them is on an FS path method.

**Why it is not a Finding.** Charter rule 7: a Finding needs a failure
scenario. There is none here — the defect is that a *hypothetical future*
plugin author would have no statement to rely on. Every FS plugin that ships
today already produces UTF-8 on these methods and I verified the two that
matter: `regedt/fs2.cpp:64-75` (`GetCurrentPath` → `WStrToU8`),
`fs2.cpp:77-100` (`GetFullName` → `WStrToU8`), and `undelete` names come from
`library/miscstr.cpp:247-258` (`CP_UTF8`). No mis-encoded path reaches the
panel today.

**FR-012 scope.** No user-visible failure; the fix is comment-only in one SDK
header (no ABI, no binary change, no rebuild of plugins required). It is the
cheapest item in the batch and pairs naturally with the `spl_gen.h` gaps found
under F-P5-03 and F-P5-12.

---

## F-P5-07 · BY-DESIGN for `PrintDiskSize` (the 067 freeze) · CONFIRMED for the `NumberToStr` re-widening, which the freeze does **not** cover

**The `PrintDiskSize` half — BY-DESIGN.** The mechanism reproduces exactly as
P5 describes, and it is the documented state:
- `src/zip.cpp:1402-1405` `CSalamanderGeneral::PrintDiskSize(char* buf, const
  CQuadWord& size, int mode)` — three parameters, so `::PrintDiskSize`'s
  fourth argument takes its default `u8 = FALSE` (`src/consts.h:497`).
- `src/salamdr6.cpp:416-421` `char* (*loadStr)(int, HINSTANCE) = u8 ?
  LoadStrU8 : LoadStr;` → `:424-429` `ExpandPluralString(expanded, 200,
  loadStr(IDS_PLURAL_X_BYTES, NULL))` + `sprintf(buf, expanded,
  NumberToStr(num, size))`, where `NumberToStr` splices the UTF-8
  `ThousandsSeparator` (`src/salamdr1.cpp:137, 966-976`
  `SalGetLocaleInfoU8(LOCALE_STHOUSAND, …)`, `:2932-2951`).
- `src/consts.h:490-497` states the freeze in the source itself: "*Plugins via
  `CSalamanderGeneral` keep the ANSI default: their output bytes are frozen
  (several plugin sinks are genuinely ANSI and render the ANSI unit word
  correctly today).*" Checklist row B11.3 repeats it.

That is precisely the situation the batch instructions call by design: the
finding consists of the freeze being visible, so it is not a new defect.
For the record the ftp rendering is real and I verified every link:
`ftp/dialogs6.cpp:377-379` three `PrintDiskSize(…, 1)` calls → `:381`
`_snprintf_s(buf, _TRUNCATE, LoadStr(IDS_LOWDISKSPACEONTGTPATH), num1, num2,
num3)` with the Czech template non-ASCII (`translations/czech/ftp.slt:1809`
`11060,1,"V cílové cestě není dostatek volného místa.\nPožadované místo:
%s…"`) → `:383` `SetActionShowHint(buf)` → `src/gui.cpp:1340-1347` →
`SetToolTipText` → `gui.cpp:893-898` `SalLegacyToU8Alloc(text)`, which sees an
invalid-UTF-8 buffer and converts the **whole** thing from CP_ACP, turning the
UTF-8 `C2 A0` separator into `Â` + NBSP. The user sees `1Â 234Â 567 bajtů` —
character-for-character the defect in `specs/067-fix-drive-info-encoding/spec.md:20-24`.
Actionable only as a plugin-local mitigation or a future interface-version
service; not a core defect and not in this feature's scope.

**The `NumberToStr` half — CONFIRMED, and separable.** `NumberToStr` has no
`u8` flag to freeze: contract B11.1 says its output is **always valid UTF-8**,
and `zip.cpp:1397-1400` forwards it unchanged. So a plugin that takes that
output and re-widens it through `CP_ACP` is defective on its own terms, not
because of any freeze:
- `src/plugins/regedt/finddlg.cpp:409-410`
  `SG->NumberToStr(buf, CQuadWord().Set(Size, 0)); StrToWStr(buffer, 100, buf);`
  → `regedt/utils.h:33` `MultiByteToWideChar(CP_ACP, 0, sour, sourLen, dest,
  destSize)`. On any locale with a non-ASCII `LOCALE_STHOUSAND` the Size
  column of the Find-results list shows `Â` before every group separator.
  (Note the interaction with F-P5-10: if that finding holds, this column never
  renders at all, which would make this one unreachable in the same dialog.)
  Same shape at `finddlg.cpp:404` for the `KeyText` branch.

**FR-012 scope (for the confirmed half only).** User-visible in a shipped
configuration: yes on cs/fr/hu/pl-style locales, **if** the Find results list
renders (see F-P5-10). Local to the plugin: yes — `regedt/finddlg.cpp` should
use `SplU8ToW` on the `NumberToStr` result instead of `StrToWStr`. Regression
surface: the Find-results list columns only.

**Note on the rest of P5's site list.** `ftp/fs4.cpp:325-332` uses
`ExpandPluralBytesFilesDirs(…, TRUE)` on the bytes branch and hand-composes
only on the blocks branch; `dbviewer` and `ftp/operatsb.cpp` were not
re-traced — they belong to the frozen `PrintDiskSize` half unless they, like
regedt, re-widen a `NumberToStr` result through `CP_ACP`. That distinction —
frozen service vs. plugin re-widening a guaranteed-UTF-8 value — is the one
this finding must be split along before anything is fixed.

---

## F-P5-08 · CONFIRMED

**Scenario.** Plugin **zip** (`zip=on`), any UI language, any ACP. Two
surfaces:
(a) Pack (Alt+F5, ZIP) where a file inside the operation already exists —
    `zip/add.cpp:2346` → `COverwriteDialog` (IDD_OVERWRITE);
(b) the overwrite confirmation on the extract/view path —
    `zip/common.cpp:2246` → `COverwriteDialog2` (IDD_OVERWRITE2).
With a file named `Přehled.txt` the dialog's file field shows `PÅ™ehled.txt`
(UTF-8 bytes read as CP1250/CP1252). The name is data-dependent, so the
English UI shows it too.

**Evidence (each link re-read).**
- The value is UTF-8: `common.cpp:2246` `OverwriteDialog2(..., name, attr)` and
  `:2248` `SetFileAttributesU8(name, …)` on the very next line; `add.cpp:2335`
  and `:2346` use `TempName` with `CreateCFile`, and `CZipCommon::CreateCFile`
  (`common.cpp:766-814`) opens through `CreateFileU8(fileName, …)`. Both
  arguments are therefore UTF-8 by construction.
- The sink is genuinely ANSI: `zip/dialogs.cpp:1839-1840` and `:1925-1926`
  `SendDlgItemMessage(Dlg, IDC_FILE, WM_SETTEXT, 0, (LPARAM)File);`. Plugins
  are built **without** `UNICODE` (`src/plugins/shared/vcxproj/plugin_base.props:18`
  defines only `_MT;WIN32;_WINDOWS;_USRDLL`), so this is `SendDlgItemMessageA`.
  The dialogs are created by `DialogBoxParam` = `DialogBoxParamA`
  (`dialogs.cpp:1795` IDD_OVERWRITE, `:1886` IDD_OVERWRITE2), and the static is
  subclassed with `SetWindowLongPtr` = the **A** variant
  (`dialogs.cpp:138-145` `CDlgRoot::SubClassStatic`), so the control stays an
  ANSI window: no A→W translation happens, the raw UTF-8 bytes are stored and
  drawn as ACP.
- The plugin already ships the correct helper and uses it everywhere else in
  the same file: `zip/common.cpp:337-345` `SetDlgItemTextU8` (→ `SplU8ToWAlloc`
  + `SetDlgItemTextW`), used at `dialogs.cpp:376,391,420,449,471,477,788,1417,
  1464`. Only these two dialogs' `OnInit` bypass it.

**FR-012 scope.** User-visible in a shipped configuration: **yes**, in all 8
languages (data-driven, not language-driven). Local to the plugin: **yes** —
4 lines in `src/plugins/zip/dialogs.cpp`, replaced by the plugin's own
`SetDlgItemTextU8`. Regression surface enumerable: `COverwriteDialog::OnInit`
and `COverwriteDialog2::OnInit` only; both statics are subclassed by
`TextControlProc`, which must keep working with the text set through the W
setter (it stays an ANSI window; `SetDlgItemTextW` on an ANSI window converts
to ACP for storage — so the fix should also consider making the two statics
Unicode, or accept the same ACP loss the rest of the plugin already has).
This is the strongest FR-012 candidate in the batch.

**Note.** The companion `IDC_FILEATTR` `WM_SETTEXT` on the same lines carries
`attr` from `GetInfo()`, which formats a date/time/size — worth checking for
the same class, but it was not traced here.

---

## F-P5-09 · CONFIRMED (mechanism and effect), **REFUTED on the language list**

**Scenario.** Plugin **filecomp** (`filecomp=on`). Compare two files
(Files → Compare Files, or the plugin's Compare command). When the comparison
finishes, the compare window's **title bar goes completely empty** — and
stays empty for the progress updates too. Reproduces in **Czech, French,
Hungarian and Slovak**. Does *not* reproduce in English, German, Spanish,
Dutch or Romanian.

**Evidence.**
- `filecomp/mainwnd.cpp:2114-2126` builds `buf` with `_stprintf(buf, fmt, …)`
  where `fmt` is `LoadStr(IDS_MAINWNDHEADER…)` — ANSI (plugins build without
  `UNICODE`, `LoadStr` → `LoadStringA` → ACP bytes) — and the `%s` arguments
  are `SG->SalPathFindFileName(Path1/Path2)`, UTF-8 by interface 104.
- `:2135-2137` `WCHAR* wBuf = SplU8ToWAlloc(buf); SetWindowTextW(HWindow, wBuf
  != NULL ? wBuf : L"");` and the identical shape at `:2043-2046` for the
  progress title. `SplU8ToWAlloc` is strict
  (`src/plugins/shared/splunicode.h:33` `MB_ERR_INVALID_CHARS`), so a single
  ACP byte in `fmt` makes it return NULL, and the fallback throws the text
  away instead of degrading to `SetWindowTextA(HWindow, buf)`.
- The invalidity is provable per language. Czech
  (`translations/czech/filecomp.slt`): `1035` `"…Porovnání souborů - %d
  Rozdíl{|1|y|4|ů}"` — `á` = `0xE1` followed by `n` = `0x6E`, i.e. a 3-byte
  lead with no continuation byte → not valid UTF-8. Same for `1036, 1062,
  1063, 1064`. French `1035` `"…%d Différence{|1|s}"` (`é` = `0xE9` + `r`),
  Hungarian `1035` `"Fájl összehasonlító…"`, Slovak `1035` `"Porovnanie
  súborov…"` — all invalid.
- Introduced by `ebb27fb [004] Complete plugin ports to SDK 104`, i.e. live
  since 0.1.0.

**Refuted part.** P5 claims "every shipped language except English (cs, de,
fr, hu, nl, ro, sk, es)". I checked all eight `.slt` files for ids
1000/1035/1036/1062/1063/1064: **German** ("Dateivergleich - Berechne
Unterschiede"), **Spanish** ("Comparador de ficheros - Calculando
diferencias"), **Dutch** ("Bestand Vergelijker - Verschillen worden bepaald")
and **Romanian** ("Comparatorul de Fisiere - Fara Diferente", no diacritics)
are **pure ASCII** in every one of these strings, so their titles render
correctly. The real blast radius is 4 of 8 languages, not 7 of 8.
The `IDS_PLUGINNAME` fallback branch (`:2132`) is likewise non-ASCII only in
Czech (`"Porovnání souborů"`), Hungarian (`"Fájl összehasonlító"`) and Slovak
(`"Porovnanie súborov"`) — French/German/Spanish/Dutch/Romanian plugin names
are ASCII.

**FR-012 scope.** User-visible in a shipped configuration: **yes**. Local to
the plugin: **yes** — 6 lines in `src/plugins/filecomp/mainwnd.cpp`; the fix is
two-part exactly as P5 says (build `buf` from a UTF-8 string loader so the
composition stays valid, and make the fallback `SetWindowTextA(HWindow, buf)`
so text is never dropped). Regression surface enumerable: the two
title-setting sites; `buf` has no other consumer at either site.

**Note.** Contract B2/B3-C2's "never drop the text" rule is violated by the
`: L""` fallback independently of the mixed composition, so the fallback fix
is worth making even if the composition is fixed first.

---

## F-P5-10 · CONFIRMED (the flagged assumption is settled by static evidence in this repo; a 1-minute runtime check would clinch it)

**Scenario.** Plugin **regedt** (`regedt=on`), any UI language, any data.
Enter a registry path in the panel, invoke the plugin's **Find** command
(`fs3.cpp:1003-1010` spawns `CFindDialogThread`), run a search. The results
list fills with rows (the count and the "items found" status text update —
`LVN_ITEMCHANGED` is format-independent and its `UpdateStatusText()` handler
runs), but **every row is blank: no text in any column and no icon**.

**Evidence chain.**
1. The control is virtual with no other text source:
   `src/plugins/regedt/lang/lang.rc:170` `CONTROL "",IDC_RESULTS,
   "SysListView32", LVS_REPORT | LVS_SINGLESEL | **LVS_OWNERDATA** | …`;
   `finddlg2.cpp:228` `ListView_SetItemCountEx(...)`. A full grep of
   `finddlg.cpp`/`finddlg2.cpp` for `ListView_SetItem*`/`LVM_SETITEM`/
   `InsertItem` finds only `SetItemCount*` and `SetItemState` — nothing ever
   pushes item text.
2. The only display-info handler is the W one: `finddlg2.cpp:932`
   `case LVN_GETDISPINFOW:` (and it is written for W throughout —
   `finddlg.h:97` `LPWSTR GetText(int i, LPWSTR buffer)`, `finddlg.h:220`
   `WCHAR LVItemTextBuffer[100]`). There is no `LVN_GETDISPINFO(A)` case.
3. The dialog is an ANSI window: regedt compiles `..\..\shared\winliblt.cpp`
   (`regedt.vcxproj:128`) and `CFindDialog : CDialogEx : CDialog`
   (`regedt/dialogs.h:73`); `CDialog::Create()` is
   `src/plugins/shared/winliblt.cpp:448-453` `CreateDialogParam(...)`, which
   with no `UNICODE` define anywhere in regedt's project or headers is
   `CreateDialogParamA`. `DefDlgProcA` answers `WM_NOTIFYFORMAT`/`NF_QUERY`
   with `NFR_ANSI`.
4. Nothing overrides that: a grep for `NOTIFYFORMAT|NF_REQUERY|
   SETUNICODEFORMAT` across `src/plugins/` returns **no** hit in
   `winliblt.cpp` and **no** hit anywhere in regedt.

**Settling P5's flagged assumption without running the plugin.** The
assumption ("an ANSI dialog answers `WM_NOTIFYFORMAT` with `NFR_ANSI`") is
demonstrated *inside this repository* by two sibling plugins that use the
identical framework and identical build flags and had to work around exactly
this in feature 004:
- `checksum/dialogs.cpp:317-318` — "*the names are UTF-8 -> let the listview
  re-ask for the notification format (we want W, see WM_NOTIFYFORMAT)*" +
  `SendMessage(..., WM_NOTIFYFORMAT, (WPARAM)HWindow, NF_REQUERY);` and
  `:326-334` a `WM_NOTIFYFORMAT`/`NF_QUERY` handler returning `NFR_UNICODE`;
  the file header comment at `:32-34` states the rule outright.
- `renamer/rendlg.cpp:1650-1651, 1655` — the same pair, and
  `renamer/preview.h:55` `GetDispInfo(LV_DISPINFOW*) // W notifications, see
  CRenamerDialog WM_NOTIFYFORMAT`.
If the default already resolved to Unicode, both of those workarounds would be
dead code. They are not; they are the documented remedy for precisely the
condition regedt is missing. I therefore treat the assumption as settled and
the finding as CONFIRMED. The one thing static analysis cannot rule out is a
comctl32 v6 manifest quirk on this specific machine, so a 60-second manual
check (open regedt Find, run a search, look at the rows) is still worth doing
before the fix lands.

**About the "it's been broken since Open Salamander" objection.** P5 is right
that `git show 3945ecf:src/plugins/regedt/finddlg2.cpp` already contains
`case LVN_GETDISPINFOW:` at line 931 and no A handler, so this predates the
fork and every encoding feature. That is an argument about how long it has
been broken, not about whether it is broken; the Find dialog on a registry FS
is a rarely exercised surface.

**FR-012 scope.** User-visible in a shipped configuration: **yes** (all 8
languages, data-independent). Local to the plugin: **yes** — one
`WM_NOTIFYFORMAT` case plus one `NF_REQUERY` in `WM_INITDIALOG`, copied
verbatim from `checksum/dialogs.cpp:317-334`. Regression surface enumerable:
the one dialog (`CFindDialog`) and its one list control; the existing
`LVN_GETDISPINFOW` handler already does the right thing once it starts firing,
and the other notifications it handles (`NM_RCLICK`, `LVN_COLUMNCLICK`,
`LVN_ITEMCHANGED`, `LVN_KEYDOWN`) have no A/W split, so they are unaffected.

**Cross-reference.** If this is fixed, the second half of F-P5-07 becomes
reachable: `finddlg.cpp:404,409-410` widens `KeyText` and the `NumberToStr`
result through `CP_ACP` (`regedt/utils.h:33`), so the Name and Size columns
would then render with `Â` before each digit-group separator on cs/fr/hu-style
locales, and with mojibake for non-ASCII key names. Fix both together.

---

## F-P5-11 · CONFIRMED for (a) long paths · CONFIRMED IN PART for (b) lenient conversion

**Code confirmed verbatim.** `src/plugins/sftp/operats.cpp:24-27` `Utf8ToWide`
= `MultiByteToWideChar(CP_UTF8, **0**, s, -1, out, outCount) > 0` — no
`MB_ERR_INVALID_CHARS`; `:29-32` `WideToUtf8` = `WideCharToMultiByte(CP_UTF8,
**0**, …)` — no `WC_ERR_INVALID_CHARS`; `:35-43` `MakeLocalWidePath` calls
`Utf8ToWide` and adds **no** `\?\` prefix, returns `void` so callers cannot
detect failure (the CF-18 comment covers only the uninitialized-buffer case).
Nine `MakeLocalWidePath` call sites — `:181, 283, 425, 496, 506, 550, 564,
577, 628` — plus two `WideToUtf8` name conversions at `:524` and `:574`.
The ftp reference is exactly as cited: `ftp/ftputils.cpp:3349-3352`
`SplU8ToWExtAlloc` (strict + `\?\`, `splunicode.h:96-119`) with
`ERROR_INVALID_NAME` on failure.

**(a) Long paths — CONFIRMED, with one nuance P5 did not state.** The process
*is* long-path aware: `src/manifest.xml:46` `<longPathAware>true</longPathAware>`.
That is only half of what Windows requires — the machine-wide
`HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled` policy
must also be set, and it is off by default. So on a stock Windows 11:
download a remote file into a local directory deeper than `MAX_PATH` (which
the core reaches fine, feature 004/027 + `SalPathToWExtAlloc`), and
`GetFileAttributesExW`/`CreateFileW` at `:186,:195` fail with
`ERROR_PATH_NOT_FOUND`; the same for the upload enumeration
`FindFirstFileW(wpattern, …)` at `:509` and `:567`. ftp, using
`SplU8ToWExtAlloc`, works on the same path. The wide buffers being 4096 units
makes the omission look intentional and is what makes it look correct at a
glance — it is not the buffer that limits it.

**(b) Lenient conversion — CONFIRMED as a defect, but P5's stated consequence
is only right for part of the population.**
- For a path component supplied by the **core** as WTF-8 (feature 066's
  population — a lone surrogate in the local target path): `Utf8ToWide`
  substitutes `U+FFFD`, and that path does **not** exist on disk, so the
  operation fails with `ERROR_PATH_NOT_FOUND`. The result is a confusing
  failure, not "operating on the wrong path". P5's phrase "creating or
  overwriting a file named with `U+FFFD` instead of failing" does not hold for
  this case.
- It *does* hold for names the plugin creates itself from **server-supplied**
  bytes. `DownloadDir` (`:283`) calls `CreateDirectoryW(wdir, NULL)` on a path
  whose leaf comes from `SanitizeLocalName(e->Name, …)` (`:49-60` — it only
  replaces reserved characters, it does not validate UTF-8). An SFTP server
  that sends non-UTF-8 file names (common: latin-1 names from older Linux
  hosts) makes every non-ASCII byte collapse to `U+FFFD`, so `Müller` and
  `Möller` become the same local directory `M<U+FFFD>ller` and their contents
  merge/overwrite. That is real, silent, and a data risk — and it is a
  *bigger* problem than the surrogate case P5 argued.
- The `WideToUtf8` sites are lossy but fail closed in practice: `:574` in
  `DeleteLocalTree` converts a surrogate-bearing name to `U+FFFD` and then
  back (`:577` `MakeLocalWidePath`), so `DeleteFileW(wchild)` silently does
  nothing and the enclosing `RemoveDirectoryW(wdir)` (`:585`) then fails —
  i.e. on **Move**, the local source is left behind after a reported-successful
  upload. `:524` in `UploadDirRecursive` produces an `lpath` that does not
  exist, so `UploadOneFile` fails and the file is skipped rather than uploaded
  under a wrong name.

**FR-012 scope.** User-visible in a shipped configuration: **yes** for (a) on
any machine without the long-path policy; **yes** for (b) against a
non-UTF-8 SFTP server, and as confusing failures for the 066 population.
Local to the plugin: **yes** — three static helpers in one file. Regression
surface enumerable and small: the nine `MakeLocalWidePath` call sites
(`:181, 283, 425, 496, 506, 550, 564, 577, 628`) and the two `WideToUtf8`
uses (`:524, 574`). Routing both through `SplU8ToWExtAlloc` / `SplWToU8`
(`src/plugins/shared/splunicode.h`) fixes (a) outright and turns (b) into a
clean refusal. Note the signature change this implies: `MakeLocalWidePath`
currently returns `void`, so every call site must gain a failure branch — that
is the real cost, not the conversion itself. Also note the `"%s\*"` pattern
built at `:505` and `:565` before prefixing: with `\?\` the prefix must be
applied to the directory, not appended around the wildcard.

---

## F-P5-12 · CONFIRMED (mechanism, contract gap and user-visible failure) · **count REFUTED: 47 call sites, ~27 defective, in 5 plugins — not 127 in 19**

**Scenario (verified end to end).** Plugin **ftp** (`ftp=on`), **Czech UI on a
Czech Windows** (i.e. the system's `FormatMessage` text is also localized).
*Connect → Server Type → Export…* to a path the user cannot write: the message
box shows the Czech template correctly and the OS error text as mojibake
(`SystÃ©m nemÅ¯Å¾e nalÃ©zt uvedenou cestu.`). The **English UI hides it
exactly**, and so — I add — does a **Czech UI on an English Windows**.

**Mechanism, each link re-read.**
- `GetErrorText` is UTF-8: `src/salamdr2.cpp:201-232` — `FormatMessageW(...)`
  → `SalWToU8(wmsg, -1, u8msg, …)`, with the feature-010 comment that names
  this exact failure mode. The core documented the trap for its own sites and
  never told plugin authors.
- The SDK block is silent: `src/plugins/shared/spl_gen.h:1548-1555` documents
  buffer lifetime, size and thread-safety for `GetErrorText` and says nothing
  about encoding. Confirmed verbatim.
- The plugin template is ANSI: `LoadStr` in a plugin is `LoadStringA` from its
  `.slg`, i.e. ACP bytes.
- The sink is all-or-nothing: `ftp/dialogs3.cpp:210` `SalMessageBox(...)` →
  `CMessageBox`, whose body goes out through `src/msgbox.cpp:475`
  `SalSetDlgItemTextU8(HWindow, IDS_MSGBOX_TEXT, Text.Get())` and is measured
  at `:678` `WCHAR* bodyW = SalU8ToWAlloc(Text.Get()); … else
  DrawText(hDC, Text.Get(), …)` — one invalid byte drops the **whole** string
  to the narrow path.
- The templates really are non-ASCII in the shipped languages. Checked the
  actual `.slt` rows, not assumed:
  `ftp` `10459` — cs "Nelze exportovat typ serveru do souboru.\n\nChyba: %s",
  fr "Impossible d'exporter…\n\nErreur: %s", hu "A szervertípus nem
  exportálható fájlba…", sk "Nie je možné exportovať…" are non-ASCII; de, es,
  nl, ro are ASCII for this string. `regedt` `1040` is non-ASCII in cs, de,
  fr, hu, sk, es (ASCII in nl, ro). `tar` `11265` is non-ASCII in cs, de, hu,
  sk. So the affected language set is **per string**, typically 4–6 of 8.

**Four sampled sites, as the charter requires.**
1. `ftp/dialogs3.cpp:209-210` — `sprintf(buf, LoadStr(IDS_SRVTYPEEXPORTERROR),
   SalamanderGeneral->GetErrorText(err)); SalMessageBox(HWindow, buf, …)`.
   ANSI template + UTF-8 error → all-or-nothing sink. **Defective.**
2. `tar/fileio.cpp:37-40` — `strcpy(txtbuf, LoadStr(IDS_GZERR_FOPEN));
   strcat(txtbuf, SalamanderGeneral->GetErrorText(err));
   SalamanderGeneral->ShowMessageBox(txtbuf, …)` →
   `src/zip.cpp:470-485` → `SalMessageBox` → same sink. **Defective.**
3. `regedt/fs2.cpp:517` — `SalPrintf(buf1, 1024, LoadStr(IDS_ENUMKEY),
   SG->GetErrorText(err))` → `SalMessageBoxEx` with `mbp.Text = buf1`.
   **Defective** (and `1040` is non-ASCII in 6 of 8 languages).
4. `uniso/uniso.cpp:261` and `7zip/7zip.cpp:261` — the shared
   `Error`/`SysError` helper: `sprintf(buf, "%s\n\n%s", msg, GetErrorText(err))`
   where `msg` is the caller's `LoadStr(resID)` (7zip expands it with
   `vsprintf(msg, LoadStr(resID), arglist)` first). **Defective**, and each is
   a single choke point for many callers.
5. Counter-example proving the mechanism is composition, not the service:
   `pictview/render1.cpp:2947,2958`
   `SalMessageBox(HWindow, SalamanderGeneral->GetErrorText(err), …)` passes the
   UTF-8 text **alone**, so the buffer is valid UTF-8, the wide path succeeds
   and it renders correctly in every language. **Not defective.**

**The count is wrong.** I enumerated `->GetErrorText(` across the 19 enabled
plugins: **47** call sites, in 8 plugins (7zip 7, ftp 18, pictview 5, regedt 4,
tar 10, dbviewer 1, renamer 1, uniso 1). P5's 127 comes from grepping the
substring `GetErrorText`, which also catches two unrelated functions:
- `FTPGetErrorText` / `FTPGetErrorTextForLog` — ftp's **own** helper,
  `ftp/ftputils.cpp:943-962`, built on the **ANSI** `FormatMessage`, so it
  produces ACP bytes and composes *correctly* with ftp's ANSI `LoadStr`. 50 of
  ftp's 68 hits are these. Feeding them into the same fix would be a
  regression.
- `PVGetErrorText` / `PVMessage_GetErrorText` — the PictView library's error
  text (`pictview/pictview.h:13,45`, `PVMessage.h:34,139,142`); 39 of
  pictview's 44 hits.
Of the 47 real calls, the ones that compose with the plugin's own ANSI text
are ~27: ftp 10 (`ctrlcon2.cpp:2064,2078`, `dialogs2.cpp:348,358`,
`dialogs3.cpp:209,219,316,326`, `dialogs4.cpp:981,993`), tar 10
(`deb/deb.cpp:22`, `fileio.cpp:38,50,72`, `tardll.cpp:87`,
`untar.cpp:698,762,823,1838,1871`), regedt 3 (`fs2.cpp:517,632`,
`fs3.cpp:673`), 7zip 2 (`7zip.cpp:261`, `FStreams.cpp:43`), uniso 1
(`uniso.cpp:261`). The rest are either correct (pictview's two standalone
passes), diagnostic-only (`ftp/operats5.cpp:2063` `TRACE_E`), or need
per-site tracing (`ftp/ssl.cpp:228-368` seven sites copying into a buffer,
`regedt/utils.cpp:143`, `renamer/utils.cpp:49`, `pictview` three
caller-buffer forms, `dbviewer/parser.cpp:149`, `7zip/update.cpp:226`,
`7zip/7zip.cpp:407,443,1203` which hand it to the core's `DialogError` as a
*separate* argument and are therefore probably fine).

**FR-012 scope.** User-visible in a shipped configuration: **yes**, for a user
whose Windows *and* Tandem UI are both a non-ASCII language. Local: **no** as
a whole, but far more tractable than P5 suggests — 27 sites in 5 plugins, and
7zip/uniso/tar concentrate theirs in one or two helper functions
(`SysError`, `Error`, the `strcat` idiom), so the real edit count is closer to
a dozen. The cheapest correct fix per plugin is to load the *template* through
a UTF-8 loader so the composition stays valid UTF-8 (`plugins1.cpp:1584,2184`
made exactly this move in the core). The prerequisite is the contract
statement in `spl_gen.h:1548-1555` — pair this with F-P5-06.

---

## F-P5-13 · CONFIRMED (mechanism + reachability) · **REFUTED on the surface: `ftp/fs4.cpp:356` is the information line, not the panel Name column**

**Scenario.** Plugin **ftp** (`ftp=on`), any UI language. Set *Options →
Configuration → Panels → File name format* to anything except the default
("as on the disk"): Capitalize, lowercase, UPPERCASE, Explorer style, VC style
or "part of name lowercase". Browse an FTP server holding `ČESKÝ.TXT` (or any
non-ASCII name) and focus it: the **information line** at the bottom of the
panel shows the name as a run of replacement characters / mojibake instead of
the name.

**Mechanism confirmed, line by line.**
- `ftp/fs4.cpp:353-356`: `GetConfigParameter(SALCFG_FILENAMEFORMAT, …)` then
  `SalamanderGeneral->AlterFileName(formatedFileName, file->Name,
  fileNameFormat, 0, isDir)` with `file->Name` UTF-8 by interface 104.
- `src/zip.cpp:3129-3135` `CSalamanderGeneral::AlterFileName` →
  `::AlterFileName(tgtName, srcName, -1, format, changedParts, isDir)`.
- `src/salamdr2.cpp:1855` `AlterFileName`. The folding sites are exactly the
  nine P5 lists — I re-grepped `LowerCase[|UpperCase[` in the file and the
  hits inside `AlterFileName` are `:1921, 1927` (format 5 Explorer),
  `:1945, 1951` (format 1 capitalize), `:1966` (format 2 lower), `:1976`
  (format 3 upper), `:1998, 2004, 2010` (format 7) — plus the two probes at
  `:1897` and `:1907`. Every one is a raw `LowerCase[byte]`/`UpperCase[byte]`
  on the ACP tables from `src/common/str.cpp:109-116`.
- Corruption is real: `Č` = `C4 8C`; on CP1250 `LowerCase[0xC4] = 0xE4` and
  `LowerCase[0x8C] = 0x9C`, so `ČESKÝ` folds to `E4 9C 65 …` — `E4` is a
  3-byte lead followed by `9C` then `65` ('e'), which is not a continuation
  byte. The result is not valid UTF-8 and the display converters render it
  leniently.

**Reachability — narrower than "any UI language, browse a server".** Format
`0` and format `4` fall through to the `default:` case at
`src/salamdr2.cpp:2013-2019`, a plain `strcpy`/`memcpy` with no folding. The
shipped default is format 4: `src/dialogs4.cpp:280` `FileNameFormat = 4;
// as on the disk` in the `CConfiguration` constructor. So this needs a
deliberate, non-default setting — exactly as P5 says in its FR-012 note,
though the headline scenario does not mention it. Formats 1, 2, 3, 5, 6 and 7
all fold (6 is rewritten to 3 or 2 at `salamdr2.cpp:1861`).

**Refuted part — the surface.** `ftp/fs4.cpp:356` sits inside
`CFTPListingPluginDataInterface::**GetInfoLineContent**`, which composes the
panel's *information line* text (and hands it to `LookForSubTexts` at `:332`),
not the Name column. The panel **Name column** for a plugin FS is rendered by
the **core**, at `src/filesbx1.cpp:1936,2025` and `src/filesmap.cpp:119`
(`AlterFileName(formatedFileName, f->Name, -1, Configuration.FileNameFormat,
0, isDir)`) — the plugin is not involved. So the column corruption P5
describes is real but belongs to the core panel path (P2/P6's surface); what
the *plugin boundary* contributes is the ftp information line. That
distinction matters for the FR-012 call.

**FR-012 scope.** User-visible in a shipped configuration: **yes**, but only
with a non-default name-format setting. Local to the plugin: **no** —
`AlterFileName` is core machinery (`salamdr2.cpp:1855`) reached from six core
call sites (`filesbx1.cpp:1936,2025`, `filesmap.cpp:119`,
`execute.cpp:952,960,973`, `finddlg1.cpp:2782,2793,2806`, plus the
Change Case operation `fileswn6.cpp:1426,2381,3103`) as well as the exported
plugin service. Its regression surface is every panel item in every panel and
the **Change Case** file operation, which actually renames files on disk — a
fix there changes what gets written to the file system, not only what is
drawn. That is the highest-risk item in this batch and the fix record must
carry both the timing numbers (per-item path) and a Change Case round-trip
proof.

**Relationship to F-P5-02.** Correct as P5 states it: `ToLowerCase`/
`ToUpperCase` are the explicit exports of the same machinery, `AlterFileName`
the implicit one. But note the severity is inverted from F-P5-02: there the
folded value was a lookup key nobody reads, here it is displayed and, through
Change Case, written to disk.
