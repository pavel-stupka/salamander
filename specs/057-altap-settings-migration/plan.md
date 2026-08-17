# Implementation Plan: Altap Salamander Settings Migration Utility

**Branch**: `057-altap-settings-migration` | **Date**: 2026-08-17 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `specs/057-altap-settings-migration/spec.md`

## Summary

A standalone, repo-only, single-file console utility (`utils/migrate-altap-settings.cmd`)
that selectively copies per-user settings from any Altap/Servant Salamander
registry configuration (2.5x–4.0 era, `HKCU\Software\Altap\...`) into Tandem
Commander's registry root (`HKCU\Software\Tandem Commander\0.1`). It runs as an
interactive console wizard (detect sources → pick source → category checklist
with item counts → confirm → transfer → summary), backs up the destination to a
`.reg` file with a generated one-click restore script before writing, treats
the source strictly read-only, replaces selected categories wholesale, and
reports every skipped item with a reason. The Tandem Commander application,
build, and installer are not touched (FR-014); per the constitution the
application itself never reads predecessor configuration — this utility is a
separate, user-initiated tool.

Technically: a cmd/PowerShell polyglot — a `.cmd` batch header that relaunches
the file's embedded **Windows PowerShell 5.1** payload with
`-ExecutionPolicy Bypass`, so the one file is double-click runnable on stock
Windows 11 with no prerequisites. Registry work uses the .NET
`Microsoft.Win32.Registry` API for type-exact subtree copies; the per-category
compatibility rules (verbatim copy vs. transform vs. skip) come from the
Phase 0 analysis of the product's own config-version upgrade chain
(see [research.md](research.md) and
[contracts/category-mapping.md](contracts/category-mapping.md)).

## Technical Context

**Language/Version**: Windows batch (launcher, ~10 lines) + Windows PowerShell 5.1 (payload) in ONE polyglot `.cmd` file; PS 5.1 is preinstalled on every Windows 11 system
**Primary Dependencies**: none beyond stock Windows 11 — .NET Framework `Microsoft.Win32.Registry` classes (in-box), `reg.exe` (in-box, for `.reg` backup export)
**Storage**: Windows registry only — reads `HKCU\Software\Altap\*` (+ Servant-era roots, read-only), writes `HKCU\Software\Tandem Commander\0.1`; backup artifacts (`.reg` + generated `restore.cmd` + summary `.txt`) written next to the script (fallback: `%USERPROFILE%\Documents`)
**Testing**: scripted harness `utils/test/run_migration_tests.cmd` — imports synthetic Altap-era fixture hives (`.reg` files) under a scratch registry key, drives the wizard via redirected stdin, verifies destination state with PowerShell assertions; supported by test-only environment-variable overrides of the source-scan and destination roots
**Target Platform**: Windows 11+ (same baseline as the product); works on Windows 10 incidentally but that is not a support claim
**Project Type**: standalone single-file console utility (repository `utils/` directory; NOT part of the product, build, or installer)
**Performance Goals**: complete transfer of a typical configuration (~1–2 thousand registry values) in under 10 seconds; wizard fully usable per SC-001 (whole migration under 5 minutes)
**Constraints**: single self-contained file; pure ASCII content (cmd polyglot header forbids a UTF-8 BOM — deliberate, documented deviation from the repo's UTF-8-BOM rule for C++ sources); English-only UI (FR-013); no writes outside the TC root + backup/summary files; source opened with read-only access rights
**Scale/Scope**: ~10 setting categories + per-plugin configs for the 18 shipped plugins; 4 source root generations (Servant 2.5x era → Altap Salamander 4.0); one script estimated ≤ ~1,200 lines including wizard + copy engine + transforms

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle | Verdict | Notes |
|---|-----------|---------|-------|
| I | Build Reproducibility | ✅ PASS | Build pipeline untouched; the utility is not built, compiled, or copied by `build.cmd`; no new build steps. |
| II | Backward Compatibility | ✅ PASS | The **application** still never reads/writes predecessor configuration — zero application code changes (FR-014). The utility is a separate user-initiated repo tool; it writes only TC's own root in TC's current formats, so TC's config never "moves" (Release Documentation rule: MINORB must not require migration — no version bump ships with this feature at all). Identity separation is preserved: nothing is reintroduced into the product. |
| III | Incremental Modernization | ✅ PASS (n/a) | No legacy C++ touched; one new standalone script. |
| IV | Windows Platform Commitment | ✅ PASS | Utility targets Windows 11+, uses only in-box Windows tooling. The "C++/MSVC" constraint governs the application; repo tooling in other languages is established precedent (`tools/codesign/sign_release.ps1`, `src/vcxproj/lang_policy.ps1`, Python in `tools/`). Own code, GPLv2-compatible. |
| V | Plugin Architecture Preservation | ✅ PASS (n/a) | Plugin system untouched; plugin configs are migrated as data only. |
| VI | UI Consistency | ✅ PASS (n/a) | No application dialogs added; the utility is an external console program, deliberately not presenting itself as part of the product UI. |
| — | Release Documentation | ✅ PASS | The utility changes what a user can rely on → one `CHANGELOG.md` entry under the next release's "Added". No version/build bump: release artifacts are unchanged (repo-only distribution per Clarifications 2026-08-17). |

**Post-design re-check (after Phase 1)**: no design artifact introduced an
application, build, or installer change; the category mapping writes only
value formats the current TC loader accepts, keeping principle II intact.
Gate remains ✅ PASS on all rows. No Complexity Tracking entries needed.

## Project Structure

### Documentation (this feature)

```text
specs/057-altap-settings-migration/
├── plan.md              # This file
├── research.md          # Phase 0: technology + per-category compatibility research
├── data-model.md        # Phase 1: entities (source, category, backup, summary)
├── quickstart.md        # Phase 1: validation scenarios
├── contracts/
│   ├── category-mapping.md   # Phase 1: category → registry subtree mapping + transform rules
│   └── wizard-flow.md        # Phase 1: console wizard screens, inputs, exit codes
└── tasks.md             # Phase 2 (/speckit-tasks — not created by /speckit-plan)
```

### Source Code (repository root)

```text
utils/                                  # NEW top-level directory (Clarifications 2026-08-17)
├── migrate-altap-settings.cmd         # THE deliverable: single-file cmd+PS5.1 polyglot wizard
├── README.md                          # what it does, how to download the one file, restore procedure
└── test/                              # repo-only test harness (users never need this)
    ├── run_migration_tests.cmd        # imports fixtures under a scratch key, drives wizard, asserts
    └── fixtures/
        ├── altap40-full.reg           # synthetic AS 4.0 config (all categories populated)
        ├── altap25-minimal.reg        # ancient-era source (missing categories)
        └── tc-preexisting.reg         # destination with user-created content (replace semantics test)
```

**Structure Decision**: single new top-level `utils/` directory as fixed in
the spec clarifications — the deliverable is exactly one downloadable file;
`README.md` and `test/` live beside it in the repository only. No `src/`,
solution, or installer changes.

## Phase 0 → research.md

Unknowns resolved by researching this repository itself (the product is the
authority on its own registry formats):

1. Config-version machinery: current config version number, the
   version→product mapping, all version-gated conversion sites → which
   categories changed shape between AS 2.5x/3.x/4.0 and TC now.
2. Historical Altap/Servant registry roots (pre-feature-032
   `SalamanderConfigurationRoots[]` from git history) → the utility's source
   scan list.
3. Plugin config storage layout + plugin-key naming → how to match "FTP config
   in source" to "FTP config in destination"; which plugin values are
   installation metadata (never copied).
4. FTP bookmark + password storage and the password manager's encryption →
   what FR-010 means concretely (scrambled vs. AES-with-master-password).
5. Per-category registry formats and post-fork format drift (feature 047 hot
   path icons, features 048/049 dark mode colors, …) → verbatim / transform /
   skip verdict per category.
6. Script-technology decisions (cmd+PS5.1 polyglot, .NET registry API, reg.exe
   backup, stdin-safe prompting, test-only root overrides).

## Phase 1 → design artifacts

- **data-model.md**: SourceConfiguration, SettingCategory (with compatibility
  status + transform), MigrationPlan (user selection), MigrationBackup,
  MigrationSummary — states and validation rules from FR-002..FR-011.
- **contracts/category-mapping.md**: the authoritative category table — source
  subtree(s) → destination subtree, item-count rule, transform/filter rules,
  skip conditions per source generation. This is the FR-004 "definitive
  per-category list fixed during planning".
- **contracts/wizard-flow.md**: every wizard screen, its inputs and valid
  answers, refusal conditions (running processes, no source found), exit
  codes, and the summary format — the testable UI contract for FR-003,
  FR-008, FR-011, FR-012.
- **quickstart.md**: runnable validation scenarios mapping to the spec's four
  user stories + edge cases (fixture import → wizard run → assertions →
  restore check).
