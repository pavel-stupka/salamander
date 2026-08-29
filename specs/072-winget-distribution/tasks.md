# Tasks: winget Distribution

**Input**: Design documents from `/specs/072-winget-distribution/`
**Prerequisites**: spec.md (3 user stories), plan.md (D1-D6), contracts/winget-manifest.md, quickstart.md

**Tests**: There is no unit-test surface here — the artefact is a manifest, and
the only meaningful checks are `winget validate` (run by the generator itself
on every invocation) and a real install. Both are explicit tasks below, so a
ticked box means "checked", not "written".

**Organization**: Phases 3-5 map to spec user stories US1-US3.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files, no dependency on an incomplete task)
- Paths are repository-relative

---

## Phase 1: Setup

- [x] T001 Create the branch `072-winget-distribution` from `main` and the
  running log `specs/072-winget-distribution/fix-log.md`
- [x] T002 Record the locked decisions (identifier, submission method, scope,
  minimum OS) in `plan.md` D1-D6 with the reasoning for each

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: the templates every later step renders

- [x] T003 [P] Write `tools/winget/templates/version.yaml.in` (schema 1.10.0,
  `PackageIdentifier`, `{{VERSION}}`, `DefaultLocale: en-US`)
- [x] T004 [P] Write `tools/winget/templates/installer.yaml.in` per
  contracts/winget-manifest.md §2-§4: `InstallerType: inno`,
  `UpgradeBehavior: install`, `MinimumOSVersion: 10.0.19041.0`,
  `AppsAndFeaturesEntries.ProductCode` = `{AppId}_is1`, and one machine-scope
  installer entry with no switches (the two-entry machine+user version was
  withdrawn after it failed validation - see T017)
- [x] T005 [P] Write `tools/winget/templates/locale.en-US.yaml.in` with every
  catalogue metadata field: publisher/author/URLs, `License: GPL-2.0-or-later`,
  copyright per the CLAUDE.md holder rule, short + long description, 16 tags,
  `Moniker`, `{{RELEASE_SUMMARY}}`, `ReleaseNotesUrl`, `Documentations`,
  `InstallationNotes` (WebView2 on Windows 10)

## Phase 3: US1 - Install with one command (P1)

- [x] T006 Write `tools/winget/publish.ps1` (Windows PowerShell 5.1, ASCII,
  `tools/codesign/sign_release.ps1` style): parameters, version from
  `setup/tandemcommander.iss`, release date + lead paragraph from
  `CHANGELOG.md`, the `PrivilegesRequiredOverridesAllowed` invariant check,
  placeholder substitution, comment stripping, download, Authenticode verification against
  `tools/codesign/codesign.cfg`, SHA256, write, `winget validate`,
  `wingetcreate submit`
- [x] T007 Run quickstart §1 (first for 0.1.5); fix the two schema errors it exposed
  (`InstallerSuccessCodes: [0]` is rejected; the locale field is
  `Documentations`, not `Documentation`)
- [x] T008 Commit the generated manifests as the record of what was
  submitted (FR-010). `manifests/0.1.6/`; the 0.1.5 directory was generated
  during development, never submitted, and has been removed
- [x] T009 Run quickstart §2 (signature gate) and confirm both rejection paths
- [ ] T010 [MANUAL] Run quickstart §4: real silent install from the manifests,
  `winget list`, uninstall. Needs an administrator shell.

## Phase 4: US2 - Publish in one command (P1)

- [x] T011 Write `.github/workflows/winget-publish.yml`: `release: published`
  + `workflow_dispatch`, skip pre-releases, resolve version from the tag,
  degrade to generate-and-validate when `WINGET_PAT` is absent, pass the token
  only through the environment (FR-011), upload the manifests as an artifact
- [x] T012 Write `tools/winget/README.md`: one-time fork/token setup,
  per-release procedure, local testing, how the install scope works,
  troubleshooting
- [x] T013 [P] Add the *Publishing to winget* subsection to `README.md` under
  *Release, Code Signing & Installer*
- [x] T014 Workflow verified on `main` (2026-08-29), and twice over: run #1
  fired by itself on the v0.1.6 release publication with no `WINGET_PAT` yet
  and correctly degraded to generate-and-validate; run #2 was the deliberate
  `workflow_dispatch` dry run. Both green

## Phase 5: US3 - Per-user installation (P2)

- [x] T015 **Premise refuted, plan corrected.** The planned
  `PrivilegesRequiredOverridesAllowed=commandline dialog` edit was made, then
  reverted: a probe built with the installer's exact privilege configuration
  showed `dialog` alone already accepts `/CURRENTUSER` in a silent install.
  `setup/tandemcommander.iss` keeps `=dialog` and gains only a comment saying
  the directive must not be narrowed (quickstart §3)
- [x] T016 Version gating replaced with an invariant check in `publish.ps1`,
  proven to fire; **later removed again** with the per-user entry, since the
  manifests stopped depending on the directive it guarded
- [x] T017 **Assumption refuted in production.** The per-user entry failed
  check 08 Installation Validation on microsoft/winget-pkgs#426038
  (`Validation-Shell-Execute`) and was removed; the package ships
  machine-only. Reinstating it needs `SandboxTest.ps1` and a separate pull
  request. See plan.md D6

## Phase 6: Ship gate

- [x] T018 Add the *Recent Changes* entry to `CLAUDE.md`
- [x] T019 Record deviations, open items and verification results in
  `fix-log.md`
- [x] T020 CHANGELOG entry announcing winget availability, written with the
  0.1.6 version bump (build 190, released 2026-08-29) as the constitution
  requires; phrased so it stays true before Microsoft accepts the submission
- [x] T021 First pull request submitted for **0.1.6**:
  microsoft/winget-pkgs#426038 (2026-08-29). CLA signed, checks 01-06 and
  license/cla green, 05 neutral as expected for a new package, 07-10 running;
  awaiting the human review the `New-Package` label calls for
- [x] T022 User-facing *Installing* section in `README.md` (installer
  download + the two winget commands, with availability stated as pending
  acceptance). tandemcommander.org still to follow - outside this repository
