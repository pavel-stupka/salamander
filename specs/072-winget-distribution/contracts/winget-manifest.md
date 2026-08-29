# Contract: winget manifest

What the manifests published for Tandem Commander must state, and why each
value is what it is. Referenced from `tools/winget/publish.ps1`.

Authoritative sources this contract binds together:

| Value | Comes from | File |
|---|---|---|
| Version | `#define MyAppVersion` | `setup/tandemcommander.iss` |
| Release date, release notes | `## [<version>] - <date>` section | `CHANGELOG.md` |
| Installer SHA256 | the published release asset | GitHub Releases |
| Expected signing certificate | `thumbprint` | `tools/codesign/codesign.cfg` |
| ProductCode | `AppId` | `setup/tandemcommander.iss` |
| Everything else | the templates | `tools/winget/templates/` |

## 1. Identity

- `PackageIdentifier` is **`PavelStupka.TandemCommander`** and never changes.
  A different identifier is a different package; installed users are not
  migrated to it.
- `Publisher` is **`Pavel Stupka`**. It must keep agreeing with `AppPublisher`
  in `setup/tandemcommander.iss` and with the subject of the code-signing
  certificate — winget-pkgs validation compares them.
- `Moniker` is `tandemcommander`, giving `winget install tandemcommander`.
- Catalogue path: `manifests/p/PavelStupka/TandemCommander/<version>/`.

## 2. Installer identity and upgrade detection

`AppsAndFeaturesEntries[0].ProductCode` MUST be
`{<AppId>}_is1` — currently
`{35C0B0DC-DB73-429C-AAA8-FBC41C937F66}_is1`.

Inno Setup derives its uninstall registry key from `AppId` by appending
`_is1`. That key is how winget recognises an already installed Tandem
Commander for `winget list`, `winget upgrade` and `winget uninstall`. If
`AppId` in `setup/tandemcommander.iss` ever changes, this value must change
with it in the same commit, or upgrade detection silently breaks: winget will
believe the package is not installed and will keep offering it.

`UpgradeBehavior: install` — Inno Setup upgrades in place over an existing
installation; winget must not uninstall first.

## 3. Install scopes

winget selects a scope by appending a switch from `InstallerSwitches.Custom`.
Inno Setup accepts `/ALLUSERS` and `/CURRENTUSER` only when the installer
permits the override — and **`PrivilegesRequiredOverridesAllowed=dialog`
already does**: Inno Setup documents permitting the dialog as also enabling the
command-line parameters. `setup/tandemcommander.iss` has carried that directive
since before this feature, so **every released version supports both scopes**,
including those published before it.

Verified against a probe installer built with the *exact* privilege
configuration of `tandemcommander.iss` (`PrivilegesRequired` unset = admin,
`PrivilegesRequiredOverridesAllowed=dialog`, `DefaultDirName={autopf}\...`):
`/VERYSILENT /CURRENTUSER` exits 0, installs into
`%LOCALAPPDATA%\Programs\<AppName>` **with no elevation**, and writes the
`HKCU` uninstall key `{AppId}_is1`.

Both installer entries reference the same file and the same hash:

| Scope | Switch | ElevationRequirement | Installs into |
|---|---|---|---|
| `machine` | `/ALLUSERS` | `elevatesSelf` | `%ProgramFiles%\Tandem Commander` |
| `user` | `/CURRENTUSER` | *(absent)* | `%LOCALAPPDATA%\Programs\Tandem Commander` |

- The machine entry's `elevatesSelf` reflects Inno Setup raising its own UAC
  prompt.
- The user entry deliberately carries **no** `ElevationRequirement`. A
  per-user install started from an already elevated shell is legal; it simply
  installs into that profile. `elevationProhibited` would make winget refuse
  the request in an elevated console. (`elevationNotRequired` is not a value
  the schema defines — the enum is `elevationRequired`, `elevationProhibited`,
  `elevatesSelf`.)

**This is the one thing here that a change to the installer could silently
break.** If `PrivilegesRequiredOverridesAllowed` were removed or narrowed,
these manifests would keep advertising a per-user install that no longer
works. `publish.ps1` therefore asserts the directive is present in
`setup/tandemcommander.iss` and refuses to generate without it.

## 4. Platform floor

`MinimumOSVersion: 10.0.19041.0`. winget refuses to install below this value,
so it states what the product can run on (the binaries target
`_WIN32_WINNT=0x0601`; long-path awareness needs Windows 10 1607) rather than
the Windows 11 the project markets. `Architecture: x64` matches
`ArchitecturesAllowed=x64compatible`; Windows on ARM installs it under
emulation.

The Markdown viewer's WebView2 requirement is stated in `InstallationNotes`,
not as a `Dependencies` entry — a dependency would install the runtime for
every user, including those who never open a Markdown file.

## 5. Provenance

- The installer referenced by `InstallerUrl` MUST be signed by the certificate
  whose thumbprint is in `tools/codesign/codesign.cfg`. The generator verifies
  this before producing anything; `-SkipSignatureCheck` exists for testing an
  unsigned local build and must never be used for a submission.
- `InstallerSha256` MUST be computed from the *published* asset. `-LocalFile`
  hashes a local build for rehearsal only; if that build is later rebuilt, the
  hash no longer matches and every install fails the integrity check.

## 6. Generated-file rules

- Generated manifests keep **only** the `# yaml-language-server:` schema line
  plus a one-line provenance header; every other comment is stripped.
  Consequence: **no line inside a YAML block scalar may start with `#`**, or
  it would be removed from the value.
- Release notes are the lead paragraph of the version's `CHANGELOG.md`
  section, flattened to one paragraph, stripped of Markdown emphasis, links
  and inline code, with typographic dashes and quotes folded to ASCII, capped
  at 900 characters.
- Output is UTF-8 without BOM, CRLF, written to
  `tools/winget/manifests/<version>/` and committed.
- `InstallerSuccessCodes` must not list `0` — the schema rejects it; success
  is implicit.
- The locale manifest field for documentation links is `Documentations`
  (plural).
