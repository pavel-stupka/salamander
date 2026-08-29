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
  `AppsAndFeaturesEntries.ProductCode` = `{AppId}_is1`, and two installer
  entries over the same file - machine (`/ALLUSERS`, `elevatesSelf`) and user
  (`/CURRENTUSER`, no `ElevationRequirement`)
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
- [x] T007 Run quickstart §1 for 0.1.5; fix the two schema errors it exposed
  (`InstallerSuccessCodes: [0]` is rejected; the locale field is
  `Documentations`, not `Documentation`)
- [x] T008 Commit the generated `tools/winget/manifests/0.1.5/` as the record
  of what will be submitted (FR-010)
- [x] T009 Run quickstart §2 (signature gate) and confirm both rejection paths
- [ ] T010 [MANUAL] Run quickstart §4: real silent install from the manifests,
  `winget list`, uninstall. Needs an administrator shell.

## Phase 4: US2 - Publish in one command (P1)

- [x] T011 Write `.github/workflows/winget-publish.yml`: `release: published`
  + `workflow_dispatch`, skip pre-releases, resolve version from the tag,
  degrade to generate-and-validate when `WINGET_PAT` is absent, pass the token
  only through the environment (FR-011), upload the manifests as an artifact
- [x] T012 Write `tools/winget/README.md`: one-time fork/token setup,
  per-release procedure, local testing in both scopes, how the scopes work,
  troubleshooting
- [x] T013 [P] Add the *Publishing to winget* subsection to `README.md` under
  *Release, Code Signing & Installer*
- [ ] T014 [MANUAL] Run quickstart §5: `workflow_dispatch` with `submit`
  unchecked on this branch; confirm it finishes green and attaches the artifact

## Phase 5: US3 - Per-user installation (P2)

- [x] T015 **Premise refuted, plan corrected.** The planned
  `PrivilegesRequiredOverridesAllowed=commandline dialog` edit was made, then
  reverted: a probe built with the installer's exact privilege configuration
  showed `dialog` alone already accepts `/CURRENTUSER` in a silent install.
  `setup/tandemcommander.iss` keeps `=dialog` and gains only a comment saying
  the directive must not be narrowed (quickstart §3)
- [x] T016 Replace the version gating with an invariant check in
  `publish.ps1`: refuse to generate if `PrivilegesRequiredOverridesAllowed` is
  missing from the installer script; prove it fires by removing the directive
- [ ] T017 [MANUAL] Run quickstart §4b: install `--scope user` without
  elevation and `--scope machine` with it; confirm winget does not displace
  `InstallerSwitches.Custom`

## Phase 6: Ship gate

- [x] T018 Add the *Recent Changes* entry to `CLAUDE.md`
- [x] T019 Record deviations, open items and verification results in
  `fix-log.md`
- [x] T020 CHANGELOG entry announcing winget availability, written with the
  0.1.6 version bump (build 190, released 2026-08-29) as the constitution
  requires; phrased so it stays true before Microsoft accepts the submission
- [ ] T021 [MANUAL] Submit the first pull request (quickstart §6). Irreversible
  and public; explicitly held for the user's decision
- [x] T022 User-facing *Installing* section in `README.md` (installer
  download + the two winget commands, with availability stated as pending
  acceptance). tandemcommander.org still to follow - outside this repository
