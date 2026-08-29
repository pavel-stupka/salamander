# Implementation Plan: winget Distribution

**Branch**: `072-winget-distribution` | **Date**: 2026-08-29 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/072-winget-distribution/spec.md`

## Summary

Tandem Commander is published to the Windows Package Manager catalogue as
`PavelStupka.TandemCommander`. The catalogue stores no binary — it stores three
YAML manifests that point at the installer already on GitHub Releases and pin
its SHA256 — so publishing a version means opening a pull request against
`microsoft/winget-pkgs`.

The whole feature is distribution tooling. No product file changes
behaviour; the installer script gains only a comment.

- **Templates are the source of truth** (`tools/winget/templates/*.yaml.in`).
  All catalogue metadata — description, tags, URLs, licence, ProductCode,
  minimum OS — lives there, with the authoring rationale in comments that the
  generator strips, so submitted manifests stay clean and conventional.
- **One entry point** `tools/winget/publish.ps1` (Windows PowerShell 5.1, the
  `tools/codesign/sign_release.ps1` tier and style): derive version and release
  date, flatten the changelog lead paragraph into release notes, download the
  published asset, **verify its Authenticode signature against the committed
  thumbprint**, hash it, render, `winget validate`, and — only with `-Submit` —
  hand the directory to `wingetcreate submit`.
- **A workflow calls the same script** on `release: published`, skipping
  pre-releases, and degrades to generate-and-validate when `WINGET_PAT` is
  absent, following the repository's existing "not applicable → inform and
  succeed" guard convention.
- **One machine-scope installer entry, no switches.** Per-user installation
  was attempted and withdrawn: Inno Setup accepts the scope switches, but the
  catalogue's Installation Validation rejected the entry. See D6.

## Technical Context

**Language/Version**: Windows PowerShell 5.1 (stock Windows, no install), YAML
manifests against winget schema 1.10.0
**Primary Dependencies**: `winget` (validation, optional), `wingetcreate`
(submission, downloaded on demand from `https://aka.ms/wingetcreate/latest`);
the committed signing profile `tools/codesign/codesign.cfg`
**Storage**: none — generated manifests are committed under
`tools/winget/manifests/<version>/` as a record
**Testing**: `winget validate` inside the generator; `winget install
--manifest` in both scopes and `winget-pkgs\Tools\SandboxTest.ps1` per
[quickstart.md](quickstart.md); YAML well-formedness
**Target Platform**: the catalogue targets x64 Windows 10 2004 and newer
**Project Type**: release tooling (`tools/`) plus one GitHub workflow
**Constraints**: no product, build or plugin change; the identifier is
permanent; the manifests must never advertise an install mode the installer
would reject; the token must never reach a command line
**Scale/Scope**: 1 script, 3 templates, 1 workflow, 3 documentation surfaces

## Constitution Check

| Principle | Gate | Status |
|-----------|------|--------|
| I. Build Reproducibility | Nothing in the build changes. The generator derives every value from committed sources (`tandemcommander.iss`, `CHANGELOG.md`, `codesign.cfg`) and from the published asset; the same version always produces the same manifests, and they are committed. | PASS |
| II. Backward Compatibility | No functional change to the installer at all — only a comment was added. Nothing about how Tandem Commander installs or behaves changes; winget merely becomes a second way to obtain the same installer. | PASS |
| III. Incremental Modernization | All new code is in new files under `tools/winget/`; the only edit to an existing product file is a comment. | PASS |
| IV. Windows Platform Commitment | Windows-only tooling: Windows PowerShell 5.1, winget, Inno Setup, Authenticode. | PASS |
| V. Plugin Architecture Preservation | No `src/` change at all; plugin interface untouched. | PASS |
| VI. UI Consistency | No user interface. The installer wizard is unchanged. | PASS |
| Release Documentation | Nothing shipped in the product changes, so the tooling itself needs no entry. Availability through winget is user-visible and is recorded in the `CHANGELOG.md` entry for **0.1.6** (build 190, 2026-08-29), the release that carries the first submission. | PASS |

## Project Structure

### Documentation (this feature)

```text
specs/072-winget-distribution/
├── spec.md                         # Feature specification (clarified 2026-08-29)
├── plan.md                         # This file
├── tasks.md                        # Task breakdown
├── quickstart.md                   # Validation guide
├── contracts/
│   └── winget-manifest.md          # What the manifests must state, and why
└── fix-log.md                      # Running record
```

### Source (repository root)

```text
tools/winget/
├── README.md                       # NEW: one-time setup, per-release procedure, testing, troubleshooting
├── publish.ps1                     # NEW: generate -> verify -> validate -> submit
├── templates/
│   ├── version.yaml.in             # NEW
│   ├── installer.yaml.in           # NEW: scopes, ProductCode, minimum OS
│   └── locale.en-US.yaml.in        # NEW: all catalogue metadata
└── manifests/0.1.5/                # NEW: generated, committed record

.github/workflows/winget-publish.yml  # NEW: release: published -> the same script
setup/tandemcommander.iss             # EDIT: comment only - why the directive must not be narrowed
README.md                             # EDIT: "Publishing to winget" subsection
CLAUDE.md                             # EDIT: Recent Changes entry
```

## Key Decisions

**D1 — Identifier `PavelStupka.TandemCommander`.** winget documents the form
`Publisher.Package`, and winget-pkgs validation compares the manifest's
`Publisher` with the installer's `AppPublisher` and the signing certificate;
both say "Pavel Stupka". `TandemCommander.TandemCommander` would have matched
the GitHub organisation and the domain instead, and is equally acceptable in
practice — the choice was the user's. `Moniker: tandemcommander` provides the
short install command either way. **Permanent**: a later change would create a
separate package and strand installed users.

**D2 — Templates + generator rather than `wingetcreate update`.**
`wingetcreate update` bumps the version and hash on whatever is already in the
catalogue, which makes the catalogue the source of truth and lets local
metadata drift. Keeping the templates authoritative means a metadata change is
a reviewable diff in this repository, and the submitted manifests are
reproducible from it.

**D3 — Comments stripped from generated manifests.** The templates carry long
explanations (why the ProductCode is what it is, why both scopes work).
Submitted winget-pkgs manifests conventionally carry only the schema
line, and unusual commentary invites reviewer questions. The generator keeps
the schema line, adds a one-line provenance header, and drops the rest — so
documentation is unlimited where it is maintained. Consequence recorded in the
contract: no line inside a YAML block scalar may start with `#`.

**D4 — Authenticode verification before generating.** The generator hashes the
file users will actually receive; verifying it against the committed thumbprint
turns "the right file was uploaded" from an assumption into a check, at the one
moment the file is already in hand. It reuses `tools/codesign/codesign.cfg`, so
certificate rotation needs no second edit.

**D5 — `MinimumOSVersion: 10.0.19041.0`.** The project markets Windows 11, but
the binaries are built against `_WIN32_WINNT=0x0601` and the installer sets no
floor. winget *refuses to install* below `MinimumOSVersion`, so the value
describes capability rather than marketing. Windows 10 users are told in
`InstallationNotes` that the Markdown viewer needs the WebView2 runtime (which
Windows 11 includes) — a note rather than a hard `Dependencies` entry, which
would force the runtime on everyone.

**D6 — Per-user installation: attempted, refuted twice, withdrawn.** This
went wrong in two stages and the record is worth keeping whole.

*First premise, wrong:* that `/ALLUSERS` and `/CURRENTUSER` need
`PrivilegesRequiredOverridesAllowed=commandline`, so older releases had to be
gated out with a version boundary. Testing refuted it — Inno Setup enables the
switches for the `dialog` mode too, which `setup/tandemcommander.iss` has
always had, and a probe with the installer's exact privilege configuration
installed per-user silently with no elevation. The installer change and the
boundary were both removed.

*Second premise, also wrong:* that the per-user entry was what check 08
rejected. It was removed on that theory — the pipeline runs manifests elevated
(winget-pkgs issue 72224), so `/CURRENTUSER` would land in the administrator's
profile — and the machine-only manifest **failed identically**. The theory was
plausible, matched the label and matched T017, and was still wrong.

*The actual cause* was not in the manifest at all: the installer could not be
installed silently. See D7. Whether a per-user entry works remains untested,
because the real defect masked it.

**The process error, not the technical one:** the failure was never
reproduced. Running the installer with winget's own switches took under a
minute and gave the answer at once; it should have been the first step after
round 1, not the third. Two rounds of validation were spent on theories
instead. Separately, T017 was recorded as an *unverified assumption* and the
manifest submitted with it anyway, in the one pull request that also had to get
a brand-new package accepted.

The `PrivilegesRequiredOverridesAllowed` assertion added to `publish.ps1`
alongside the boundary went with the entry: the manifests no longer depend on
that directive, so the check guarded nothing.

**D7 — The installer could not be installed silently, and that was the real
defect.** Tandem Commander's AI disclaimer page (feature 050) keeps the *Next*
button disabled until its checkbox is ticked. A silent install still traverses
the wizard's pages, so Setup met a button it could not press and aborted with
exit code 1 and `Failed to proceed to next wizard page`. **Every release from
v0.1.0 to v0.1.6** was affected: unattended installation had never worked, and
nobody noticed because the installer had only ever been run by hand.

Proven with a pair of probe installers differing only in the guard — without
it exit 1 and the same log line, with it exit 0 and `Installation process
succeeded.` Fixed in `setup/tandemcommander.iss` by guarding `CurPageChanged`
with `not WizardSilent`, shipped in **0.1.7**; interactive behaviour is
unchanged.

0.1.6 could not be salvaged, because the manifest pins the SHA256 of the
published asset and replacing a released binary is not acceptable. PR #426038
was closed and the submission remade for 0.1.7.

The lasting consequence is a new mandatory step, quickstart §2b: verify
`/VERYSILENT` on the built installer before every submission. It has to be run
from an elevated shell — from a normal one the UAC prompt cannot be answered
and the result is exit code 2, which looks like a defect and is not one.

## Complexity Tracking

None. No new project, no new dependency shipped with the product, no
configuration, no plugin-interface change.
