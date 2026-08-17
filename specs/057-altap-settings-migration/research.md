# Research: Altap Salamander Settings Migration Utility

**Feature**: 057-altap-settings-migration
**Phase**: 0
**Method**: three parallel codebase investigations (config-version machinery,
plugin/password storage, per-category formats), findings cross-checked against
the live registry on a machine with both Altap Salamander 4.0 and Tandem
Commander 0.1 installed, and the two load-bearing claims re-verified by hand
(`src/mainwnd2.cpp:2590-2596` window-key tolerance;
`src/mainwnd2.cpp:1044-1064` config-exists rule).

---

## R1. Script technology: single-file cmd + Windows PowerShell 5.1 polyglot

**Decision**: One `.cmd` file (`utils/migrate-altap-settings.cmd`) whose batch
header immediately re-invokes the same file's embedded PowerShell 5.1 payload
(`powershell -NoProfile -ExecutionPolicy Bypass ...`), then exits. Pure ASCII,
no BOM (a BOM before the batch header breaks `cmd.exe` parsing). Registry
work uses the .NET `Microsoft.Win32.Registry` classes, not the PowerShell
provider.

**Rationale**: Meets every constraint at once — double-click launchable
(clarification Q2), single self-contained file (FR-001), zero prerequisites on
stock Windows 11 (PS 5.1 is in-box), bypasses the default `Restricted`
execution policy that blocks plain `.ps1` files, and .NET registry classes
give type-exact reads/writes of all value kinds (`REG_BINARY`,
`REG_MULTI_SZ`, `REG_EXPAND_SZ`) plus wide-char string fidelity. Wide-API
fidelity is mandatory: TC pops a modal "Unexpected value type." box for every
wrongly-typed value (`src/regwork.cpp:115-159`), and Altap-era Czech strings
are already correct UTF-16 in the registry — they must be copied without
re-encoding (`src/salamdr6.cpp:2298-2303`; verified live on
`"&Připojit na FTP server..."`).

**Alternatives considered**: plain `.ps1` (rejected: double-click opens
Notepad; execution policy blocks it), pure batch + `reg.exe` (rejected: no
sane interactive checklist, no per-value filtering, no binary-value
handling), compiled C++/C# exe (rejected: user asked for a script; adds a
build product and signing questions), PowerShell 7 (rejected: not in-box).
Prompting uses stdin-safe reads (`[Console]::In.ReadLine()`-style), not
console-handle APIs, so the test harness can drive the wizard by redirected
stdin (wizard-flow contract).

## R2. Source discovery: the historical root list, verbatim

**Decision**: The utility scans the exact list of per-user registry roots
that upstream Salamander itself used for auto-import — the pre-feature-032
content of `SalamanderConfigurationRoots[]` (recovered from git,
`3945ecf:src/mainwnd2.cpp`) — minus `Software\Open Salamander\5.0` (out of
the spec's Altap/Servant scope). That is ~79 roots from
`Software\Altap\Altap Salamander 4.0` down to `Software\Salamander` (1.52).
A root qualifies as a migration source by the same rule TC applies to its
own root: the root exists **and** contains a `Configuration` subkey
(`src/mainwnd2.cpp:1044,1055-1064`). Newest qualifying root is the W3
default.

**Rationale**: This list is the product's own authoritative enumeration; it
also captures traps a hand-built list would miss — the 2.5x-era build tags
contain a space (`"(DB 72)"`, `"(IB 55)"`) while 3.x/4.0 tags do not
(`"(DB177)"`), and `SalamanderConfigurationVersions[]` normalizes the names
differently, so registry paths must come from the roots array only.

**Alternatives considered**: scanning `Software\Altap\*` by wildcard
(rejected: misses `Software\Salamander`, includes non-Salamander Altap
products like the standalone shell-extension key); including Open
Salamander 5.0 (deferred: trivially addable later, but outside the clarified
scope).

## R3. Source generation and the config-version floor

**Decision**: The source's `Version\Configuration` DWORD (missing key ⇒ 1,
missing value ⇒ 2, same as TC's loader, `src/mainwnd2.cpp:2373-2381`)
classifies the source: **AS4** (≥ 100), **AS3** (66–99), **AS25** (39–65),
**Ancient** (< 39, Servant-branded era). All categories are offered for
config ≥ 39 (Altap Salamander 2.5+, the spec's stated range); for Ancient
sources the `colors` and `viewtemplates` categories are additionally skipped
(their format conversions predate 2.5 and are not reproduced), everything
else remains offered best-effort. Product-version ↔ config-version mapping
comes from the product's own table (`src/mainwnd2.cpp:26-145`): AS 4.0 = 103,
3.08 = 101 … 3.0 = 84, 2.54 = 63, 2.52 = 46/48, 2.5 = 39; TC = 105.

**Rationale**: Between config 39 and TC's 105 the complete set of
version-gated load conversions (exhaustively enumerated from `ConfigVersion <`
sites) touching offered categories reduces to exactly two transforms the
utility performs itself — R5 — plus gates that only affect skipped material.
Below 39 the conversion chain gets long (color scheme remap < 28, view
template #6 reset < 23, filter merge < 22, highlight injection < 16 …) for
users that realistically no longer exist; best-effort per clarification Q4.

## R4. Version stamping: the destination is always a current-format config

**Decision**: The utility never copies the source's `Version` key. It writes
values already in TC-current shape and ensures the destination carries
`Version\Configuration = 105` (creating it, and creating an empty
`Configuration` subkey, when the destination is virgin). It never lowers an
existing destination version value.

**Rationale**: Seeding an older version number would make TC run its global
in-place upgrade at startup: forced plugin auto-install + immediate
`SaveConfig` (`src/salamdr1.cpp:4508-4539`), and — decisive — the feature-010
gate `ConfigVersion < 105` discards the whole `Packers & Unpackers` content
and rebuilds it from defaults (`src/mainwnd2.cpp:2728-2733`), which on a
non-virgin destination would destroy the **user's own TC packer settings in
an unselected category**, violating FR-009. Stamping 105 keeps TC's loader
inert; the two conversions the utility then owes are trivial (R5). The
virgin-destination provisions come from verified loader behavior: a root
without a `Configuration` subkey is ignored entirely and overwritten with
defaults at first save (`src/mainwnd2.cpp:1044`, `src/salamdr1.cpp:4212-4213,
4532-4539`), while a missing `Window` key merely returns FALSE after loading
everything else (`src/mainwnd2.cpp:2590-2596` — verified by hand).

**Alternatives considered**: copying the source version and letting TC's own
conversion chain upgrade (rejected: packers wipe + startup save churn +
plugin auto-install side effects, and conversions would also run over TC's
current-format values in unselected categories).

## R5. The only two transforms (sources ≥ 39)

**Decision**: (a) sources with config < 47: double every `$` in hot-path
`Path` values (TC would do this only under its own version gate,
`src/mainwnd1.cpp:280-282`, which R4 disarms); (b) sources with config < 44:
lowercase the extension parts of `Masks` values in Viewers / Alternative
Viewers / Editors (`src/salamdr2.cpp:2583,2682`). Everything else offered is
byte-stable between config 39 and 105.

**Rationale**: Direct consequence of R3's exhaustive gate enumeration; no
other `< N` gate with N in (39, 105] touches an offered category (the < 105
packers gate affects only the skipped packers category; `Show Translation Is
Incomplete`'s `== 105` gate is satisfied by R4's stamp).

## R6. Plugin config matching and the FTP category

**Decision**: Plugin configuration lives under
`<root>\Plugins Configuration\<Configuration Key>`; the key name is a stable
literal supplied by each plugin (`"FTP"`, `"ZIP"`, `"7zip"`, `"Checksum"`,
`"DBVIEWER"`, `"DISKMAP"`, `"File Comparator"`, `"PEVIEWER"`, `"PictView"`,
`"RegEdit"`, `"Renamer"`, `"UnCAB"`, `"UNDELETE"`, `"UnISO"`), only
uniquified on collision (`src/plugins2.cpp:2312-2329`). The utility matches
source→destination plugin config by joining the `DLL` value across both
roots' `Plugins\<n>` registrations (numeric subkeys are registration-order
specific — FTP is `Plugins\13` in AS 4.0 vs `Plugins\9` in TC on the
reference machine — never match by number); when the destination has no
registration yet (virgin TC), the known literal key name is used directly.
The whole `Plugins Configuration\<key>` subtree is copied verbatim
**including the plugin's own `Version` value** (it drives the plugin's
internal config upgrades — FTP's schema version is 37 in both products,
verified live). The `Plugins` registration key itself (DLL paths, icon
caches, function masks, menu cache, `LastSLGName`, `HomePage` …) is **never**
copied — pure installation metadata, and TC rewrites that key wholesale on
every save (`src/plugins2.cpp:1587`).

**Rationale**: verified live on both installs; plugin-internal versioning
makes plugin categories robust across all source generations without
utility-side transforms. FTP bookmarks (`…\FTP\Bookmarks\1..n`), proxies and
server types are numbered lists whose loaders read `1..n` until a gap —
destination list subkeys must be deleted before writing (replace semantics
already require this).

**Shipped-plugin category set**: `ftp` is its own top-level category (the
spec names it); the other 13 literals above form the `plugin-configs`
category. Not migratable and reported as such when present in the source:
configs of plugins TC does not ship (Automation, CHECKVER, IEVIEWER,
MMVIEWER, nethood, UnARJ, UnMIME, UnRAR, SplitCombine, Encrypt & Decrypt,
WMOBILE — note portables ≠ wmobile: different DLL name, no saved config),
and `tar` (writes only a `Version` value — nothing to migrate). `sftp` and
`mdview` are Tandem-only (nothing to migrate from).

## R7. Passwords: fully portable, master-password caveat handled

**Decision**: FTP bookmark secrets are copied as-is. `PasswordS` blobs
(signature byte 1) are scrambled with a hardcoded table — portable anywhere,
no user action needed. `PasswordE` blobs (signature byte 2) are
WinZip-AES-256 (PBKDF2, salt embedded in each blob, MAC) keyed **only** by
the master password string — no DPAPI, no machine/user/root binding
(`src/pwdmngr.cpp:481-583`, `src/pwdmngr.h:84-92`). Handling:

- Source has master password, destination does not: copy the source's
  `Password Manager` pair (`Use Master Password` + `Master Password
  Verifier`, atomically — both or neither, `src/pwdmngr.cpp:820-854`) and
  tell the user their existing master password now guards TC (FR-010 NOTES).
- Destination already uses its own master password: **never overwrite the
  destination verifier** (it protects TC-side secrets, e.g. SFTP). Encrypted
  source blobs would be undecryptable under the destination's different
  master password → those bookmarks transfer without the secret and the
  summary says the password must be re-entered (FR-010). If the two master
  passwords happen to be identical the blobs remain usable; the utility
  cannot verify this (it never asks for the master password) and the summary
  says so.
- Neither uses a master password: nothing to do (`PasswordS` just works).

**Rationale**: crypto verified in source and against a live `PasswordS` blob
(1 + 17-byte scramble padding formula matched); atomic-pair rule comes from
the loader, which reads the verifier only when the flag is TRUE.

## R8. The `Configuration` category: exclude-list, not allow-list; overwrite-in-scope

**Decision**: The general-configuration category copies the `Configuration`
key's scalar values plus the subkeys `Drive Special Settings`,
`Copy Move Options`, `Find Options`, `Find Ignore`, minus a precise
exclude-list (category-mapping contract): language selection
(`Language`, `Use/Alternate Language for Plugins`, `Language Changed` — TC
ships a different language set; a missing `.slg` causes a startup dialog),
plugin-install counters (`Plugins.ver Version (x64)/(x86)`), machine-specific
shell-overlay lists (`Enable/Disabled Custom Icon Overlays`), toolbar
button-layout strings (`Top/Middle/Left/Right ToolBar` — command ids drifted
post-fork; visibility flags are copied), `Conversion Table`, `Main window
icon index`, dialog/session geometry values, and the never-copy set. History
subkeys and `Working Directories` are excluded as transient session state
(spec assumption). Replacement semantics for this category: delete owned
scope first, **except the TC-only value `Theme Mode`** (feature 028 dark-mode
selector — no Altap counterpart; wiping it would reset the user's theme,
which no Altap data replaces).

**Rationale**: every value is read independently with a compiled default
(`src/mainwnd2.cpp:3021-3591`), so partial copies are safe; the value
vocabulary is unchanged post-fork except `Theme Mode`. Unknown Altap-only
value names are never read by TC and — since `Configuration` is not cleared
on save — would linger; the exclude-list plus the copy being name-driven
keeps harmful ones out, and inert leftovers are acceptable (documented).

**Alternatives considered**: allow-list of ~150 known names (rejected:
heavy to maintain, no safety gain — the exclude-list covers every value with
cross-installation meaning, and same-name values have had stable types since
the 2.5 era; color values even have a dual-type-tolerant reader,
`src/salamdr2.cpp:2241-2277`).

## R9. Categories deliberately not offered

**Decision** (each reported with its reason when present in the source,
FR-011):

- **Packers & Unpackers** (custom packers/unpackers, predefined packers,
  archive associations): TC deliberately rebuilds these from defaults for
  any pre-105 config (feature 010 encoding baseline,
  `src/mainwnd2.cpp:2728-2733`); additionally the entries carry positional
  indexes into the plugin/packer tables, which differ between the products.
  Bypassing a reset the product itself enforces is out.
- **Internal ZIP Packer**: dead key, no reader in TC.
- **Left/Right Panel, Window, Find Dialog Window**: window geometry and
  session state (current paths, sort, filter) — excluded per spec assumption.
- **Histories** (14 `Configuration\* History` subkeys + `Working
  Directories`): transient session state.
- **Configs of plugins TC does not ship** (R6 list).
- **Root markers**: `Save In Progress` (its presence on the *source* root
  additionally makes the utility warn that the source may carry an
  interrupted save), `AutoImportConfig`, `Copy Is OK`, the `Version` key —
  never copied in any category (FR-005).

**Rationale**: FR-005 (nothing that could put TC in an invalid state),
feature-010's documented intent, spec assumption on session state.

## R10. Backup, restore, refusal checks

**Decision**: Backup = `reg.exe export` of the entire destination root to a
timestamped `.reg` before the first write (mandatory; export failure aborts
with zero writes), plus a generated `restore.cmd` that deletes the current
destination root and imports the backup (or only deletes, when the root did
not exist pre-run — recorded at backup time). Running-process refusal checks
for `tandemcommander` and `salamand` process names at W2 and again
immediately before the first write. Output files land next to the script,
falling back to `%USERPROFILE%\Documents`.

**Rationale**: `.reg` + delete-then-import is the only restore that removes
keys *added* by the migration (a plain import merges); `reg.exe` is in-box
and its export format is stable. TC saves its whole configuration on exit
(`CMainWindow::SaveConfig`) — a running instance would overwrite migrated
data, hence the hard refusal (FR-008). The corrupted-config marker semantics
(`Save In Progress` ⇒ TC offers to delete the whole root,
`src/mainwnd2.cpp:928-1043`) reinforce why the utility must never leave that
value behind.

## R11. Test strategy

**Decision**: `utils/test/run_migration_tests.cmd` drives the wizard end to
end via redirected stdin against fixture hives imported under
`HKCU\Software\TCMigTest\`, using the four documented test-only environment
overrides (wizard-flow contract). Fixtures are `.reg` files modeling: full
AS 4.0 config (incl. `PasswordS`/`PasswordE` blobs, a foreign plugin's
config, a version marker, an Altap-install-dir reference), a minimal
2.5-era config, and a pre-populated destination. Every scenario asserts
source byte-immutability (export compare) and the quickstart's per-scenario
outcomes.

**Rationale**: real end-to-end coverage without touching real product roots;
`.reg` fixtures are reviewable and diffable; stdin-driving matches the
wizard contract exactly.
