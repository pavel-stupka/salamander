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

## 3. Install scope

**One installer entry, `Scope: machine`, no `InstallerSwitches`, no
`ElevationRequirement`.** That is what the installer does on its own: Inno
Setup requires administrator rights by default and raises the UAC prompt
itself, and winget handles an Inno installer's elevation without being told.

### Why not per-user as well

A second `Scope: user` entry with `Custom: /CURRENTUSER` (and `/ALLUSERS` on
the machine entry) was submitted in the first pull request,
microsoft/winget-pkgs#426038, and **failed check 08 Installation Validation**
with the label `Validation-Shell-Execute`. It was removed and the package
resubmitted as machine-only.

The Inno Setup half of the mechanism is sound and was verified: a probe
installer built with the *exact* privilege configuration of
`tandemcommander.iss` (`PrivilegesRequired` unset = admin,
`PrivilegesRequiredOverridesAllowed=dialog`, `DefaultDirName={autopf}\...`)
accepts `/VERYSILENT /CURRENTUSER`, exits 0, installs into
`%LOCALAPPDATA%\Programs\<AppName>` with no elevation and writes the `HKCU`
uninstall key `{AppId}_is1`. `dialog` implies `commandline`, so no installer
change was ever needed.

What was never verified — and is recorded in the plan as the open assumption
T017 — is how the **catalogue's validation pipeline** treats such an entry. It
runs manifests in an elevated context (winget-pkgs issue 72224), which makes a
`/CURRENTUSER` install land in the administrator's profile, where the package
is not found afterwards.

Reinstating per-user support therefore needs, in this order: a passing
`Tools\SandboxTest.ps1` run against a manifest that carries the entry, and a
pull request of its own — never folded into one that is also asking for a new
package to be accepted.

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
