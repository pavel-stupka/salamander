# Feature Specification: winget Distribution

**Feature Branch**: `072-winget-distribution`
**Created**: 2026-08-29
**Status**: Implemented
**Input**: User description: "Kdyz vydam novou verzi programu, napr. posledni 0.1.5, tak je nasledne, az ji nahraji na GitHub dostupna na URL: https://github.com/tandemcommander/tandemcommander/releases/download/v0.1.5/tandemcommander-0.1.5-x64-setup.exe. Chtel bych mit moznost distribuovat aplikaci tak, aby si ji lide mohli stahnout na Windows pres winget install. Priprav vse potrebne tak, abychom pripravili moznost distribuce Inno setup instalatoru do Windows winget katalogu a abychom tam pak mohli nahravat samozrejme aktualizace az vydam zase novou verzi"

## Background *(today's behaviour)*

Tandem Commander has exactly one distribution channel: a signed Inno Setup
installer, built by `setup\build_setup.cmd sign`, uploaded by hand to a GitHub
release. A user must find the release page, download the file, dismiss
SmartScreen if the certificate is not yet widely seen, and run the wizard.
There is no update notification of any kind — the legacy `checkver` plugin
still points at upstream Open Salamander and no update feed exists.

The release procedure itself is manual and undocumented beyond `README.md`:
bump four files, `build.cmd full release sign setup`, annotated tag, upload.
No workflow touches tags or releases, and no workflow uses a secret.

## Clarifications

### Session 2026-08-29

- Q: Which permanent `PackageIdentifier` should the package carry? → A:
  `PavelStupka.TandemCommander` — it agrees with the installer's
  `AppPublisher` and with the code-signing certificate subject, which is what
  winget-pkgs validation compares the manifest `Publisher` against.
- Q: How should updates be submitted for each new release? → A: Both a local
  script and a GitHub workflow. The script must work without any token so it
  is usable before, and independently of, the automation.
- Q: Should per-user installation (`winget install --scope user`) be
  supported? → A: Yes. Planned as a change to `setup\tandemcommander.iss`;
  implementation established that no change is needed — the existing
  `PrivilegesRequiredOverridesAllowed=dialog` already enables the command-line
  scope switches, so every released version supports it.
- Q: Which `MinimumOSVersion` should the manifest declare, given that the
  project claims Windows 11 but the binaries target the Windows 7 API? → A:
  `10.0.19041.0` (Windows 10 2004) — what the product can actually run on,
  rather than what it markets.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Install Tandem Commander with one command (Priority: P1)

Someone setting up a Windows machine types `winget install tandemcommander`
and gets the current signed release installed silently, with no browser, no
download page and no SmartScreen dialog. `winget upgrade` later moves them to
the next version.

**Why this priority**: This is the entire request. It also gives the product
its first update path.

**Independent Test**: With the manifests generated locally,
`winget install --manifest tools\winget\manifests\0.1.5` installs silently,
`winget list PavelStupka.TandemCommander` reports version 0.1.5, and
`winget uninstall PavelStupka.TandemCommander` removes it.

**Acceptance Scenarios**:

1. **Given** the manifests for a released version, **When** they are installed
   through winget, **Then** the installation is completely silent — no wizard
   page, no disclaimer page, and the application does not start afterwards.
2. **Given** Tandem Commander installed through winget, **When** the user runs
   `winget list`, **Then** the package is recognised as installed at the
   published version (via the Inno Setup uninstall key `<AppId>_is1`).
3. **Given** a newer version in the catalogue, **When** the user runs
   `winget upgrade`, **Then** Tandem Commander is offered and upgrades in
   place over the existing installation.

### User Story 2 - Publish a new version in one command (Priority: P1)

After uploading the installer to a new GitHub release, the maintainer runs one
command (or does nothing at all and lets the workflow do it) and the catalogue
update is submitted as a pull request.

**Why this priority**: A distribution channel that is laborious to update
falls behind and then misleads users about the current version.

**Independent Test**: `publish.ps1 -Version <v>` regenerates the three
manifests for a released version, they pass `winget validate`, and the run
reports the correct SHA256 for the published asset.

**Acceptance Scenarios**:

1. **Given** a published release, **When** `publish.ps1` runs, **Then** the
   version, release date, release notes and installer SHA256 are derived
   automatically — from the installer script, `CHANGELOG.md` and the asset
   itself — with no value typed by hand.
2. **Given** an installer that is unsigned or signed by an unexpected
   certificate, **When** `publish.ps1` runs, **Then** it stops with an error
   and generates nothing.
3. **Given** no `WINGET_PAT` secret, **When** the workflow runs, **Then** it
   generates and validates the manifests and finishes successfully without
   submitting.

### User Story 3 - Install without administrator rights (Priority: P2)

A user on a locked-down machine runs `winget install tandemcommander --scope
user` and gets Tandem Commander in their own profile, with no UAC prompt.

**Why this priority**: Useful but secondary. It turned out to need no
installer change, so it applies to every released version.

**Independent Test**: `winget install --manifest <dir> --scope user`
completes without elevation and installs into
`%LOCALAPPDATA%\Programs\Tandem Commander`.

**Acceptance Scenarios**:

1. **Given** any released version, **When** the manifests are generated,
   **Then** they declare two installer entries (machine and user) over the
   same file and the same hash.
2. **Given** an installer script from which `PrivilegesRequiredOverridesAllowed`
   has been removed, **When** the manifests are generated, **Then** the
   generator refuses — the manifests must never advertise a scope the
   installer would reject.

## Requirements *(mandatory)*

- **FR-001**: The package MUST be published as `PavelStupka.TandemCommander`
  with the moniker `tandemcommander`.
- **FR-002**: Manifests MUST be generated from committed templates; the
  templates MUST be the only place catalogue metadata is edited.
- **FR-003**: The generator MUST derive the version from
  `setup\tandemcommander.iss`, and the release date and release notes from
  `CHANGELOG.md`, unless overridden by a parameter.
- **FR-004**: The generator MUST hash the *published* release asset, and MUST
  verify its Authenticode signature against `tools\codesign\codesign.cfg`
  before generating anything.
- **FR-005**: The generator MUST validate the result with `winget validate`
  when winget is available, and MUST NOT fail merely because it is absent.
- **FR-006**: The generator MUST be able to submit the pull request to
  microsoft/winget-pkgs, and MUST work fully without a token when not
  submitting.
- **FR-007**: A GitHub workflow MUST run the same generator when a release is
  published, MUST skip pre-releases, and MUST finish successfully when the
  token secret is absent.
- **FR-008**: The installer MUST accept `/ALLUSERS` and `/CURRENTUSER` so
  winget can select the scope in a silent install. Satisfied by the existing
  `PrivilegesRequiredOverridesAllowed` directive; no installer change.
- **FR-009**: The generator MUST refuse to run if that directive is absent
  from `setup/tandemcommander.iss`, so the manifests cannot advertise an
  install mode the installer would reject.
- **FR-010**: Generated manifests MUST be free of authoring comments and MUST
  be committed as a record of what was submitted.
- **FR-011**: The token MUST NOT be passed on a command line by the workflow.
- **FR-012**: Documentation MUST cover the one-time fork/token setup, the
  per-release procedure, local install testing in both scopes, and
  troubleshooting.

### Non-goals

- No change to the build, to the product, or to the release procedure beyond
  the one installer directive.
- No automation of the GitHub release itself (tagging and asset upload stay
  manual).
- No `checkver` in-application update feed — a separate concern.
- No additional locale manifests beyond `en-US`.

## Success Criteria *(mandatory)*

- **SC-001**: `winget validate` passes on the generated manifests for 0.1.5.
- **SC-002**: `winget install --manifest` installs 0.1.5 silently, is listed
  by `winget list`, and uninstalls cleanly.
- **SC-003**: Generated manifests declare both install scopes, and the
  generator refuses to run if the installer directive they depend on is gone.
- **SC-004**: The generator refuses an installer whose signature does not
  match the committed thumbprint.
- **SC-005**: Publishing a subsequent version requires exactly one command and
  no hand-entered values.
