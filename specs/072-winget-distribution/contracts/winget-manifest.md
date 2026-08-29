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

### The installer must survive a silent install

This is the requirement everything else here rests on, and it was **not met**
by any release up to 0.1.6. The catalogue installs every submitted package
unattended; if `/VERYSILENT` fails, no manifest can be written that passes.

Tandem Commander's installer aborted with exit code 1 and
`Failed to proceed to next wizard page` because its AI disclaimer page keeps
the *Next* button disabled until a checkbox is ticked, and a silent install
still traverses the wizard's pages. Fixed in 0.1.7 by guarding
`CurPageChanged` with `not WizardSilent` in `setup/tandemcommander.iss`. Proven
with a pair of probe installers differing only in that guard: without it exit 1
and the same log line, with it exit 0 and `Installation process succeeded.`

Before submitting any future version, `/VERYSILENT` must be verified on the
built installer - quickstart section 2b.

### Why there is no per-user entry, and what is still unknown

The first submission carried a second `Scope: user` entry with
`Custom: /CURRENTUSER`, plus `/ALLUSERS` on the machine entry. It failed check
08 Installation Validation with the label `Validation-Shell-Execute`, and was
removed on the theory that the pipeline runs manifests elevated so a
`/CURRENTUSER` install lands in the administrator's profile.

**That theory was wrong.** The machine-only manifest failed the same check in
the same way, because the installer could not be installed silently at all.
Whether a per-user entry works is therefore **still untested** - the real
defect masked it.

What is known: Inno Setup does accept the switches.
`PrivilegesRequiredOverridesAllowed=dialog` implies `commandline`, and a probe
with the installer's exact privilege configuration took
`/VERYSILENT /CURRENTUSER`, exited 0, installed into
`%LOCALAPPDATA%\Programs\<AppName>` with no elevation and wrote the `HKCU`
uninstall key `{AppId}_is1`.

If per-user support is wanted, it is a change of its own: add the entry, prove
it with `Tools\SandboxTest.ps1`, and submit it separately - never folded into a
submission that is also asking for something else.

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
