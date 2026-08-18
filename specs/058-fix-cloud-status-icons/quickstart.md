# Quickstart: Validating Cloud Sync Status Icons & Non-ASCII Path Fixes

**Feature**: 058-fix-cloud-status-icons
**What is being proven**: badges, base icons and auto-refresh work in
non-ASCII paths (spec SC-001…SC-008); see [research.md](research.md) for the
three root causes (RC1 badges, RC2 activation refresh, RC3 base icons).

## Prerequisites

- Windows 11, VS2022 with C++ Desktop workload, repo cloned.
- Czech (or any non-ASCII-capable) locale not required — test folders are
  created explicitly with diacritic names.
- For cloud checks: OneDrive signed in (ASCII profile path) and Google Drive
  for desktop mounted (default `G:\`, `G:\Můj disk`). The provider-independent
  checks below need **no** cloud client.
- Debug builds emit TRACE; watch traces with the usual TRACE server
  (`src/tserver`) when noted.

## Build

```batch
build.cmd full            :: Debug x64 + runtime data + plugins.ver
:: release verification pass at the end:
build.cmd full release
```

Run: `%OPENSAL_BUILD_DIR%\salamander\Debug_x64\tandemcommander.exe`
(or `.\build\...` when `OPENSAL_BUILD_DIR` is unset).

## 1. Provider-independent repro (RC2 + RC3) — before/after

Setup (any local disk):

```batch
mkdir "D:\Test\Zkouška"
copy <any .docx> "D:\Test\Zkouška\"  & copy <any .pdf> "D:\Test\Zkouška\"
mkdir "D:\Test\Control"
copy "D:\Test\Zkouška\*" "D:\Test\Control\"
```

| Check | Broken (pre-fix) | Expected (post-fix) | SC |
|-------|------------------|---------------------|----|
| List `D:\Test\Zkouška`: Word/PDF icons | generic blank icons | identical to `D:\Test\Control` and Explorer | SC-007 |
| Debug TRACE on entering the folder | `Unable to receive change notifications for directory 'D:\Test\Zkouška'` | no such trace | FR-013 |
| With the panel on `Zkouška`, create a file from another app (`echo x> "D:\Test\Zkouška\new.txt"`) | file does not appear until manual refresh | appears automatically, same delay as in `Control` | SC-008 |
| Alt-Tab away/back 10× with panel on `Zkouška` | busy-cursor flash each activation, nothing changes | no busy cursor (beyond what `Control` shows) | SC-002 |

## 2. Badges in a cloud location (RC1)

OneDrive variant (works even without Google Drive): create a **diacritic**
subfolder inside the OneDrive sync root, put a few files in it, mark some
online-only / always-available via Explorer.

- Pre-fix: items inside `…\OneDrive\Zkouška` show **no** status badges while
  the ASCII sibling folder shows them.
- Post-fix: badges match Explorer item-by-item; on-screen items get their
  badge within 2 s of the listing appearing (SC-001 timing, ~100 items).

Google Drive variant (the reported case): open `G:\Můj disk` next to the
same folder in Explorer.

- Post-fix: every item that carries a badge in Explorer carries the matching
  badge in the panel (SC-001). Note: parity is defined against Explorer on
  the same machine — states Explorer itself does not show (e.g. the
  provider-side "syncing" imperfection known from Altap) are not failures.
- Change a file's state (make available offline / online-only) and refresh:
  badge updates (US1 scenario 2).

## 3. No-regression checks

- OneDrive ASCII folder with mixed states: badges identical to pre-fix
  behavior and to Explorer (SC-005).
- Plain ASCII local folder: icons, listing speed, and activation behavior
  unchanged (FR-009, SC-003 baseline).
- Configuration → the icon-overlays settings page: disable overlays →
  no badges anywhere including `G:\Můj disk` (FR-011); re-enable → back.
- Stop the Google Drive client (`G:` disappears): panel on a former `G:`
  path follows the normal drive-removal flow; no busy-cursor loop, no
  repeated probing (SC-006). Start the client again and re-enter the path:
  badges are back without app restart (FR-010).

## 4. Automated gates

```batch
:: unit tests (includes SalU8ToW coverage)
msbuild src\vcxproj\saltests\saltests.vcxproj -p:Configuration=Debug -p:Platform=x64
%OPENSAL_BUILD_DIR%\salamander\Debug_x64\saltests.exe

build.cmd full release      :: release build must be clean
```

Expected: saltests all-pass (baseline 1145/0 as of feature 056), zero new
warnings at the touched sites.

## 5. Release notes (when shipping)

Per constitution (Release Documentation): add a `Fixed` entry to
`CHANGELOG.md` describing all three user-visible symptoms (badges, busy
cursor, generic icons — in folders with non-ASCII names), and bump
version/build in the same change (`spl_vers.h`, `tandemcommander.iss`,
`CLAUDE.md`). Not part of this feature's code fix unless it ships a release.
