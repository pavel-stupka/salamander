# Tandem Commander — Project Context

## What Is This?

Tandem Commander is a two-panel file manager for Windows, derived from
Open Salamander (open-sourced under GPLv2 in 2023). It is a pure
WinAPI C++ application — no MFC, no Qt, no cross-platform frameworks.

## Product Identity (established in feature 032, renamed in feature 046)

- **Product name**: Tandem Commander, version **0.1.4** (internal build 188);
  released versions and what changed in each are recorded in `CHANGELOG.md`
  (mandatory per the constitution: a release bumps
  `VERSINFO_SALAMANDER_MINORB` + `VERSINFO_BUILDNUMBER` in
  `src/plugins/shared/spl_vers.h`, `MyAppVersion` in
  `setup/tandemcommander.iss`, and this line, in the same change as the
  changelog entry; the plugin interface version
  `LAST_VERSION_OF_SALAMANDER` is independent and changes only with the
  plugin API). Version 0.1.0 was the first public release;
  known as Newt Commander before feature 046 — the rename covered every
  user/OS-visible surface, kernel-object/IPC names, URLs, translations and the
  installer (new AppId), with **no** config import from the old registry root
- **Binary**: `tandemcommander.exe` (set via `<TargetName>` in `salamand.vcxproj`)
- **Registry root**: `HKCU\Software\Tandem Commander\0.1` — never reads or
  writes Open Salamander/Altap/Newt Commander registry keys (no config import)
- **Websites**: https://tandemcommander.org · repo github.com/tandemcommander/tandemcommander
- **Copyright rule**: years up to 2026 → "Open Salamander Authors",
  2026 onward → **Pavel Stupka** (sftp+mdview plugins are solely his).
  The holder name is defined **once**, as `VERSINFO_HOLDER_TANDEM` in
  `src/plugins/shared/spl_vers.h`; every notice concatenates it
  (`"… , © 2026 " VERSINFO_HOLDER_TANDEM`) and never spells it out — that
  covers all 30 `versinfo.rh2` files, the standalone `.rc` files
  (salmon, shellext, zip sfx trio, fcremote, salpvenv) and the two
  hardcoded strings in `plugins2.cpp` / `zip/add_del.cpp`. The two
  notices shown in the About dialog and on the splash screen live in
  `src/versinfo.rh2` (`VERSINFO_COPYRIGHT_TANDEM` above
  `VERSINFO_COPYRIGHT_OPENSAL`) and are never translated — the About
  controls carry an empty caption in `lang.rc`. Do not look for this
  text in the language files (feature 040).
- **IMPORTANT**: source files, functions, classes, project/solution names
  (`salamand.sln`, `salamand.vcxproj`, `SALAMANDER_*` constants) deliberately
  keep their upstream names — rename only user/OS-visible identity
- **Brand assets**: `tools/brand/` — hand-swappable sources (feature 035):
  `icon-master.png` (+ optional `icon-<N>.png` overrides) → all shipped
  `.ico`; `about.png` → `src/res/logo.png` (About + splash artwork);
  `python tools/brand/gen_icons.py` regenerates everything, see
  `tools/brand/README.md`

## Technology

- **Language**: C++ (C++20, `/std:c++latest`)
- **Compiler**: MSVC v143 (Visual Studio 2022)
- **Platform**: Windows 11+, pure WinAPI
- **Build system**: MSBuild (`.sln` / `.vcxproj` / `.props`)
- **Plugin format**: `.spl` (plugin DLL) + `.slg` (language resource)

## Repository Structure

```
src/                   All source code (~2,224 files)
  common/              Shared libraries and headers
    dep/               Third-party libs (zlib, bzip2, sqlite, fmt, wil...)
  plugins/             28 plugins (archive, viewer, utility, network)
    shared/            Shared plugin build infrastructure
  vcxproj/             VS solution (salamand.sln) and project files
  lang/                English resources for main app
  salmon/              Crash reporter
  shellext/            Shell extension (x86 + x64)
  setup/               Installer/uninstaller
architecture/          Architecture documentation (see below)
convert/               Character conversion tables
doc/                   License files, third-party notices
help/                  User manual source (HTML Help)
tools/                 Build utilities (code signing, timing)
translations/          UI translations
```

## Build Quick Start

```batch
set OPENSAL_BUILD_DIR=D:\Build\OpenSal\
build.cmd                       :: Debug x64 incremental build (from repo root)
build.cmd rebuild               :: Full clean + rebuild Debug x64
build.cmd full                  :: Complete build: also copies runtime data
                                ::   (convert tables, toolbars, scripts) and
                                ::   generates plugins\plugins.ver so all
                                ::   enabled plugins auto-register in
                                ::   Plugin Manager
build.cmd full release          :: Complete Release x64 build
```

**Plugin build policy**: `plugins.cfg` in the repository root decides
which plugins are compiled and shipped (`name=on|off`, one line per
plugin; currently 18 on / 10 off). Every `build.cmd` run validates the
file, builds only enabled plugins (via a generated solution filter
`src\vcxproj\salamand.gen.slnf`, gitignored), and removes outputs of
disabled plugins. See `specs/007-plugin-build-policy/`.

**Language build policy**: `translations/languages.cfg` decides which
languages are built and shipped — each `[folder]` section carries
`enabled = on|off`, the language counterpart of `plugins.cfg`. Every
`build.cmd` run validates the registry and reconciles the output tree
(any `.slg` not belonging to an enabled language is deleted from `lang\`
and `plugins\*\lang\`); language modules are *produced* only on a full
build. Currently 8 of 11 enabled — Simplified Chinese, Russian and
Ukrainian are off pending a menu rendering defect; their translation
source is retained, so re-enabling is one line. Authoring tools skip
disabled languages by default (`translate.merge --language <folder>` is
the opt-in). See `specs/039-language-build-policy/`.

Alternative scripts in `src\vcxproj\`: `build.cmd` (simple), `rebuild.cmd` (interactive menu) — these build the full solution and ignore `plugins.cfg`.

**Prerequisites**:
- Windows 11 or newer
- Visual Studio 2022 (Community, Professional, or Enterprise)
- "Desktop development with C++" workload installed in VS2022
- Windows 10/11 SDK (any version; projects use `10.0` = latest installed)
- Environment variable `OPENSAL_BUILD_DIR` (optional — defaults to `.\build\`)

## Key Facts

- **76 projects** in salamand.sln (1 main app, 28 plugins, 29 lang
  modules, 7 helper libs, 5 utilities, 2 shell exts, 3 setup, 1 other)
- **Plugin set is policy-driven**: 8 obsolete plugins were removed in
  feature 007 (pak, unarj, unlha, unfat, wmobile, ieviewer, splitcbn,
  winscp); `plugins.cfg` disables 10 more by default (demos and
  marginal plugins), so a default build ships 18 plugins
- **All dependencies are embedded** — zero NuGet packages
- **Missing deps**: unrar.dll (unrar), OpenSSL (ftp); pictview runs on
  the built-in Windows WIC engine since feature 006 (no pvw32cnv.dll
  needed)
- **Encoding**: UTF-8-BOM, formatted with clang-format
- **Comments**: Legacy Czech OK, new comments in English
- **Debug builds** use fixed base addresses (no ASLR) for leak detection
- **Release builds** use LTO/WPO and code signing

## Architecture Documentation

Detailed analysis is in the `architecture/` directory:

| Document | What It Covers |
|----------|---------------|
| [01-project-overview.md](architecture/01-project-overview.md) | History, tech stack, repo layout |
| [02-solution-structure.md](architecture/02-solution-structure.md) | All 90 projects with categories |
| [03-build-pipeline.md](architecture/03-build-pipeline.md) | Build scripts, configs, output paths |
| [04-dependencies.md](architecture/04-dependencies.md) | Third-party libs, missing deps |
| [05-compiler-comparison.md](architecture/05-compiler-comparison.md) | MSVC vs Clang-cl vs MinGW vs Intel |
| [06-plugin-architecture.md](architecture/06-plugin-architecture.md) | Plugin API, .spl/.slg format |
| [07-preprocessor-defs.md](architecture/07-preprocessor-defs.md) | All #defines by configuration |
| [08-code-standards.md](architecture/08-code-standards.md) | Encoding, formatting, conventions |
| [09-plugin-catalog.md](architecture/09-plugin-catalog.md) | All 36 plugins categorized by purpose |
| [10-plugin-maintenance-outlook.md](architecture/10-plugin-maintenance-outlook.md) | Per-plugin 2026+ maintenance assessment (Czech) |
| [11-webview2-integration.md](architecture/11-webview2-integration.md) | **Binding contract** for any plugin embedding WebView2: canonical user data folder, single browser-arguments helper, keeper pattern (warm shared engine) |

## Compiler Recommendation

- **Primary**: MSVC 2022 (full compatibility, zero effort)
- **Secondary CI**: Clang-cl (catches extra bugs, MSBuild-compatible)
- **Not viable**: MinGW-w64 (no x86 SEH, no MSBuild)

## Plugin Build Pattern

Each plugin: `plugins/<name>/vcxproj/<name>.vcxproj` → outputs `<name>.spl`
Each language: `plugins/<name>/vcxproj/lang_<name>.vcxproj` → outputs `english.slg`
Property sheets: `plugins/shared/vcxproj/plugin_base.props` + debug/release variants

## Branching Strategy

- **`main`** — upstream/stable branch
- **`ai-main`** — main branch for AI-assisted development
- Feature branches (e.g., `003-speckit-review`) are created from and merged into `ai-main`

## Constitution

Project principles are in `.specify/memory/constitution.md`
("Tandem Commander Constitution", v3.0.0): build reproducibility,
backward compatibility (baseline Tandem Commander 0.1.0 — the break
with Open Salamander 5.0 was made in feature 032, the Newt→Tandem
rename in feature 046; both deliberate, documented, one-time),
incremental modernization, Windows platform commitment,
plugin architecture preservation, UI consistency.

<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->

## Active Technologies
- Windows Batch script (.cmd) + MSBuild (from VS2022), vswhere.exe (002-msvc-x64-build-script)
- C++ (C++20, `/std:c++latest`), MSVC v143 (VS2022) + Pure WinAPI (no frameworks); internal shared libs (`src/common/`); no new external dependencies (004-long-paths-unicode)
- Windows Registry for configuration (`REG_SZ` string values); NTFS/exFAT/FAT/network file systems as managed objects (004-long-paths-unicode)
- Translation data: `translations/<language>/<module>.slt` UTF-8-BOM text archives, committed; consumed at build time by `translator.exe` quiet modes to produce `<language>.slg` (038-translations-build-integration)
- Python 3.13 (`tools/`, `pyproject.toml`) + `anthropic` SDK for offline machine translation — developer-side only, never invoked by the build (038-translations-build-integration)
- Language build policy: `translations/languages.cfg` `enabled = on|off` per language; validated and reconciled by `src/vcxproj/lang_policy.ps1` on every `build.cmd` run (039-language-build-policy)
- Code signing: `tools/codesign/codesign.cfg` (committed profile: certificate SHA-1 thumbprint + Certum timestamp URL) consumed by `tools/codesign/sign_release.ps1` (Windows PowerShell 5.1 sweep, idempotent) and `setup/build_setup.cmd`; signtool.exe from the Windows SDK; Inno Setup 7 for the installer (050-code-signing)
- SFTP plugin: vendored libssh2 1.11.1_DEV (`src/common/dep/libssh2`) on the WinCNG backend (`LIBSSH2_WINCNG` + `LIBSSH2_ECDSA_WINCNG` — RSA/ECDSA only, no ed25519, no OpenSSL); test harness `src/plugins/sftp/test/` runs against a local Docker reference server (container `tandem-sftp`, localhost:2222) (051-fix-sftp-keyauth-hang)
- **WebView2 shared-engine contract** — MANDATORY for any plugin embedding
  WebView2 (planned: formatted source viewer, WebGPU), see
  `architecture/11-webview2-integration.md`: one canonical user data folder
  `%LOCALAPPDATA%\Tandem Commander\WebView2` (a different UDF spawns a
  separate cold browser tree), one browser-arguments set built by the shared
  options helper in `src/plugins/mdview/webview.cpp` (later environments'
  args are silently ignored — extensions are coordinated helper changes,
  never per-plugin overrides), per-controller security stays per-plugin,
  and each plugin arms its own session-long keeper at its own first use
  (any one live controller keeps the warm tree for all). SDK vendored at
  `src/common/dep/webview2/` (v1.0.4078.44); second consumer lifts the
  helper to `src/common/` instead of copying it (065-mdview-instant-render)

## Recent Changes
- 002-msvc-x64-build-script: Added Windows Batch script (.cmd) + MSBuild (from VS2022), vswhere.exe
- 038-translations-build-integration: 12 shipped languages (English + 10 existing + new machine-translated Ukrainian) x 20 enabled modules; `.slt` import is strictly positional, so translation source is always regenerated from a current-structure English template
- 039-language-build-policy: which languages ship is now a committed policy (`enabled = on|off` in `translations/languages.cfg`), honoured by the build on every run; 3 non-Latin-script languages disabled pending a menu rendering defect, source retained
- 046-tandem-commander-rebrand: product renamed Newt Commander → Tandem Commander (`tandemcommander.exe`, registry root `HKCU\Software\Tandem Commander\0.1`, TandemCommander*/TCExten_* kernel/IPC names, tandemcommander.org, new installer AppId, new icon/artwork from `tools/brand/`); no config migration; upstream `salamand*`/`SALAMANDER_*` names retained
- 050-code-signing: on-demand release signing — `build.cmd full release sign [setup]` signs all shipped PE artifacts (exe/dll/spl/slg, ~206 files) via idempotent sweep `tools/codesign/sign_release.ps1` + compiles a signed Inno Setup installer (`setup/build_setup.cmd [sign]`, `#ifdef SIGN` in the .iss); default builds never sign (per-target hook `sign_with_retry.cmd` is a no-op unless `TC_CODESIGN=1`); Release trees no longer contain `.pdb/.lib/.exp` (redirected to `obj\` by `src/Directory.Build.targets`, cleaned + installer-excluded as safety nets)
- 052-fix-plugin-name-encoding: Plugins Manager showed mojibake names of
  not-loaded plugins in non-English UI. Root cause: `CPluginData::Name` had no
  defined encoding — CP1250 from a loaded plugin (`LoadStringA`) but UTF-8 from
  the feature-004 registry facade — and the name column used the ANSI listview
  call. Fix: plugin metadata is **UTF-8 by contract** (normalized at intake via
  `SalLegacyToU8Alloc`, see `specs/052-.../contracts/plugin-metadata-encoding.md`),
  name column renders via `SalListViewSetItemTextU8`, 15 mixed-composition
  sites converted to `LoadStrU8`, `tools/check_encoding.py` tracks the contract
  identifiers, **`build.cmd` now fails when python is missing** (guard can't be
  silently skipped). ZIP plugin renamed to literal "ZIP" in all languages
  (was machine-translated as "postal code" in cs/sk/fr/es/zh), pinned in
  `translations/ui-overrides.json`. No registry migration — stored values were
  verified intact; the defect was display-only.
- 056-prerelease-review: release gate for **0.1.2** (build 186, released
  2026-08-07). Multi-agent review (6 independent perspectives — memory,
  concurrency, network security, credentials, encoding, tooling/data — with
  adversarial verification) over the whole `v0.1.1..HEAD` delta (features
  052–055). All four code-safety perspectives judged the delta itself clean;
  the only shipped-product regression was F1 (SFTP: Duplicate on the now-transient
  Quick Connect row produced an empty bookmark — `dialogs.cpp` gates it on
  `isBookmark` like Save/Rename/Delete). Deferred, non-shipping: a dev-only
  `addrows.py` bug left the 3 disabled languages' `sftp.slt` 5 rows short (fix
  before re-enabling them); pre-existing `plugins1.cpp` fixed-buffer patterns.
  Gates G1–G9 green (full Debug+Release build, saltests 1145/0, SFTP harness
  7/7 + leak check, key-format fixtures 66/0, slt round-trip, cs+en smoke,
  version sweep). Report: `specs/056-prerelease-review/review-report.md`.
- 055-contextual-retranslation: every machine-provenance UI string outside the
  SFTP plugin (≈3,300 entries, 8 enabled languages × 19 modules) re-translated
  with usage context — the feature-051 method applied product-wide. Tooling:
  `translate.merge --redo-machine` (demotes all `machine` `.origin` entries to
  gaps; human/skip untouched by construction) + repeatable `--exclude-module`;
  `uicontext._DOMAINS` now covers all 20 enabled modules. Latent pipeline
  defects fixed: translations identical to their English were re-sent to DeepL
  on every run (match.py now trusts the sidecar), `dedupe_accelerators` was
  exponential on salamand's large menus (proper Kuhn visited-set sharing:
  >4 min → 0.3 s per language) **and rewrote accelerators inside human
  translations** (human/skip rows are now frozen obstacles), overrides that
  matched the engine's output lost their `human` provenance, contexts were
  built 16× instead of once. 20 pins added to `ui-overrides.json` (mdview
  View-menu strings across 7 languages, theme names, plugin name). Run cost
  52,728 DeepL chars; verification: provenance-scoped diff over 59,360
  entries proves human/skip entries byte-identical (0 violations); details in
  `specs/055-contextual-retranslation/run-notes.md`.
- 051-fix-sftp-keyauth-hang: fixed whole-app freeze on private-key connect. Root cause was in vendored libssh2: `_libssh2_pem_parse_memory`'s scan loops never terminate when the expected PEM marker is absent (`readline_memory` cannot signal EOF), and the WinCNG in-memory loader only understood classic RSA/DSA PEM — so an OpenSSH-container key (ssh-keygen's default since OpenSSH 7.8) spun the CPU forever on the UI thread. Patched pem.c (bounds guards, documented in `src/common/dep/libssh2/readme.txt` "Local patches"), added openssh-key-v1 RSA/ECDSA import + classic-PEM passphrase decryption to the WinCNG memory path, real libssh2 error codes on key-load failures. Plugin side: connect runs on a worker thread with a cancellable wait window (prompts stay on the UI thread via a `cpHostKey`/`cpPassphrase`/`cpPassword` retry handshake), the socket stays non-blocking so libssh2's timeout is actually enforced, key-format gate rejects PKCS#8/ed25519/.ppk up front, error classification by code (not message substrings), password fallback on a server-rejected key, dead-transport detection + reconnect, cancellable F3 download. Test harness reworked onto the product's `publickey_frommemory` path with a hang watchdog (`test/run_keyauth.cmd`, 7 scenarios) plus key-format fixtures in `test/build_and_run.cmd`
- 058-fix-cloud-status-icons: three feature-004 regressions-by-omission
  garbled the UTF-8 panel path in code still treating it as ANSI, breaking
  every folder whose path contains non-ASCII characters (e.g. Google Drive's
  `G:\Můj disk`): no cloud sync-status overlay badges (icon-reader wide
  prefix converted via CP_ACP, `fileswn1.cpp`), generic file icons
  (`SHILCreateFromPath` CP_ACP, `geticon.cpp`), and silently dead
  auto-refresh causing a busy-cursor re-list on every window activation
  (ANSI `FindFirstChangeNotification` in `snooper.cpp`, 3 sites). All three
  converted to the house pattern `SalU8ToW`/`SalU8ToWAlloc` + CP_ACP
  fallback (legacy plugin callers of `GetFileIcon` keep working); new W
  overload in the HANDLES layer. The provider was never the trigger — ASCII
  OneDrive paths worked all along. Contract:
  `specs/058-fix-cloud-status-icons/contracts/path-encoding-icon-pipeline.md`.
- 059-fix-onedrive-syncing-badge: the sync-in-progress badge (blue arrows)
  now shows as in Explorer. Windows exposes cloud state through two
  channels; folders in a pending state are claimed by NO overlay handler
  (all seven OneDrive `IsMemberOf` return S_FALSE) — Explorer draws them
  from `PKEY_StorageProviderState` (documented "Property for the cloud file
  state icon"), which the overlay-only pipeline never read (missing since
  Open Salamander). Fix: property fallback in
  `CShellIconOverlays::GetIconOverlayIndex` — only when every handler
  declined AND the panel path is under a CFAPI sync root
  (`CfGetSyncRootInfoByPath`, cldapi.dll dynamic; `G:` letter drives are
  not CFAPI → unchanged); states {4,5,6,10} map to the synthetic overlay
  `TandemCloudSyncPending` (own icon `src/res/syncpend.ico`, generator
  `tools/brand/gen_overlay_syncpend.py`; disable-able via the existing icon
  overlay config). `GPS_DELAYCREATION|GPS_BESTEFFORT` keeps content
  property handlers from running (no hydration, no failures on malformed
  documents). Integration exposed + fixed a latent upstream RTC bug:
  uninitialized `HRESULT res` in `GetIconOverlayIndexAuxAux` when a reader
  slot is NULL. `cfapi.h` cannot be included at `_WIN32_WINNT=0x0601` —
  the two needed ABI-stable declarations are mirrored locally.
