# Verdicts — batch V5 (file-system, shell & launch boundary)

Independent verifier, charter = **refute**. Read-only on the product; no build.
Findings verified: F-P1-01 … F-P1-28 from `findings/P1.md` (27 findings;
**F-P1-11 was withdrawn by its own author before submission** and is recorded
here as WITHDRAWN without an independent verdict).

Ground rules applied throughout:

- core built **without** `UNICODE` ⇒ every un-suffixed Win32 text API
  (`DeleteFile(`, `FindFirstFile(`, `GetTempPath(`, `SHGetFolderPath(`,
  `ExtractIconEx(`, `SHFileOperation(` …) is the **A** entry point;
  `HANDLES(…)` / `HANDLES_Q(…)` are debug bookkeeping shims and do not change
  which entry point is called;
- built with `/J` ⇒ plain `char` is unsigned;
- the `Sal*` facade (`src/common/salfileio.h`, `salpath.h`) takes **UTF-8/WTF-8**
  and fails with `ERROR_INVALID_NAME` when `SalU8ToWAlloc` rejects the input
  (`src/common/salfileio.cpp:239` → `src/common/salpath.cpp:260`);
- 8 shipped UI languages; ru/uk/zh disabled ⇒ a claim that rests only on those
  is **LATENT**. None of batch V5 rests on a UI language: every scenario here is
  driven by *path* bytes, not by translated text, so the UI language is
  irrelevant to all 27 findings. What matters instead is the **system ACP**
  (1250 on a Czech machine, 1252 on a Western one) and whether the path in
  question is ASCII.

Two reachability premises recur; both were checked once, here:

- **P-A — a non-ASCII `%TEMP%` is a shipped-configuration reality.**
  `%TEMP%` defaults to `%USERPROFILE%\AppData\Local\Temp`, and for a **local**
  Windows account the profile folder is the account name verbatim — `Jiří`,
  `Kovács`, `Łukasz`, `Müller` all produce a non-ASCII `%TEMP%`. (Microsoft-account
  profiles are truncated to ASCII-ish 5-char stems, so the exposure is not
  universal, and the user can also point TEMP anywhere, e.g. `D:\Dočasné`.)
  Conclusion: reachable, common in the cs/hu/pl/de user base the product ships
  translations for, **not** universal. Where a finding needs a *non-ACP* `%TEMP%`
  (a Greek/Cyrillic folder on a CP1250 machine) that is a much rarer
  configuration and is called out as such.
- **P-B — `SalGetTempFileName` really produces UTF-8 today.**
  `src/salamdr3.cpp:216-244`: for `path == NULL` it calls `GetTempPathW` /
  `GetSystemDirectoryW` and converts with `SalWToU8` (feature 063); the object
  itself is created through `SalCreateFile` / `SalCreateDirectory`
  (`:275`, `:285`), which are the UTF-8 facades — so a name that survives to
  the `strcpy(tmpName, tmpDir)` at `:279`/`:286` is UTF-8 **by construction**,
  not by assumption. For `path != NULL` the caller's path is copied verbatim
  and is UTF-8 by the 004 contract. Every "the producer is UTF-8" step in
  cluster A therefore holds.

---

## Cluster A — temp / cache under a non-ASCII `%TEMP%` (F-P1-01 … F-P1-04)

Shared mechanism, verified once:

1. Producer proven UTF-8 — premise **P-B** above.
2. `src/cache.cpp` sinks on that value, enumerated exhaustively
   (`grep -n 'SetFileAttributes\|DeleteFile\|RemoveDirectory\|FindFirstFile\|GetTempPath' src/cache.cpp`):
   `:117` `SetFileAttributes`, `:119` `DeleteFile`, `:349/:350` and `:472/:474`
   `SetFileAttributes`+`RemoveDirectory`, `:384` `FindFirstFile`,
   `:1192/:1204` `RemoveDirectory`, `:1152` and `:1461` `GetTempPath`,
   `:1470` `FindFirstFile` — **all A**. There is no W call anywhere in
   `cache.cpp`.
3. `RemoveTemporaryDir` / `RemoveEmptyDirs` (`src/salamdr3.cpp:988-1075`) are A
   end to end — `SetCurrentDirectory` `:1021`, `FindFirstFile` `:996`,
   `DeleteFile` `:1009`, `RemoveDirectory` `:1015/:1028` — with exactly one
   UTF-8 island inside them, `ClearReadOnlyAttr`
   (`src/salamdr5.cpp:1465-1477`, `SalGetFileAttributes`/`SalSetFileAttributes`).
4. Sink behaviour on UTF-8 input: the A entry point converts its argument from
   the **ACP**, so UTF-8 bytes for `ř` (`C5 99`) become two CP1250 characters;
   the resulting wide path does not exist ⇒ `ERROR_PATH_NOT_FOUND` /
   `ERROR_FILE_NOT_FOUND`. No exception, no message — the return value is
   discarded at every one of these call sites.

Consequence class for the whole cluster: **loss of function (disk leak), not
wrong text.** Nothing the user sees is mis-rendered; files simply stay behind.

---

## F-P1-01 · CONFIRMED (with one correction to the finding's own text)

### Scenario

Any shipped UI language, any ACP. Windows local account `Jiří` ⇒
`%TEMP% = C:\Users\Jiří\AppData\Local\Temp`. F3-view a file inside an archive,
or use any plugin viewer that goes through the disk cache. On exit (and on
every cache eviction) the extracted temp file **and** the whole `SAL####.tmp`
directory stay on disk. Repeat use grows `%TEMP%` without bound; there is no
error message and no UI symptom — only `TRACE_E("Unable to delete tmp-file …")`
in a debug build.

### Evidence chain

- `src/salamdr3.cpp:223-244` → `:275/:285` — `TmpName` is UTF-8 (premise P-B).
- `src/cache.cpp:507` `tmpFullName` → `CCacheData` ctor (`src/cache.cpp:31`)
  stores it in `TmpName`.
- `src/cache.cpp:104` `DWORD attrs = SalGetFileAttributes(TmpName);` — the
  **UTF-8 facade**, succeeds.
- `src/cache.cpp:117` `SetFileAttributes(TmpName, FILE_ATTRIBUTE_ARCHIVE);` and
  `:119` `DeleteFile(TmpName);` — **A**, fail. The same buffer reaches a UTF-8
  facade and an ANSI API fifteen lines apart; that internal contradiction is
  the finding's strongest evidence and it is real.
- `src/cache.cpp:349-350` (`~CCacheDirData`) and `:472-474`
  (`RemoveEmptyTmpDirsOnlyFromDisk`) — `SetFileAttributes` + `RemoveDirectory`
  on `Path`, **A**, fail. `Path` comes from `newDirPath`
  (`src/cache.cpp:1179`, `SalGetTempFileName(…, FALSE)`) via the
  `CCacheDirData` ctor at `:328-338`.

### Correction

The finding says *"`CCacheData::CleanFromDisk` reports success"*. It does not:
after the failed `DeleteFile`, the re-probe at `src/cache.cpp:121` finds the
file still present, so control falls through to `return FALSE`
(`src/cache.cpp:138`), and `~CCacheData` (`:61-71`) emits
`TRACE_E("Unable to delete tmp-file (it is probably locked by other
process).")`. The outcome is the same (the file stays) but the mechanism is a
*mis-diagnosed* failure, not a false success. The misdiagnosis is itself
notable: the only diagnostic the developer gets points at file locking, never
at encoding.

### Scope

Loss of function (unbounded temp-directory growth), silent. Not user-visible as
text. Reachable per P-A.

---

## F-P1-02 · CONFIRMED

### Scenario

Same account as F-P1-01. Every cached file gets its **own** new
`SAL####.tmp` directory instead of sharing one, because the reuse test compares
an ACP `%TEMP%` prefix against a UTF-8 stored path byte-wise. Combined with
F-P1-01 (nothing is ever removed) the result is one leaked directory per
cached file rather than one per session.

### Evidence chain

- `src/cache.cpp:1152` `if (!GetTempPath(MAX_PATH, sysTmpDir))` — **A** ⇒
  `sysTmpDir` is CP_ACP; `rootTmpPathExp = sysTmpDir` (`:1154`).
- `src/cache.cpp:1163-1164` → `Dirs[i]->ContainTmpName(tmpName, rootTmpPathExp,
  rootTmpPathExpLen, &canContainThisName)`.
- `src/cache.cpp:358-360`
  `if (rootTmpPathLen < PathLength && StrNICmp(Path, rootTmpPath, rootTmpPathLen) == 0)`
  — `Path` is the UTF-8 `SalGetTempFileName` output, `rootTmpPath` is the ACP
  `%TEMP%`. For `C:\Users\Jiří\…` the bytes diverge at `ř` (`C5 99` vs `F8`),
  so the comparison never matches and `*canContainThisName` stays FALSE
  (initialised at `:357`).
- Fall-through at `src/cache.cpp:1171-1179` creates a new directory every time.

### Scope

Loss of function (cache reuse defeated) + amplifies F-P1-01's leak. No message,
no wrong text. Note the byte-length mismatch is a second, independent reason the
prefix test fails: the ACP prefix is *shorter* than the UTF-8 one, so even a
lucky byte match would mis-align.

---

## F-P1-03 · CONFIRMED IN PART / REFUTED IN PART

### What is confirmed

`ClearTEMPIfNeeded` (`src/cache.cpp:1458-1533`) is an ANSI chain:
`:1461 GetTempPath` (A) → `:1470 FindFirstFile` (A) → `:1522
RemoveTemporaryDir(tmpDir)` / `:1528 SendMessage(hActivePanel,
WM_USER_FOCUSFILE, (WPARAM)"", (LPARAM)tmpDir)`.

- **Non-ACP `%TEMP%`** (e.g. a Greek or Cyrillic folder on a CP1250 machine):
  `GetTempPathA` substitutes `?` for the unrepresentable characters, the
  subsequent `FindFirstFileA` finds nothing, and leftover `SAL*.tmp`
  directories are never offered for deletion. Confirmed — but this is the rare
  configuration, not the common `Jiří` one.
- **The "Focus" button is genuinely broken for a merely non-ASCII `%TEMP%`**:
  `tmpDir` is ACP bytes and `WM_USER_FOCUSFILE`
  (`src/fileswnb.cpp:850`) is the panel's UTF-8 path consumer — the panel
  cannot change to an ACP path. Confirmed.

### What is REFUTED

The finding's claim that with an ACP-representable non-ASCII `%TEMP%`
*"both buttons then fail"* does not hold for the **Delete** button. `tmpDir`
there is ACP and `RemoveTemporaryDir` is ANSI **end to end**
(`src/salamdr3.cpp:1019-1028`, `:988-1015`) — a consistent ANSI chain on an
ACP-representable path works correctly. The only UTF-8 island inside it,
`ClearReadOnlyAttr` (`src/salamdr3.cpp:1006`, `:1026`), fails to *probe* the
ACP path, but it only matters for read-only content: with `attr` passed in
(`:1006`) it is a no-op unless `FILE_ATTRIBUTE_READONLY` is set
(`src/salamdr5.cpp:1471`), and cache temp trees are not read-only. So the
Delete button does clean up under a `Jiří`-style `%TEMP%`.

Note the irony this exposes: `ClearTEMPIfNeeded` works *because* it is
consistently ANSI, and F-P1-04's `RemoveTemporaryDir` fails *because* its other
callers hand it UTF-8. The two findings' premises about the same function are
in tension; the resolution is that the function is ANSI and the *callers* are
the defect.

### Scope

Confirmed part: loss of function on the Focus button (any non-ASCII `%TEMP%`)
and on the whole dialog for a non-ACP `%TEMP%` (rare). Refuted part: the Delete
button.

---

## F-P1-04 · CONFIRMED

### Scenario

(a) Any shipped language, non-ASCII `%TEMP%` (P-A) or a non-ASCII unpack
target: unpack an archive with an external archiver, or F3-view a file from an
archive — the whole temp tree is left behind, silently.
(b) A plugin calling `CSalamanderGeneral::RemoveTemporaryDir` with a UTF-8 path
gets a **silent no-op** — verified callers among **enabled** plugins:
`src/plugins/regedt/dialogs.cpp:789,801,909`,
`src/plugins/renamer/rendlg.cpp:1907`, `rendlg4.cpp:21,30,43,216,355`,
`src/plugins/zip/extract.cpp:1789` (`plugins.cfg`: `regedt=on`, `renamer=on`,
`zip=on`; the demoplug caller is `off` and `selfextr.cpp` has its own private
copy, not this service).
(c) `RemoveEmptyDirs` after a Move leaves emptied non-ASCII source directories
behind.

### Evidence chain

- Plugin-facing by contract: `src/plugins/shared/spl_gen.h:1010`
  `virtual void WINAPI RemoveTemporaryDir(const char* dir) = 0;` →
  `src/plugins.h:1927` → `src/zip.cpp:828-831`
  `void CSalamanderGeneral::RemoveTemporaryDir(const char* dir) {
  ::RemoveTemporaryDir(dir); }` — the finding's claim about `src/zip.cpp:828`
  is exact.
- `src/salamdr3.cpp:1021` `SetCurrentDirectory(dir);` (A) — fails, harmless.
- `src/salamdr3.cpp:996` `HANDLE find = HANDLES_Q(FindFirstFile(path, &file));`
  (A) — fails ⇒ the loop body never runs ⇒ nothing inside the tree is deleted.
- `src/salamdr3.cpp:1015` and `:1028` `RemoveDirectory(path)` (A) — fail ⇒ the
  directory itself stays.
- `src/salamdr3.cpp:1006` `ClearReadOnlyAttr(path, file.dwFileAttributes);` —
  a **UTF-8** helper between two ANSI calls in the same loop; the finding's
  evidence line is accurate.
- `RemoveEmptyDirs` (`src/salamdr3.cpp:1031-1075`) is the same shape.

### Scope

Loss of function (disk leak) in the core; **silent no-op of a documented
plugin API** for three enabled plugins. This is the widest-reaching member of
cluster A because the defect is in a shared service, not at a call site.

---

## Cluster B — ACP text from shell/module APIs into the strict facade (F-P1-07, 08, 10)

Shared mechanism, verified once:

1. **The producers really are ANSI.** `GetModuleFileName(` and
   `SHGetFolderPath(` are un-suffixed ⇒ A entry points; there is no `W` variant
   of either anywhere in the core (`grep -n 'SHGetFolderPath' src/*.cpp` returns
   7 A sites and no W site). Both return the OS's wide value converted through
   **CP_ACP**, i.e. `C:\Users\Jiří\…` arrives as CP1250 bytes `4A 69 F8 ED`.
2. **The consumers really are strict.** `FileExists`/`DirExists`
   (`src/salamdr2.cpp:696-728`) are thin wrappers over `SalGetFileAttributes`;
   `SalGetFileAttributes` (`src/common/salfileio.cpp:372-383`) is
   `SalPathToWExtAlloc` → `GetFileAttributesW`, and returns
   `INVALID_FILE_ATTRIBUTES` + `ERROR_INVALID_NAME` the moment
   `SalPathToWExtAlloc` (`src/common/salpath.cpp:261-263` → `SalU8ToWAlloc`)
   returns NULL. `SalCreateProcess` and `SalShellExecuteEx`
   (`src/common/salfileio.cpp`) are **all-or-nothing**: one unconvertible
   string ⇒ `ERROR_INVALID_NAME` and no launch at all.
3. **CP1250/CP1252 bytes really are rejected by the strict decoder.** The
   accented ACP bytes are either invalid UTF-8 lead bytes (`ř` = `0xF8` — the
   5-byte form, illegal) or valid leads without valid continuations
   (`á` = `0xE1`, needing two `80..BF` bytes that a Latin letter is not). So
   `SalU8ToWAlloc` fails and the chain breaks. This is not "probably"; it is
   forced by the byte values.

So the class DC-09 (ANSI producer → strict facade) is real. What varies between
the three findings is only *what breaks* and *how reachable it is*.

---

## F-P1-08 · CONFIRMED (three consequences checked; one needs correcting)

### Scenario

Any shipped UI language, any ACP. Windows **local** account with a non-ASCII
name (`Jiří`, `Šárka`, `Kovács`) ⇒ `%APPDATA%` / `%LOCALAPPDATA%` are
non-ASCII.

**(a) `config.reg` auto-import from `%APPDATA%\Tandem Commander` — CONFIRMED.**
`src/salamdr1.cpp:3570`
`if (!FileExists(ConfigurationName) && GetOurPathInRoamingAPPDATA(curDir) && SalPathAppend(curDir, configReg, MAX_PATH) && FileExists(curDir))`
— `GetOurPathInRoamingAPPDATA` is `src/salamdr5.cpp:1853-1857`,
`SHGetFolderPath(NULL, CSIDL_APPDATA, …, buf)` (A) + `SalPathAppend`. The final
`FileExists(curDir)` is the strict facade on ACP bytes ⇒ FALSE. A `config.reg`
the user deliberately placed there is silently ignored. Same shape again at
`src/salamdr1.cpp:3672`. The **write** side is broken symmetrically:
`CreateOurPathInRoamingAPPDATA` (`src/salamdr5.cpp:1859-1877`) hands the ACP
path to `CreateDirectory` (A) — which *works*, because that is a consistent
ANSI pair — and then to `src/mainwnd3.cpp:2895` as the export default
directory. So the app can write there but can never read it back. That
asymmetry is worth calling out in the fix.

**(b) Google Drive — CONFIRMED, but the stated consequence is wrong.**
`src/shiconov.cpp:120` `SHGetFolderPath(…CSIDL_LOCAL_APPDATA…, sDbPath)` (A) →
`:123-124` / `:130-132` `SalPathAppend(… "Google\\Drive\\…sync_config.db") &&
FileExists(sDbPath)` (strict) ⇒ `pathOK` FALSE ⇒ the sqlite read never runs ⇒
`ret` FALSE ⇒ the fallback at `:204-212` stores `%USERPROFILE%\Google Drive`
with `pathIsFromConfig == FALSE`.
The finding then says the ACP fallback "fails as a panel path". It never gets
that far: `CShellIconOverlays::HasGoogleDrivePath` (`src/shiconov.cpp:1148-1157`)
returns FALSE unless `GoogleDrivePathIsFromCfg`, so
`src/drivelst.cpp:1976-1982` **never adds the Google Drive item to the drive
bar / Alt+F1 at all**. The correct consequence is "the Google Drive entry
disappears", not "clicking it fails". The second half of the claim —
`IsGoogleDrivePath` (`src/shiconov.h:132`) never matching, so the
Google-Drive overlay handlers are skipped at `src/shiconov.cpp:952-955` and the
sync badges are missing — does hold, because `GoogleDrivePath` then holds ACP
bytes and the panel path is UTF-8.

**(c) Dropbox — CONFIRMED.** `src/drivelst.cpp:1338-1340`
`SHGetFolderPath(…CSIDL_APPDATA…)` (A) → `SalPathAppend(sDbPath,
"Dropbox\\host.db") && FileExists(sDbPath)` (strict) ⇒ FALSE; the
`CSIDL_LOCAL_APPDATA` retry at `:1345-1349` has the identical shape and also
fails ⇒ `DropboxPath` stays empty ⇒ `src/drivelst.cpp:1984-1990` never adds the
Dropbox drive-bar item.

### Scope

Loss of function (three features silently absent), not wrong text. No error
message on any of the three paths — only `TRACE_I`. Reachable in every shipped
language; gated on the account name, not on the UI language.

### Note — an adjacent site the finding did not raise

Even on a **pure-ASCII** profile the Google Drive root itself is degraded:
`src/shiconov.cpp:161` `ConvertU2A(widePath, -1, mbPath, _countof(mbPath))`
uses the default `codepage = CP_ACP` (`src/common/strutils.h:17-18`), so a root
like `G:\Můj disk` is stored as CP1250 bytes and
`IsGoogleDrivePath(UTF-8 panel path)` can never match it. That is the same
defect class as F-P1-09 and, unlike F-P1-08, needs no unusual account name —
only a Czech-named Google Drive folder, which is what feature 058's own
scenario used. It belongs in whatever fix covers F-P1-09.

---

## F-P1-10 · CONFIRMED

### Scenario

Installation (or portable copy) under a non-ASCII, ACP-representable path —
`D:\Programy\Tandém Commander`, or the very common portable case
`C:\Users\Jiří\Downloads\TC\`. Any shipped UI language.

**F1 Help — CONFIRMED.** `src/mainwnd3.cpp:171-174`
`GetModuleFileName(HInstance, CurrentHelpDir, MAX_PATH) != 0 && CutDirectory(...) && SalPathAppend(CurrentHelpDir, "help", MAX_PATH) && DirExists(CurrentHelpDir)`
— the strict `DirExists` rejects the ACP bytes, the whole `if` is skipped,
`ok` stays FALSE, and `src/mainwnd3.cpp:217-228` shows
`SalMessageBox(parent, LoadStr(IDS_FAILED_TO_FIND_HELP), LoadStr(IDS_HELPERROR), …)`
— a hard error dialog for a help directory that is physically present next to
the .exe.

**`config.reg` next to the .exe — CONFIRMED.** `src/salamdr1.cpp:3566`
`GetModuleFileName(HInstance, ConfigurationName, MAX_PATH)` (A) → `:3570`
`FileExists(ConfigurationName)` (strict) ⇒ FALSE.

**`$(SalDir)` in a launched command line — CONFIRMED, and it is the harshest
of the three.** `src/execute.cpp:825-830` `ExecuteExpSalDir` is
`GetModuleFileName(HInstance, data->Buffer, …)` (A); the expansion table binds
it at `src/execute.cpp:1615,1622` (and `ExecuteExpSalDir2` at `:1653`). The
expanded command line reaches `src/mainwnd4.cpp:935 SalShellExecuteEx(&sei)` or
`:1000 SalCreateProcess(NULL, cmdLine, …)`, both of which convert
**all-or-nothing** ⇒ `ERROR_INVALID_NAME`. So a *single* ACP byte anywhere in
the line kills the launch — the user-menu item does nothing.

**The finding's own caveat is right and important.** The pure ANSI chains on
the same buffers keep working: plugin `.spl` loading, `lang\*.slg`
enumeration + `LoadLibrary`, `convert\` tables, `plugins.ver` — all `A` APIs
fed `A` values, consistent, and correct for any ACP-representable install path.
That is what makes the symptom so confusing in the field: the app starts,
plugins load, translations load, and only Help, `config.reg` and `$(SalDir)`
user-menu items fail.

### Scope

Loss of function; one of the three (Help) is also user-visible as a **wrong
error message** ("failed to find help" when it is there). Reachable in every
shipped language; gated on the install path, not the UI language.

---

## F-P1-07 · CONFIRMED (narrow: needs a non-ASCII install path **and** an external archiver)

### Scenario

Install path non-ASCII as in F-P1-10, **and** an operation that runs an
external archiver (`.rar`/`.arj`/`.lzh`/`.uc2`/`.ace` — see cluster E for the
reachability proof). Alt+F5 into `test.rar`: the operation fails with the
packer error box `IDS_PACKERR_PROCESS` naming `SpawnExe` and the
`ERROR_INVALID_NAME` text — i.e. the user is told the *archiver* could not be
started, when what actually failed is the conversion of the app's own
directory.

### Evidence chain

- `src/pack3.cpp:363` `GetModuleFileName(NULL, SpawnExe, MAX_PATH)` (A) →
  `:370-375` appends `utils\` + the spawn exe name. `SpawnExe` is ACP.
- `src/pack3.cpp:1753`
  `sprintf(tmpCmdLine, "\"%s\" %s %s", SpawnExe, SPAWN_EXE_PARAMS, cmdLine);`
  — ACP spliced into a line whose other half (`cmdLine`) is UTF-8.
- `src/pack3.cpp:1755` `SalCreateProcess(NULL, tmpCmdLine, …)` — strict,
  all-or-nothing ⇒ FALSE.
- `src/pack3.cpp:1759`
  `return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_PROCESS, SpawnExe, GetErrorText(err));`
- The same shape a second time at `src/pack1.cpp:613-629` / `:717`.
- `PackExecute` (`src/pack3.cpp:1704-1709`) calls `InitSpawnName` unconditionally,
  so **every** external-archiver operation goes through this.

### Why it is a separate finding from F-P1-10 and not a duplicate

The other `GetModuleFileName` consumers are strict *facades*; this one is a
strict *command-line* consumer, and it is the only place where the ACP value is
concatenated with UTF-8 text before conversion — so the failure is not "the
install path is unconvertible", it is "the whole command line, including the
correctly-encoded archive and file names, is thrown away". That distinction
matters for the fix (converting `SpawnExe` at the source fixes both halves).

### Scope

Loss of function, with a misleading error message. Conjunction of two
conditions (non-ASCII install path **and** an external archiver configured and
present) — narrower than F-P1-10, but not a non-shipping configuration, so
CONFIRMED rather than LATENT.

---

## Cluster C — cloud / volume / subst paths (F-P1-09, 12, 13, 14, 15)

Shared mechanism, verified once:

- **`ConvertU2A` silently lossy.** `src/common/strutils.cpp` `ConvertU2A` calls
  `WideCharToMultiByte(codepage, 0, …, NULL, NULL)` with **no**
  `WC_ERR_INVALID_CHARS` and **no** `lpUsedDefaultChar`. Unrepresentable
  characters therefore become `?` and the function still returns a non-zero
  count — the caller cannot detect the loss. Default `codepage` is `CP_ACP`
  (`src/common/strutils.h:17-18`). So "convert wide → `ConvertU2A` → store"
  loses information twice over: `?` for anything outside the ACP, and ACP bytes
  (not UTF-8) for everything else.
- **`GetVolumeInformation(` / `QueryDosDevice(` / `WNetGetConnection(` /
  `GetDiskFreeSpaceEx(` / `GetDriveType(` are all un-suffixed ⇒ A.** Their
  *string outputs* are CP_ACP; their *path inputs* are interpreted as CP_ACP.
- **The UI sinks are UTF-8-first with an ANSI fallback, not UTF-8-only.** This
  matters enormously for cluster C and is what separates a real defect from a
  cosmetic one: the app's own popup menu draws item text with
  `SalU8ToWAlloc` + `DrawTextW` and falls back to `DrawText` (A) when the text
  is not valid UTF-8 (`src/menu3.cpp:925-962`, `src/menu1.cpp:336-371`); the
  Volume Information dialog does the same with `SetWindowTextW` /
  `SetWindowText` (`src/dialogs3.cpp:1325-1332`). ACP bytes therefore render
  **correctly** on these surfaces. Only text that the ACP could not represent
  in the first place (already `?`-substituted by the A API) is lost.

---

## F-P1-09 · CONFIRMED IN PART / REFUTED IN PART

### Confirmed — the OneDrive personal root

`src/drivelst.cpp:1477` `DynSHGetKnownFolderPath(my_FOLDERID_SkyDrive, 0, NULL, &path)`
hands over a correct wide path; `:1481`
`done = ConvertU2A(path, -1, OneDrivePath, _countof(OneDrivePath)) != 0;`
immediately degrades it to CP_ACP (and, per the cluster preamble, cannot even
report the loss). The **one live operational consumer** is
`src/fileswn3.cpp:2585`
`driveType == drvtOneDrive && strcpy_s(path, OneDrivePath) == 0` → `:2588`
`return ChangePathToDisk(parent, path);`. With a `C:\Users\Jiří\OneDrive` root
the panel is asked to change to a path that is not valid UTF-8 and the change
fails. All the other `OneDrivePath` references
(`src/drivelst.cpp:1993,1995,2010`, `src/mainwnd3.cpp:5917-5921`) are
`[0] != 0` emptiness tests, unaffected.

The `DropboxPath` twin at `src/drivelst.cpp:1384`
(`ConvertU2A(widePath, -1, mbPath, _countof(mbPath))` — note the sibling call
one line above deliberately passes `CP_UTF8`, so the omission is visible in the
same expression) has the identical single consumer at
`src/fileswn3.cpp:2584`. Its reachability is narrower: the Dropbox item only
appears when `host.db` was found, which under a non-ASCII profile already fails
(F-P1-08), so the surviving case is an ASCII profile with a relocated,
non-ASCII Dropbox folder (`D:\Zálohy\Dropbox`).

### REFUTED — the `shellsup.cpp:2722` consumer

The finding states that *"`IsPathOnOneDrive`-style prefix tests
(`src/shellsup.cpp:2722`) never match, so OneDrive-specific handling is
silently off."* That code is **dead**: `MakeFileAvailOfflineIfOneDriveOnWin81`
lies entirely inside the `/* … */` block that opens at `src/shellsup.cpp:2706`.
P1's own "Dismissed grep lines" appendix says exactly this about
`src/shellsup.cpp:2726` — the finding contradicts its author's own dismissal
list. There is no live prefix test on `OneDrivePath` anywhere in the tree
(`grep -rn 'OneDrivePath' src/*.cpp src/*.h` — every remaining hit is an
emptiness test, a `strcpy`, or the display text). So "OneDrive-specific
handling is silently off" is not a consequence of this defect; nothing is
silently off, because nothing is there.

### Scope

Loss of function on one drive-bar / Alt+F1 item per provider. Reachable in any
shipped language; OneDrive needs only a non-ASCII account name (no config file
involved, unlike F-P1-08), which makes it the most reliably reproducible member
of cluster C.

---

## F-P1-12 · CONFIRMED IN PART (mechanism corrected) / one claim strengthened

### (b) UNC with a non-ASCII share — CONFIRMED, straightforwardly

`src/salamdr2.cpp:1417` `MyGetVolumeInformation` → `GetRootPath(ourPath, resPath)`
gives `\\server\Účetnictví\` (UTF-8) → `IsUNCPath` TRUE → the `else` branch at
`src/salamdr2.cpp:1482` `ret = GetVolumeInformation(ourPath, …)` (**A**) on
UTF-8 bytes ⇒ fails ⇒ `ret = FALSE`. No file-system name, no volume label, no
max-component-length, no flags. For an ASCII share the same call succeeds, so
this is a genuine encoding-only regression surface.

### (a) volume mounted on a non-ASCII directory — CONFIRMED in effect, but the finding's mechanism is wrong

The finding says the `CutDirectory` loop *"walks up to `C:\` and the
information line reports the **parent** volume's file system and free space."*
Traced, it does not:

- `ResolveLocalPathWithReparsePoints` (`src/salamdr2.cpp:1284+`) starts with
  `lstrcpyn(resPath, path, MAX_PATH)`, so `ourPath` is the **full** path — that
  part of the finding is right.
- Reparse-point detection is **wide** since feature 062:
  `GetReparsePointDestination` (`src/salamdr2.cpp`) does
  `SalU8ToW(...) == 0 && MultiByteToWideChar(CP_ACP, …) == 0` then
  `GetFileAttributesW`/`CreateFileW`. So a non-ASCII mount point **is** found.
- For a volume mount point the destination is `\??\Volume{GUID}\`, so
  `repPointPath[1] != ':'` and the loop takes the branch that sets
  `*cutResPathIsPossible = FALSE` and breaks.
- Back in `MyGetVolumeInformation`, the first `GetVolumeInformation(ourPath, …)`
  (A, `:1440`) fails on the UTF-8 mount-point path, and because
  `cutPathIsPossible` is FALSE the loop does `ret = FALSE; break;`.

So the result is **no volume information at all**, not the parent volume's.
Conversely, for an ordinary (non-mount-point) non-ASCII path there is **no
defect**: no reparse point is found, `cutPathIsPossible` stays TRUE, and the
loop walks up to `C:\` exactly as it does for an ASCII path — `GetVolumeInformation`
requires a root or mounted-folder path anyway, so the walk-up is the designed
behaviour, and the answer is identical. The finding over-claims the blast
radius and under-claims the severity of the case that does break.

### Strengthening — an uninitialised buffer the finding missed

`src/mainwnd5.cpp:1226-1228` (Compare Directories):
```
char fileSystem[20];
MyGetVolumeInformation(LeftPanel->GetPath(), …, fileSystem, 20);
leftFAT = StrNICmp(fileSystem, "FAT", 3) == 0;
```
The return value is **ignored** and `fileSystem` is never initialised. On the
(b) failure path `GetVolumeInformation` leaves the buffer untouched, so
`StrNICmp` reads indeterminate stack — the finding says "gets an empty
`fileSystem`", which is not what happens. Same shape at `:1232-1234`. This is a
DC-20 twin of F-P1-16/18 and should be fixed with them: whether Compare
Directories applies the 2-second FAT timestamp tolerance becomes a coin flip on
a non-ASCII UNC path.

### `GetDiskFreeSpaceEx` — CONFIRMED, cosmetic

`src/salamdr2.cpp:1182` `GetDiskFreeSpaceEx(ourPath, …)` (A) on the full panel
path fails for non-ASCII and falls through to the cluster-arithmetic fallback,
which walks up to the root. The number shown is then the volume's free space
rather than the directory's quota-aware free space — identical on an
unquota'd volume, wrong on a quota'd one. **Wrong text**, not lost function.

### Scope

(b): loss of function (no FS info on non-ASCII UNC shares) plus an
indeterminate-buffer read at one consumer. (a): loss of function, but only for
volumes mounted into a non-ASCII directory — a rare configuration, though not a
non-shipping one.

---

## F-P1-13 · CONFIRMED (narrow)

### Scenario

`subst X: D:\Dokumenty\Šablony`, then browse `X:` in a panel, on any shipped
language. The mixed-encoding path is produced exactly as claimed.

### Evidence chain

- `src/salamdr2.cpp:1781` `return QueryDosDevice(deviceName, target, MAX_PATH);`
  — **A**, so `target` = `\??\D:\Dokumenty\Šablony` in CP_ACP.
- `src/salamdr2.cpp:1799-1803` `lstrcpyn(path, target + 4, pathMax)` — the ACP
  target becomes `GetSubstInformation`'s output.
- `src/salamdr2.cpp:1266-1267`
  `if (GetSubstInformation(LowerCase[resPath[0]] - 'a', tgt, MAX_PATH) && …) { if (!SalPathAppend(tgt, resPath + 2, MAX_PATH)) … }`
  — ACP head + **UTF-8** tail → `:1277 lstrcpyn(resPath, tgt, MAX_PATH)`. The
  result is valid in neither encoding.

### Consequences, checked

- `src/fileswn8.cpp:437-445` — `ResolveSubsts(formatedFileName)` then
  `GetReparsePointDestination(...)`; the wide conversion of a mixed string
  either fails (`SalU8ToW` rejects the ACP head) or succeeds through the CP_ACP
  fallback with the UTF-8 tail mojibaked, so `GetFileAttributesW` finds nothing
  and `repPointType` is never set. The delete confirmation then says
  "file"/"directory" (`IDS_QUESTION_FILE`/`IDS_QUESTION_DIRECTORY`,
  `src/fileswn8.cpp:449`) instead of "junction"/"symlink"/"volume mount point",
  and `deleteLink` stays FALSE. **Confirmed** — and this one is more than wrong
  text: the confirmation text is what tells the user whether they are about to
  delete a link or its target.
- `src/salamdr1.cpp:1284-1289` (`PathsAreOnTheSameVolume`) and `:1386`
  (`ResolveSubsts(path1NetPath)` / `path2NetPath` → `IsTheSamePath`) —
  **Confirmed** as capable of being wrong, but the effect is a heuristic
  (`resIsOnlyEstimation`) and the fallback comparison at `:1380`
  (`_stricmp(root1, root2)`) is on roots that stay ASCII, so the practical
  impact is small.

### Scope

Loss of function, narrow: requires a `subst` drive **whose target path is
non-ASCII**. Non-subst drives are untouched, because `ResolveSubsts` leaves
`resPath` alone when `GetSubstInformation` fails. Not LATENT (nothing about the
configuration is non-shipping), but the conjunction is unusual.

---

## F-P1-14 · CONFIRMED IN PART / REFUTED IN PART

### Confirmed — labels the ACP cannot represent

`src/drivelst.cpp:1739` `GetVolumeInformation(root, volumeName, MAX_PATH, …)`
is the **A** entry point, so a label such as `Резерв` on a CP1250 machine or
`Zálohy Dokumentů` (the `ů`) on a CP1252 machine comes back with `?` already
substituted by the API — the information is destroyed before the app sees it,
and no downstream sink can recover it. The user sees `?` in the Alt+F1 drive
menu (`:1824-1832` → `drv.DriveText` → `src/drivelst.cpp:2262`
`mii.String = item->DriveText` → `MenuPopup->InsertItem`) and in the drive-bar
tooltip (`:2622` → `:2632` `sprintf(text, "%s (%s)", volumeName, freeSpaceText)`).
Same for `WNetGetConnection` at `:1769`.

### REFUTED — the implied "non-ASCII labels are mojibake" claim

The finding's scenario line reads *"A removable/fixed disk labelled with
characters outside the system ACP — e.g. … or `Zálohy Dokumentů` on an English
(CP1252) machine"*, but its "Note" then argues from
`src/dialogs3.cpp:1325` that "the two ends disagree today", implying every
non-ASCII label breaks. It does not. Both sinks are UTF-8-**first with an ANSI
fallback**:
- `src/menu3.cpp:927-962` — `SalU8ToWAlloc(item->ColumnL1, …)`; when it returns
  NULL (which is exactly what CP1250 `Zálohy` does) the code calls
  `DrawText(hDC, item->ColumnL1, …)` (A), which renders the ACP bytes
  **correctly**.
- `src/dialogs3.cpp:1325-1332` — `SalU8ToWAlloc(volumeName)`; on NULL it falls
  back to `SetWindowText(...)` (A) on an ANSI dialog, again correct.

So an ACP-representable label like `Zálohy` displays properly today. The
disagreement the finding spots is real but currently harmless — a **dead wide
branch**, not a visible defect. The defect is confined to the API-level `?`
substitution.

### REFUTED — "additionally used as an operational path"

All three live `WNetGetConnection` sites were enumerated
(`grep -rn 'WNetGetConnection' src/*.cpp src/*.h`):
`src/drivelst.cpp:1769` (display text only — the panel change uses
`drv.DriveText[0]`, the drive letter, not the text),
`src/dialogs3.cpp:1614` (the Volume Information dialog's "connected as" line,
display only), and `src/fileswn9.cpp:1884`, which **already uses
`WNetGetConnectionW`**. There is no operational consumer of an ACP
`WNetGetConnection` result.

### Scope

**Wrong text only**, and only for labels/share names outside the system ACP.
No data or function is lost. This is the weakest member of cluster C.

---

## F-P1-15 · REFUTED

### Why it cannot happen

The finding claims *"`src/shellib.cpp:2576` passes a whole path to
`GetDriveTypeA`"*. It does not — the argument is a **root**, built by the line
immediately above:

```
2569 void ResolveNetHoodPath(char* path)
2570 {
2571     if (path[0] == '\\')
2572         return;                    // UNC path -> cannot be NetHood
2573
2574     char name[MAX_PATH];
2575     GetRootPath(name, path);
2576     if (GetDriveType(name) != DRIVE_FIXED)
2577         return;
...
2580     lstrcpyn(name, path, MAX_PATH);
```

`name` holds `X:\` at line 2576 — ASCII by construction, and the UNC case
(the only way `GetRootPath` could yield non-ASCII) has already returned at
:2572. The full path is copied into `name` at line **2580**, four lines *after*
the `GetDriveType` call; that later assignment appears to be what the finding
read as the producer.

`grep -n 'GetDriveType' src/shellib.cpp` returns this single line, so there is
no other site in the file that could be meant.

### What survives

Only the DC-18 observation: `SalGetDriveTypeU8` (feature 062) exists and this
call could use it for consistency. That is a style item with no failure
scenario — a Note, not a Finding. P1's own seed C-d already classified 24 of
the 26 `GetDriveType` sites as "ASCII by construction, verified-correct"; this
site belongs in that group, and the seed's split verdict should be corrected to
26 of 26.

---

## Cluster D — "indeterminate buffers" and 058 twins (F-P1-16, 17, 18, 28)

Shared mechanism, verified once — **the house converters do not leave buffers
indeterminate**:

- `SalU8ToW` (`src/common/salunicode.cpp:237-270`) writes `buf[0] = 0` on
  **every** failure return: `:241-242` (NULL source), `:259-261` (result does
  not fit), `:268` (the general `res == 0 && buf != NULL && bufSize > 0`
  guard), and inside `SalU8ToWWtf8` (`:210-221`, established by verdict V2 for
  F-P3-01).
- `SalU8ToW**Alloc**` is different: it sizes with `buf == NULL`
  (`src/common/salunicode.cpp` `SalU8ToWAlloc` → `SalU8ToW(src, srcLen, NULL, 0)`)
  and returns NULL without touching any caller buffer. A caller that declares
  its own stack buffer *and* uses the Alloc variant first therefore does have an
  untouched buffer if the fallback also writes nothing — that distinction is
  what separates F-P1-16 from F-P1-18.

**Direct answer to the question posed with this batch:** verdict V2's
refutation of F-P3-01 does **not** by itself refute F-P1-16 — the two make
different claims. F-P3-01 said `SalU8ToW`'s return of 0 causes a `return`;
F-P1-16 says the *CP_ACP fallback's* unchecked return leaves an indeterminate
buffer. But the **evidence** V2 assembled (every `SalU8ToW` failure path writes
`buf[0] = 0`) does refute F-P1-16's "indeterminate stack memory" half, and a
line the finding did not quote refutes the rest. Details below.

---

## F-P1-16 · REFUTED as stated / CONFIRMED in a narrower residual

### Why "uninitialised stack memory" cannot happen

All three sites have the identical shape — quoted in full from
`src/snooper.cpp:581-586`:

```
WCHAR wPath[3 * MAX_PATH];
if (SalU8ToW(path, -1, wPath, _countof(wPath)) == 0)
{
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, path, -1, wPath, _countof(wPath));
    wPath[_countof(wPath) - 1] = 0;
}
```

(identically at `:731-736` and `:768-773`).

1. The `if` body is entered only after `SalU8ToW` has already written
   `wPath[0] = 0` — see the cluster preamble. So `wPath` is a valid empty
   string before `MultiByteToWideChar` is called at all.
2. The line the finding never quotes, `wPath[_countof(wPath) - 1] = 0;`,
   terminates the buffer at its last index unconditionally.

There is therefore **no execution path** on which
`FindFirstChangeNotificationW` receives unwritten stack memory: the worst case
is `L""` (fallback wrote nothing) or a truncated, NUL-terminated path (fallback
wrote a partial result). `FindFirstChangeNotificationW(L"")` fails cleanly.
"Worst case the process watches an arbitrary path" is not supported by the
code — a truncation lands mid-component with overwhelming probability, and the
call simply fails.

### What does survive — and it is worth fixing

The **capacity** argument is sound. `wPath` is `3 * MAX_PATH` = 780 WCHARs, but
`path` is a panel path that may be up to `SAL_MAX_PATH_UTF8` since features
004/027. A path longer than 779 UTF-16 units makes `SalU8ToW` return 0 for
"does not fit", the CP_ACP fallback fails for the same reason, and the result is
that `FindFirstChangeNotificationW` is called on `L""` or a truncated path ⇒ it
fails ⇒ `win->SetAutomaticRefresh(FALSE)` is never even reached at the
`AddDirectory` site (`src/snooper.cpp:587-592` only sets TRUE on success), so
the panel silently loses auto-refresh — reproducing exactly the 058 symptom the
finding names (`CheckPath` + a full re-list under `IDC_WAIT` on every window
activation). Same for the ACP=65001 case (ledger L09): `MB_PRECOMPOSED` is
invalid for CP_UTF8, the fallback returns 0 with `ERROR_INVALID_FLAGS`, and the
buffer stays `L""` — bounded, but auto-refresh is off.

So: the *defect* is real (unchecked fallback + a fixed 780-unit buffer against
a 32767-unit path domain); the *mechanism* the finding describes (indeterminate
memory handed to the API) is not. The suggested fix in the finding —
"check the fallback's return value and size the buffer from
`SalU8ToW(path,-1,NULL,0)`" — is the right fix for the surviving half.

### Scope

Loss of function (silent loss of auto-refresh) for panel paths longer than ~779
UTF-16 units, or under the UTF-8-ACP Windows setting. No memory-safety issue.

---

## F-P1-17 · REFUTED

### Why it cannot happen

The finding claims the `MakeCopyWithBackslashIfNeeded` result "is never used"
because the following line converts `path` rather than `pathCopy`. That reading
misses the signature: the function takes the **pointer by reference** and
repoints it at the copy.

`src/consts.h:210`:
```
void MakeCopyWithBackslashIfNeeded(const char*& name, char (&nameCopy)[3 * MAX_PATH]);
```
`src/salamdr5.cpp:1196-1207`:
```
void MakeCopyWithBackslashIfNeeded(const char*& name, char (&nameCopy)[3 * MAX_PATH])
{
    int nameLen = (int)strlen(name);
    if (nameLen > 0 && (name[nameLen - 1] <= ' ' || name[nameLen - 1] == '.') &&
        nameLen + 1 < _countof(nameCopy))
    {
        memcpy(nameCopy, name, nameLen);
        nameCopy[nameLen] = '\\';
        nameCopy[nameLen + 1] = 0;
        name = nameCopy;          // <-- the caller's pointer is repointed
    }
}
```

So at `src/snooper.cpp:577` `MakeCopyWithBackslashIfNeeded(path, pathCopy);`
binds `path` (the `AddDirectory` parameter, a modifiable lvalue of type
`const char*`) to the reference; after the call `path` **is** `pathCopy` when a
backslash was appended, and the `SalU8ToW(path, …)` on the next line converts
the copy. Identically at `:728`/`:733` and `:765`/`:770` with `newPath`.

The same idiom is used consistently across the tree — `src/salamdr2.cpp:1526`
(`GetReparsePointDestination`, whose subsequent `SalU8ToW(repPointDirCrFile, …)`
likewise relies on the repointing) and ~15 sites in `src/worker.cpp` — so
reading it as "the result is discarded" would condemn all of them.

The lifetime is also fine: `pathCopy` is a local of the same function and
`path` is not used after it goes out of scope.

### Scope

None. Directories whose name ends with a space or dot are watched correctly
today. The git-archaeology evidence the finding offers (`git show 4a00dc8`) is
beside the point once the signature is read.

---

## F-P1-18 · REFUTED as stated (the unchecked return is real; the consequence is not reachable)

### What is true

`src/geticon.cpp:357-367` really does not check `MultiByteToWideChar`:

```
357    WCHAR wszPathBuf[MAX_PATH];
361    WCHAR* wszPath = SalU8ToWAlloc(pszPath);
362    if (wszPath == NULL)
363    {
364        MultiByteToWideChar(CP_ACP, 0, pszPath, -1, wszPathBuf, MAX_PATH);
365        wszPathBuf[MAX_PATH - 1] = 0;
366        wszPath = wszPathBuf;
367    }
369    psfDesktop->ParseDisplayName(NULL, NULL, wszPath, &cchEaten, &pidl, NULL);
```

and, unlike the snooper, the **Alloc** variant never touches `wszPathBuf`, so
the "before" state really is uninitialised. As a hardening item the finding's
one-line fix is right.

### Why the claimed failure cannot be demonstrated

Both stated triggers dissolve on inspection:

- **"a path longer than `MAX_PATH`" (ledger L11)** — a long path that is valid
  UTF-8 never reaches the fallback at all: `SalU8ToWAlloc` allocates to fit
  (`SalU8ToW(src, srcLen, NULL, 0)` then `malloc`), so it does not fail on
  length. `wszPathBuf` is only used when `SalU8ToWAlloc` returned NULL, i.e. on
  **malformed** input (or OOM).
- **"bytes that are not valid in the ACP either"** — `MultiByteToWideChar` is
  called with `dwFlags = 0` and **no** `MB_ERR_INVALID_CHARS`, so it cannot
  fail on content: any byte sequence converts (with substitutions). Its only
  failure mode here is a buffer too small, which requires the path to be both
  malformed UTF-8 **and** longer than 259 units — and even then Windows fills
  the buffer with the partial conversion and `:365` terminates it.

So the reachable worst case is `ParseDisplayName` on a truncated or mojibaked
path ⇒ `pidl == NULL` ⇒ the caller falls back to a default icon. No
indeterminate read is demonstrable, and no user-visible consequence beyond a
generic icon.

### Scope

Latent hardening item; keep it as a one-line guard alongside F-P1-16's fix,
but it is not the memory-safety defect the finding describes. Note the
uninitialised-buffer *pattern* the finding was hunting does exist elsewhere and
was found by this verifier at `src/mainwnd5.cpp:1226-1228` / `:1232-1234`
(see F-P1-12) — that one is real.

---

## F-P1-28 · REFUTED for its headline claim / LATENT for the two icon-file sites

### REFUTED — the item name is *not* appended through the ACP

The finding's central claim is that `src/shiconov.cpp:932` "still appends the
item name through the ACP, so `IsMemberOf` is asked about a path that does not
exist and the sync-status badge is missing for exactly those items", using
`G:\Můj disk\Smlouvy\Účtenka.pdf` as the example. Read the pair:

```
931    if (SalU8ToW(name, -1, wName, MAX_PATH - (int)(wName - wPath)) == 0) // name is UTF-8 (feature 004)
932        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, name, -1, wName, MAX_PATH - (int)(wName - wPath));
933    wPath[MAX_PATH - 1] = 0; // just to be safe
```

Line **931 is the correct UTF-8 conversion and it runs first**; line 932 is
only the fallback for input `SalU8ToW` rejects. `Účtenka.pdf` is valid UTF-8,
so 931 succeeds and 932 never executes. Since feature 066 `SalU8ToW` also
accepts lone-surrogate WTF-8, so even the exotic names of feature 066 take the
correct path. The finding's own gloss — "the fallback is reached for any name
the ACP *can* represent only by luck" — inverts the condition: the fallback is
reached only for names `SalU8ToW` **rejects**, which for a real NTFS name means
essentially never.

The capacity worry that sinks F-P1-16 does not apply here either: the guard at
`src/shiconov.cpp:925-927` rejects the item up front when
`(wName - wPath) + strlen(name) >= MAX_PATH`, and UTF-16 units never outnumber
UTF-8 bytes, so `SalU8ToW` cannot fail for lack of room.

Conclusion: 058's prefix fix (`src/fileswn1.cpp:496`) and this name conversion
together form a **complete** wide path. Badges on `G:\Můj disk\…\Účtenka.pdf`
work.

### LATENT — the two icon-file paths

- `src/shiconov.cpp:1017`
  `WideCharToMultiByte(CP_ACP, 0, (wchar_t*)iconFile, -1, iconFileMB, MAX_PATH, NULL, NULL);`
  → `:1022 ExtractIcons(iconFileMB, …)`. `ExtractIcons` is declared
  `LPCTSTR` (`src/geticon.h:37`) ⇒ the **A** entry point, so this is a
  *consistent* CP_ACP → ANSI chain and works for every icon-file path the ACP
  can represent — which in practice means every overlay handler, since they
  live under Program Files / AppData. Only a handler installed under a path
  outside the system ACP loses its overlay icon.
- `src/geticon.cpp:120-127` — the same shape, and even narrower: the degraded
  `iconFile` is used for exactly two things, the ASCII test
  `iconFile[0] == '*' && iconFile[1] == 0` (`:139`, unaffected by any code
  page) and the **last-resort** `ExtractIcons(iconFile, …)` at `:264`, which
  runs only after both `Extract` attempts already failed. The primary extraction
  on the wide path uses the untouched original:
  `:252 ((IExtractIconW*)pxi)->Extract(iconFileW, …)`. On top of that, the
  whole `IExtractIconW` branch is itself a fallback taken only when
  `IID_IExtractIconA` was refused (`:114-121`).

Neither site loses anything for an ACP-representable path, and neither is on
the badge path the finding's scenario describes.

### Scope

Headline claim: no defect — badges on non-ASCII item names work. Icon-file
sites: **wrong/missing icon** (never data), reachable only for icon files
stored under a path outside the system ACP, in fallback branches. LATENT.

---

## Cluster E — operations and assorted sites (F-P1-05, 06, 19–27)

Two shared facts, verified once:

- **Reachability of the external-archiver path — CONFIRMED exactly as P1
  claims.** `CPackerFormatConfig::AddDefault` (`src/pack3.cpp:411-438`) maps
  `rar;r##`→1, `arj;a##`→9, `lzh`→3, `uc2`→4, `ace`→10, and
  `src/pack1.cpp:1369-1381` treats a **non-negative** unpacker index as "external
  archiver" (a negative index is `Plugins.Get(-index - 1)`). No enabled plugin
  claims those extensions: `plugins.cfg` has `unrar=off`, and the registrations
  of the enabled archive plugins are `7z` + `nrg;pdi;cdi;cif;ncd` + `c2d`
  (`src/plugins/7zip/7zip.cpp:610,643,649`), `tgz;tbz;taz;tar;gz;bz;bz2;z;rpm;cpio;deb`
  (`src/plugins/tar/tardll.cpp:288`) and `zip;pk3;jar`
  (`src/plugins/zip/main.cpp:515`). So for `.rar` the external archiver is the
  **only** route in a shipped build; the user needs nothing but `RAR.EXE` on the
  machine, which for a WinRAR user is automatic.
- **The panel renders invalid-UTF-8 names correctly.** This is decisive for
  F-P1-05 and is easy to get wrong: `src/fileswn4.cpp:711`
  `wNameLen = SalU8ToW(TransferBuffer, nameLen, wbuf, _countof(wbuf)) - 1;  // -1 = invalid UTF-8`
  and then every draw/measure call is guarded `if (wNameLen >= 0) …W(…) else …A(…)`
  (`:724-729`, `:794-796`, `:802-805`). A name held as CP_ACP bytes therefore
  **displays correctly**, not as mojibake.

---

## F-P1-05 · CONFIRMED IN PART / REFUTED IN PART

### CONFIRMED — the pack direction

`src/pack2.cpp:318,327,345` (`PackUniversalCompress`): the names come from
`nextName(...)` over the **disk** panel selection, i.e. genuine UTF-8/WTF-8 from
`SalFindFirstFile`. `CharToOem(name, namecnv)` interprets those bytes as ACP and
maps them to the OEM page, so packing `Žluťoučký kůň.txt` into `test.rar`
(Alt+F5) writes a name nobody typed into the list file and RAR reports it cannot
find the file. The `needANSIListFile` branch (`:329 strcpy(namecnv, name)`) is
no better: raw UTF-8 bytes read by the archiver as ACP. `src/pack2.cpp:653,658`
is the same code shape in the sibling function. **Confirmed.**

### REFUTED — "the panel shows mojibake"

`src/pack1.cpp:302` `OemToCharBuff(pomptr, newfile.Name, newfile.NameLen)` in
`PackScanLine` does store **ACP** bytes into `CFileData::Name`, which is a
UTF-8-by-contract field — that part is right, and it is a genuine contract
violation. But the stated consequence is wrong twice over:

1. **Display.** Per the cluster preamble, `src/fileswn4.cpp:711` +
   `:802-805` fall back to the byte-wise ANSI draw when `SalU8ToW` fails, so an
   ACP name from a `.rar` listing renders **correctly** in the panel.
2. **"every operation on that item addresses a nonexistent name".** The archiver
   operations round-trip: the extract list file is built by re-applying
   `CharToOem` (`src/pack1.cpp:1498`) to those same ACP bytes, producing exactly
   the OEM bytes the archiver printed. The two defects cancel, and extracting a
   file that the archiver itself listed **works** for any ACP-representable name.

So F-P1-05 is really two findings of very different weight: a live, reproducible
failure when packing disk files into an external archive, and a latent contract
violation on the listing side whose visible symptoms only appear where the ACP
name meets a strict UTF-8 consumer — which is F-P1-06's territory, not the
panel's.

### Scope

Pack direction: loss of function, any shipped language, needs a non-ASCII file
name and an external archiver. Listing direction: contract violation with no
independent user-visible symptom.

---

## F-P1-06 · CONFIRMED

### Scenario

Any shipped UI language. F3-view (or extract) a file from `test.rar`. Two
independent triggers, both real:

**(a) non-ASCII `%TEMP%` (premise P-A).** `src/pack1.cpp:1466`
`SalGetTempFileName(NULL, "PACK", tmpListNameBuf, TRUE)` produces a UTF-8 path
in `%TEMP%`; `src/pack1.cpp:1477` `fopen(tmpListNameBuf, "w")` is the **narrow
CRT**, which resolves the path through the ACP ⇒ NULL ⇒
`src/pack1.cpp:1479-1481` cleans up with `RemoveDirectory`/`DeleteFile` (both A,
so the cleanup also fails, leaving the temp directory behind) and returns
`IDS_PACKERR_FILE` — "cannot create the file list".

**(b) a non-ASCII extracted name, even with an ASCII `%TEMP%`.** In
`PackUniversalUncompressOneFile` the archiver writes the real Unicode name into
the temp directory; then
`src/pack1.cpp:1863` `FindFirstFile(extractedFile, &foundFile)` (**A**) returns
`foundFile.cFileName` in **CP_ACP**, and
`src/pack1.cpp:1888-1891`
```
char* srcName = (char*)malloc(strlen(tmpDirNameBuf) + 1 + strlen(foundFile.cFileName) + 1);
strcpy(srcName, tmpDirNameBuf);  strcat(srcName, "\\");  strcat(srcName, foundFile.cFileName);
```
splices a UTF-8 directory with an ACP name. That mixed string goes to
`src/pack1.cpp:1900` `SalMoveFile(srcName, destName)`, which is the **strict**
facade (`src/common/salfileio.cpp` `SalMoveFile` → `SalMoveFileEx` →
`SalPathToWExtAlloc`, `ERROR_INVALID_NAME` when either side fails to convert) ⇒
the user gets the packer error box `IDS_PACKERR_GENERAL` reading
`"MoveFile: <error text>"`. The `destName` half is equally mixed
(`targetDir` UTF-8 + `onlyName` from the ACP archive listing).

The `GetShortPathName` sites in the same subsystem are DC-18 twins
(`SalGetShortPathName` exists), as the finding says.

### Scope

Loss of function with a visible but misleading error, plus a leaked temp
directory. This is the finding in cluster E with the clearest end-to-end proof.

---

## F-P1-19 · CONFIRMED (with one correction)

### Scenario

*Commands ▸ Compare Directories*, any shipped language.

- **"by content" on non-ASCII file names — CONFIRMED.**
  `src/mainwnd5.cpp:292,294`
  `HANDLE hFile1 = HANDLES_Q(CreateFile(file1, GENERIC_READ, …));` /
  `hFile2 = … CreateFile(file2, …)` are the **A** entry points on UTF-8 paths;
  both fail and the pair cannot be compared.
- **subdirectory enumeration — CONFIRMED, but not silently.**
  `src/mainwnd5.cpp:572` `hFind = HANDLES_Q(FindFirstFile(path, &data));` (A);
  `path` is UTF-8 (built with `SalPathAppend` from the panel path, and the error
  message at `:583` uses `LoadStrU8(IDS_CANNOTREADDIR)`, which settles the
  encoding question). The finding says the subtree is "silently treated as
  empty and the two sides compare as equal". It is not silent: the mangled path
  yields `ERROR_PATH_NOT_FOUND`, which is neither `ERROR_FILE_NOT_FOUND` nor
  `ERROR_NO_MORE_FILES`, so `src/mainwnd5.cpp:575-588` takes the error branch and
  shows "Cannot read directory …" with a Cancel option. The silent-skip
  behaviour applies only to the `getTotal` pre-pass (`:579-580`).

### Scope

Loss of function. The corrected symptom (a visible error dialog per non-ASCII
subdirectory) is arguably worse for the user than the claimed silent one,
because the comparison becomes unusable rather than merely wrong.

---

## F-P1-20 · CONFIRMED

### Scenario

Any shipped language. F4-edit a file inside an archive; in the resulting
"changed files" dialog (`src/dialogs5.cpp:1477`
`FileStamps->CopyFilesTo(HWindow, indexes, selCount, initPath);`) use *Copy
To…* and pick a target. Nothing is copied and **no error is shown**.

### Evidence chain

- `src/salamdr3.cpp:3222-3231` — `fromStr`/`toStr` are built from
  `item->SourcePath` (a `SalGetTempFileName` temp path, UTF-8) and
  `item->FileName` / `item->ZIPRoot` (UTF-8 names).
- `src/salamdr3.cpp:3254` `SHFileOperation(&fo);` — the **A** entry point, and
  the return value is discarded entirely (no `if`, no `fo.fAnyOperationsAborted`
  check afterwards).
- **DC-18 confirmed:** `SHFileOperationW` is already used at
  `src/fileswn8.cpp:43` — with a comment naming the exact defect ("feature 005:
  the ANSI SHFileOperation reinterprets the UTF-8 name/path bytes") — and at
  `src/finddlg2.cpp:191`, which keeps `:205 SHFileOperation` only as the legacy
  fallback. `src/salamdr3.cpp:3254` is the one site of the three that was never
  converted.

### Scope

**Silent data loss from the user's point of view**: the edit exists only in a
temp file, the copy-out does nothing, and no error is reported. Within cluster E
this is the most serious consequence per occurrence, even though it needs a
non-ASCII name or `%TEMP%` to trigger.

---

## F-P1-21 · CONFIRMED (all nine site groups verified as ANSI on UTF-8 values)

Every cited line was opened; all are un-suffixed (A) entry points and all take a
UTF-8 producer:

| Site | Line as it stands | Producer |
|---|---|---|
| `src/zip.cpp:2499,2516,2527` | `::DeleteFile(pluginData->FileName);` | plugin-supplied temp name, UTF-8 by the 052 metadata contract — and this is inside `CSalamanderGeneral::ViewFileInPluginViewer`, a **plugin-facing service** |
| `src/shellsup.cpp:1004,1416` | `if (CreateDirectory(fakeRootDir, NULL))` | `SalGetTempFileName(NULL,"SAL",fakeRootDir,FALSE)` at `:998` + `SalPathAppend(…,"DROPFAKE",…)` |
| `src/mainwnd4.cpp:834` | `CreateFile(batName, GENERIC_WRITE, …)` | user-menu `.bat` wrapper in `%TEMP%` |
| `src/packac.cpp:467,656,722` | `CreateFile(filename,…)`, `FindFirstFile(fileName,…)`, `FindNextFile(…)` | SFX-archive builder inputs |
| `src/dialogs6.cpp:2458`, `src/worker.cpp:7058`, `src/salshlib.cpp:652,689` | `FindFirstFile(fileName,…)`, `CreateFile(name,…)`, `CreateFile(ArchiveFileName,…)` ×2 | panel/archive paths |
| `src/shellsup.cpp:623,630` | `FindFirstFile(s,…)` / `FindNextFile(…)` in `CountNumberOfItemsOnPath` | the panel path + `"*.*"` via `SalPathAppend` (`:620`) |
| `src/execute.cpp:1213`, `src/editwnd.cpp:996`, `src/mainwnd4.cpp:538,551` | `GetShortPathName(...)` | `$(DOSPath)`/`$(DOSFullName)` expansion on panel paths |

The consequences the finding lists follow mechanically and were not
independently re-derived one by one; the two that were traced end to end
(`src/shellsup.cpp:623` → always 0 items ⇒ the post-shell-operation refresh
heuristic misfires, and `src/zip.cpp:2499` ⇒ a plugin viewer's temp file is
never deleted) hold. `SalGetShortPathName` exists, so the four
`GetShortPathName` sites are DC-18 twins as claimed.

### Scope

Loss of function, per-site, all silent. The `zip.cpp` group deserves separate
weight because it is a **plugin-facing service**, like F-P1-04.

---

## F-P1-22 · CONFIRMED

### Scenario

Any shipped language. A user-menu item whose icon comes from an executable under
a non-ASCII path (`D:\Programy\Můj nástroj\tool.exe`): the *Change Icon* picker
shows the icons correctly, and the User Menu then shows the default icon.

### Evidence chain

- `src/salamdr3.cpp:2308` `ExtractIconEx(item->FileName, item->IconIndex, NULL, &umIcon, 1) == 1)`
  and `:2691` `ExtractIconEx(fileName, iconIndex, NULL, &UMIcon, 1) == 1)` —
  **A**, on the configured user-menu path.
- `src/dialogs3.cpp:2317-2318` — the picker in the same feature:
  ```
  // the name is UTF-8 (feature 004): extract via the W API; on conversion failure keep the legacy A call
  BOOL fileNameWValid = SalU8ToW(fileName, -1, fileNameW, _countof(fileNameW)) != 0;
  int iconsCount = fileNameWValid ? ExtractIconExW(fileNameW, -1, NULL, NULL, 0) : ExtractIconEx(fileName, -1, NULL, NULL, 0);
  ```
  This is both the DC-18 twin **and** the proof that `fileName` is UTF-8 — the
  team wrote the comment while converting the picker and did not convert the two
  runtime sites.

### Scope

**Wrong image only** (default icon instead of the right one). No data, no
function. The internal inconsistency — picker right, menu wrong — makes it easy
to reproduce and cheap to fix.

---

## F-P1-23 · CONFIRMED

### Scenario

Any shipped language, non-ASCII account name. Type `%USERPROFILE%\Desktop` into
the change-directory dialog (Shift+F7) or the command line: the panel reports
that the path does not exist.

### Evidence chain

- `src/fileswn9.cpp:666` `DWORD auxRes = ExpandEnvironmentStrings(buff, expandedBuff, buffSize + 1);`
  — **A**. The literal part of `buff` round-trips (ACP↔UTF-16 is a bijection for
  valid ACP bytes), but the **substituted value** comes back in CP_ACP:
  `C:\Users\Jiří` arrives as `…4A 69 F8 ED`. `:673 lstrcpyn(buff, expandedBuff, buffSize)`
  then puts those bytes into what the caller treats as a UTF-8 panel path.
- `src/icncache.cpp:796` `ExpandEnvironmentStrings(buf, iconLocation, MAX_PATH + 10)`
  — same shape on `DefaultIcon` registry values, which for per-user installed
  applications routinely start with `%USERPROFILE%` or `%LOCALAPPDATA%` ⇒ those
  file types fall back to the generic icon.
- `src/mainwnd4.cpp:1061` `SetEnvironmentVariable(name, dir);` (**A**) — the
  `=A:`…`=Z:` per-drive current directories exported to child processes carry
  the mis-encoded value onward.

### Scope

Loss of function (a); wrong image (b); wrong data passed to child processes (c).
Reachable in every shipped language; gated on the account name.

---

## F-P1-24 · CONFIRMED IN PART / REFUTED IN PART

### CONFIRMED — the values really are CP_ACP and they really are stored/used as UTF-8

- `src/execute.cpp:2151-2152` `BrowseCommand`: `GetOpenFileName(&ofn)` (**A**)
  ⇒ `file` is CP_ACP; `:2154 SalGetFullName(file)`; `:2156` `WM_SETTEXT` back
  into the configuration edit line.
- `src/shellib.cpp:2547` `SHGetPathFromIDList(res, path);` (**A**) inside
  `GetTargetDirectoryAux` ⇒ the Copy/Move "Browse" target directory is ACP.
- `GetMyDocumentsOrDesktopPath` (`src/shellib.cpp:2935-2972`) is ANSI end to end
  (`SHGetPathFromIDList` at `:2944`/`:2955`), and its result is used
  operationally at `src/fileswn3.cpp:2582` (the *My Documents* drive-bar item →
  `ChangePathToDisk`) and by `GetIfPathIsInaccessibleGoTo`
  (`src/salamdr6.cpp:1764-1789`). Under a non-ASCII account name both fail.
- `src/salamdr6.cpp:1752` is the DC-09 twin inside `SafeGetSaveFileNameW`:
  `if (!GetMyDocumentsOrDesktopPath(initDir, MAX_PATH) || SalU8ToW(initDir, -1, initDirW, MAX_PATH) == 0) initDirW[0] = 0;`
  — the strict converter on an ACP producer, failure branch = "no initial
  directory". Confirmed exactly as described.
- **DC-18 confirmed:** `SafeGetSaveFileNameW` exists (`src/consts.h:1708`,
  `src/salamdr6.cpp:1744`), while `src/viewer3.cpp:914`
  (`SafeGetOpenFileName`), `:1619` (`SafeGetSaveFileName`),
  `src/mainwnd3.cpp:2926` and `src/dialogs5.cpp:749` are still the A wrappers.

### REFUTED — "the value then displays as mojibake in the dialog's UTF-8-aware sinks"

The configuration dialogs are **ANSI** windows (no `UnicodeWnd`; established by
verdict V2 for F-P3-05), and `BrowseCommand` both reads
(`src/execute.cpp:2129-2130 SendMessage(..., WM_GETTEXT, ...)`) and writes
(`:2156 WM_SETTEXT`) through the ANSI message path. The whole browse round-trip
is therefore ACP-consistent and the edit line **displays the path correctly**.
The damage is deferred: the ACP bytes are persisted into a UTF-8 field and only
fail later, when the configured program is launched through
`SalCreateProcess` / `SalShellExecuteEx` (all-or-nothing, `ERROR_INVALID_NAME`)
or when a target directory reaches the UTF-8 copy engine.

### Scope

Loss of function, deferred from the moment of configuration to the moment of
use — which is what makes it hard for a user to attribute. Not a text-rendering
defect.

---

## F-P1-25 · CONFIRMED (and one consequence the finding missed is worse)

All seven cited lines are `MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, <UTF-8
value>, -1, <wide buffer>, MAX_PATH)` with **no** `SalU8ToW` attempt first —
verified individually at `src/shellsup.cpp:536`, `src/mainwnd3.cpp:7121`,
`src/dialogs6.cpp:554`, `src/fileswn0.cpp:341`, `src/fileswn2.cpp:150`,
`src/shellib.cpp:2649`, `src/worker.cpp:6212`. The contrast the finding draws is
also real: `src/shellib.cpp:1600-1630` does `SalU8ToWAlloc` first and only then
falls back, which is the shape these should have.

The `.lnk` case is the clearest: `src/shellsup.cpp:535-538`
```
OLECHAR oleName[MAX_PATH];
MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, fullName, -1, oleName, MAX_PATH);
oleName[MAX_PATH - 1] = 0;
if (fileInt->Load(oleName, STGM_READ) == S_OK && link->GetPath(linkTgt, MAX_PATH, NULL, SLGP_UNCPRIORITY) == NOERROR)
```
`fullName` is the panel's UTF-8 full path, so for `D:\Zálohy\Program.lnk` the
`Load` fails, `linkIsDir`/`linkIsFile` both stay FALSE, and Enter does not
follow the shortcut.

### Additional defect found while verifying — same site, opposite direction

`link->GetPath(linkTgt, …)` is `IShellLink**A**`, so `linkTgt` comes back in
CP_ACP, and `src/shellsup.cpp:541` immediately probes it with the **strict**
`SalGetFileAttributes`. For a shortcut whose *target* is a non-ASCII directory
the probe fails, the `else` at `:544` sets `linkIsFile = TRUE`, and a directory
shortcut is treated as a file. This trigger needs only a non-ASCII **target** —
the `.lnk` itself may sit in a plain ASCII folder — so it is strictly more
reachable than the case the finding describes, and it belongs in the same fix.

### Scope

Loss of function (shortcuts, share creation, shell context menu/properties on
non-ASCII names) plus wrong text in the Windows jump list
(`src/mainwnd3.cpp:7121`).

---

## F-P1-26 · CONFIRMED

### Scenario

Any shipped language. Drag `Účtenka.pdf` from Explorer onto the command line,
the status bar, the toolbar, or the internal viewer.

### Evidence chain

- The payload really is wide: `src/toolbar5.cpp:162-170`,
  `src/stswnd.cpp:~1459-1467`, `src/editwnd.cpp:1243-1255` all branch on
  `data->fWide` and, in the wide arm, call
  `WideCharToMultiByte(CP_ACP, 0, fileW, l + 1, path, …)` — throwing away
  exactly the information the wide payload was carrying.
- The failure is then *visible at the next line* in the toolbar case:
  `src/toolbar5.cpp:189` `if (ret && !FileExists(path)) ret = FALSE;` —
  `FileExists` is the strict UTF-8 facade, so the ACP path is rejected and the
  drop is silently refused.
- Viewer: `src/viewer3.cpp:599` `DragQueryFile((HDROP)wParam, 0, path, MAX_PATH);`
  is the **A** entry point (so the OS itself does the lossy conversion) →
  `:601 SalGetFullName(path)` → `:607 OpenFile(path, NULL, FALSE)` ⇒ the viewer
  reports the file does not exist.

### Scope

Loss of function, silent in three of the four surfaces. The finding's suggested
fix (`SalWToU8`, total since 066, and `DragQueryFileW`) is exactly right and
needs no failure branch.

---

## F-P1-27 · CONFIRMED (narrow)

### Evidence chain

`src/shares.cpp:105-107`
```
WideCharToMultiByte(CP_ACP, 0, p->shi502_netname, -1, netname, MAX_PATH, NULL, NULL) &&
WideCharToMultiByte(CP_ACP, 0, p->shi502_path,    -1, path,    MAX_PATH, NULL, NULL) &&
WideCharToMultiByte(CP_ACP, 0, p->shi502_remark,  -1, remark,  MAX_PATH, NULL, NULL))
```
`NetShareEnum` is W-only, so the true name **is** available and is discarded.
The cached ACP values are then matched against UTF-8 panel data at
`src/fileswn3.cpp:547` `file.Shared = Shares.Search(file.Name);` and
`src/drivelst.cpp:1850` `drv.Shared = Shares.Search(root);` — a share named
`Účetnictví` never matches, so the shared-folder marker/overlay is absent. There
is also an operational consumer, `src/fileswn9.cpp:1864`
`Shares.GetUNCPath(buff, uncPath, 2 * MAX_PATH)`, which cannot map a non-ASCII
local path to its UNC form.

The `src/shellib.cpp:510,579,1565,1848,1954,3039` group (`IShellFolder`/`STRRET`
name extraction) is the same pattern; those degrade to `?` for names the ACP
cannot represent.

### Scope

Missing marker / wrong text; one operational consumer (`GetUNCPath`). Requires
a share or shell-namespace name that is non-ASCII — real, but narrower than most
of cluster E because share names are usually ASCII.

---

## F-P1-11 · WITHDRAWN (no independent verdict)

The author withdrew this finding before submission, and spot-checking confirms
the withdrawal was correct: `src/shellsup.cpp:2357-2364` already reads
`WCHAR* cwdW = SalU8ToWAlloc(panel->GetPath()); if (cwdW != NULL) { SetCurrentDirectoryW(cwdW); free(cwdW); } else SetCurrentDirectory(panel->GetPath());`,
so the one load-bearing `SetCurrentDirectory` (Open With) is a feature-014 wide
call with an ANSI fallback, not an ANSI call. Recorded here for completeness;
no verdict was required of this verifier.

---

## Summary — batch V5

| Finding | Verdict | One-line reason |
|---|---|---|
| F-P1-01 | **CONFIRMED** | `cache.cpp:117,119,349-350,472-474` are A on `SalGetTempFileName`'s UTF-8 output; temp files/dirs leak. (`CleanFromDisk` returns FALSE + `TRACE_E`, it does not "report success".) |
| F-P1-02 | **CONFIRMED** | `cache.cpp:1152 GetTempPath` (A) vs `ContainTmpName`'s `StrNICmp` against the UTF-8 `Path` (`:358-360`) — the prefix can never match, so a new `SAL####.tmp` per cached file. |
| F-P1-03 | **CONFIRMED IN PART** | Focus button (ACP path → `WM_USER_FOCUSFILE`) and the non-ACP-`%TEMP%` case are real; **REFUTED**: the Delete button works, `RemoveTemporaryDir` is ANSI end-to-end and the chain is ACP-consistent. |
| F-P1-04 | **CONFIRMED** | `salamdr3.cpp:996,1009,1015,1021,1028` all A on UTF-8; silent no-op of the plugin service `spl_gen.h:1010` → `zip.cpp:828`, used by regedt/renamer/zip (all `on`). |
| F-P1-05 | **CONFIRMED IN PART** | Pack direction (`pack2.cpp:318,327,345` `CharToOem` on genuine UTF-8 disk names) is real; **REFUTED**: the panel does *not* show mojibake (`fileswn4.cpp:711,802-805` ANSI fallback) and extract round-trips because `CharToOem` re-inverts `OemToCharBuff`. |
| F-P1-06 | **CONFIRMED** | `pack1.cpp:1477 fopen` (narrow CRT) on a UTF-8 temp path, and `:1863 FindFirstFile` (A) → ACP `cFileName` spliced into a UTF-8 dir → `:1900 SalMoveFile` (strict) fails with a visible "MoveFile:" error. |
| F-P1-07 | **CONFIRMED** (narrow) | `pack3.cpp:363 GetModuleFileName` (A) → `:1753 sprintf` → `:1755 SalCreateProcess`, which is all-or-nothing; needs a non-ASCII install path **and** an external archiver. |
| F-P1-08 | **CONFIRMED** | `SHGetFolderPath` (A) → `FileExists`/`DirExists` (strict) kills `config.reg` auto-import, Dropbox and Google Drive detection. Correction: Google Drive's drive-bar item **disappears** (`HasGoogleDrivePath` needs `IsFromCfg`), it does not "fail as a panel path". |
| F-P1-09 | **CONFIRMED IN PART** | `drivelst.cpp:1481` / `:1384` `ConvertU2A` (CP_ACP default, silently lossy) → the single live consumer `fileswn3.cpp:2584-2585` → `ChangePathToDisk` fails; **REFUTED**: `shellsup.cpp:2722` is inside a `/* */` block (P1's own dismissal list says so) — there is no live prefix test. |
| F-P1-10 | **CONFIRMED** | `GetModuleFileName` (A) → strict consumers: F1 Help shows `IDS_FAILED_TO_FIND_HELP` (`mainwnd3.cpp:171-174`, `:217-228`), `config.reg` ignored (`salamdr1.cpp:3566-3570`), `$(SalDir)` kills the launch (`execute.cpp:825` → `mainwnd4.cpp:935/1000`). |
| F-P1-11 | **WITHDRAWN** | Withdrawn by its author; spot-check confirms `shellsup.cpp:2357-2364` is already the feature-014 `SetCurrentDirectoryW` with an ANSI fallback. No verdict required. |
| F-P1-12 | **CONFIRMED IN PART** | (b) non-ASCII UNC share ⇒ `salamdr2.cpp:1482 GetVolumeInformation` (A) fails, real. (a) mechanism **corrected**: at a mount point `cutPathIsPossible` is FALSE so the function returns FALSE (no info), it does not report the parent volume; an ordinary non-ASCII path is unaffected. Strengthened: `mainwnd5.cpp:1226-1228,1232-1234` read an **uninitialised** `fileSystem[20]`. |
| F-P1-13 | **CONFIRMED** (narrow) | `salamdr2.cpp:1781 QueryDosDevice` (A) → `:1266-1277` ACP head + UTF-8 tail; the delete confirmation loses "junction/symlink" (`fileswn8.cpp:437-449`). Needs a subst drive with a non-ASCII target. |
| F-P1-14 | **CONFIRMED IN PART** | Labels **outside** the ACP arrive `?`-substituted from `GetVolumeInformation` (A) — wrong text. **REFUTED**: ACP-representable labels render correctly (`menu3.cpp:927-962`, `dialogs3.cpp:1325-1332` both fall back to the A call), and no `WNetGetConnection` result is used operationally (`fileswn9.cpp:1884` already uses the W variant). |
| F-P1-15 | **REFUTED** | `shellib.cpp:2576` receives `name` from `GetRootPath(name, path)` on the **previous line** (`:2575`) — a root, ASCII by construction; the full path is copied in at `:2580`, *after* the call. UNC returns at `:2572`. |
| F-P1-16 | **REFUTED as stated / CONFIRMED in residual** | No indeterminate buffer: `SalU8ToW` writes `buf[0]=0` on every failure (`salunicode.cpp:241,259-261,268`) and `wPath[_countof(wPath)-1]=0` terminates it. Residual real defect: 780-WCHAR buffer + unchecked fallback ⇒ silent loss of auto-refresh for panel paths >779 units (and under ACP=65001). |
| F-P1-17 | **REFUTED** | `MakeCopyWithBackslashIfNeeded(const char*& name, …)` takes the pointer **by reference** and does `name = nameCopy` (`consts.h:210`, `salamdr5.cpp:1196-1207`) — the copy *is* what the next line converts. |
| F-P1-18 | **REFUTED as stated** | The unchecked `MultiByteToWideChar` is real, but the fallback runs only for malformed UTF-8 (`SalU8ToWAlloc` allocates, so length never triggers it) and CP_ACP with `dwFlags=0` cannot fail on content; `:365` terminates the buffer. Keep as a one-line hardening fix. |
| F-P1-19 | **CONFIRMED** | `mainwnd5.cpp:292,294 CreateFile` (A) and `:572 FindFirstFile` (A) on UTF-8 paths. Correction: the subdirectory case is **not** silent — `ERROR_PATH_NOT_FOUND` takes the error branch at `:575-588` ("Cannot read directory"). |
| F-P1-20 | **CONFIRMED** | `salamdr3.cpp:3254 SHFileOperation(&fo);` (A) with the return value discarded, on UTF-8 `fromStr`/`toStr`; W twins already at `fileswn8.cpp:43` and `finddlg2.cpp:191`. Silent loss of an edited file's copy-out. |
| F-P1-21 | **CONFIRMED** | All nine site groups re-opened; every one is an A entry point on a UTF-8 producer. The `zip.cpp:2499,2516,2527` group is a **plugin-facing service** (`ViewFileInPluginViewer`). |
| F-P1-22 | **CONFIRMED** | `salamdr3.cpp:2308,2691 ExtractIconEx` (A) while the picker at `dialogs3.cpp:2317-2318` already does `SalU8ToW` + `ExtractIconExW` — which also proves the value is UTF-8. Wrong image only. |
| F-P1-23 | **CONFIRMED** | `fileswn9.cpp:666 ExpandEnvironmentStrings` (A) substitutes `%USERPROFILE%` in CP_ACP into a UTF-8 path; same at `icncache.cpp:796` and `mainwnd4.cpp:1061`. |
| F-P1-24 | **CONFIRMED IN PART** | The ACP values and their UTF-8 consumers are real (`execute.cpp:2152`, `shellib.cpp:2547`, `GetMyDocumentsOrDesktopPath` → `fileswn3.cpp:2582`, `salamdr6.cpp:1752/1769`), and `SafeGetSaveFileNameW` is a genuine missed twin. **REFUTED**: nothing displays as mojibake — the config dialogs are ANSI, so the browse round-trip is ACP-consistent; the failure is deferred to launch/copy time. |
| F-P1-25 | **CONFIRMED** | Seven `MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, <UTF-8>, …)` sites with no `SalU8ToW` attempt. Plus an unraised, *more* reachable twin at the same site: `shellsup.cpp:538-544` takes `IShellLinkA::GetPath` (ACP) straight into the strict `SalGetFileAttributes`, so a shortcut **to** a non-ASCII directory is treated as a file. |
| F-P1-26 | **CONFIRMED** | The `DROPFILES` payload is wide and is thrown through CP_ACP at `toolbar5.cpp:168`, `stswnd.cpp:1465`, `editwnd.cpp:1251`; `toolbar5.cpp:189 FileExists` (strict) then rejects the drop. Viewer uses the A `DragQueryFile` (`viewer3.cpp:599`). |
| F-P1-27 | **CONFIRMED** (narrow) | `shares.cpp:105-107` discards the W-only `NetShareEnum` truth; the ACP cache is matched against UTF-8 at `fileswn3.cpp:547` / `drivelst.cpp:1850` and feeds the operational `Shares.GetUNCPath` (`fileswn9.cpp:1864`). |
| F-P1-28 | **REFUTED / LATENT** | **REFUTED** headline: `shiconov.cpp:931` *is* `SalU8ToW(name, …)` and runs first — `:932` is only the invalid-UTF-8 fallback, so badges on `…\Účtenka.pdf` work and 058's fix is complete. **LATENT**: `shiconov.cpp:1017` and `geticon.cpp:126` are ACP-consistent chains into `ExtractIcons` (A, `geticon.h:37`) that lose only icon-file paths outside the ACP, in fallback branches. |

### Cross-cutting notes for the consolidation step

1. **Nothing in batch V5 is language-gated.** Every scenario is driven by path
   bytes, the system ACP and the account/install/TEMP path — never by a
   translated string. The ru/uk/zh LATENT rule does not apply to any of these 27.
2. **Three defects found while verifying that no P1 finding raised**, all worth
   folding into the corresponding fixes:
   - `src/mainwnd5.cpp:1226-1228` and `:1232-1234` — uninitialised `fileSystem[20]`
     read after an ignored `MyGetVolumeInformation` failure (with F-P1-12).
   - `src/shiconov.cpp:161` — `ConvertU2A(…)` at the CP_ACP default degrades the
     Google Drive root even on an ASCII profile, so `IsGoogleDrivePath` can never
     match `G:\Můj disk` (with F-P1-09).
   - `src/shellsup.cpp:538-544` — `IShellLinkA::GetPath` (ACP) into the strict
     `SalGetFileAttributes`; a shortcut to a non-ASCII directory is treated as a
     file (with F-P1-25).
3. **The "UTF-8-first with an ANSI fallback" sinks are load-bearing** and were the
   single most common reason a finding's *consequence* was wrong even when its
   *observation* was right: `src/fileswn4.cpp:711,802-805` (panel items),
   `src/menu3.cpp:927-962` and `src/menu1.cpp:336-371` (menus),
   `src/dialogs3.cpp:1325-1332` (volume label). ACP bytes reaching those sinks
   render correctly, so "mojibake" is almost never the right symptom to predict
   — the right symptom is a *later* strict-facade rejection.
4. **Severity, plainly.** Data/function loss: F-P1-01, 02, 03 (part), 04, 05
   (pack), 06, 07, 08, 09 (part), 10, 12 (part), 13, 19, 20, 21, 23, 24, 25, 26.
   Wrong text/image only: F-P1-14, 22, 27, 28 (latent part), and the free-space
   half of F-P1-12. No defect. F-P1-15, 17, 18 (as stated), 28 (headline), and
   F-P1-16's memory-safety claim.
