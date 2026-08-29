# Fix log: 072-winget-distribution

Running record. A ticked task in [tasks.md](tasks.md) means the entry here
says *verified*.

## Status by task

| Task | State | Note |
|---|---|---|
| T001 Branch + log | done | branched from `main` |
| T002 Decisions | done | plan.md D1-D6 |
| T003 version template | done | |
| T004 installer template | done | two scopes, unconditional |
| T005 locale template | done | 16 tags, WebView2 note |
| T006 publish.ps1 | done | 5.1, ASCII, ~350 lines |
| T007 Generate + validate 0.1.5 | **verified** | `Manifest validation succeeded.` |
| T008 Commit manifests | done | `tools/winget/manifests/0.1.5/` |
| T009 Signature gate | **verified** | both rejection paths |
| T010 Real install | **open (manual)** | needs an administrator shell |
| T011 Workflow | done | YAML parsed; not yet run |
| T012 tools/winget/README.md | done | |
| T013 README.md subsection | done | |
| T014 Workflow dry run | **open (manual)** | needs a push + Actions run |
| T015 .iss directive | **premise refuted** | change made, then reverted - see below |
| T016 Directive guard | **verified** | fires when the directive is removed |
| T017 Both scopes installed | **open (manual)** | no special build needed any more |
| T018 CLAUDE.md | done | |
| T019 This log | done | |
| T020 CHANGELOG | done | written with the 0.1.6 bump |
| T021 First submission | **open (user's call)** | irreversible, public |
| T022 User-facing README | done | website still to follow |

## The one real correction: the installer needed no change at all

The plan's D6 was built on a premise that turned out to be false.

**Assumed**: Inno Setup accepts `/ALLUSERS` and `/CURRENTUSER` only when the
installer is compiled with `PrivilegesRequiredOverridesAllowed=commandline`.
`setup/tandemcommander.iss` had `=dialog`, so the plan added `commandline` and
introduced `$UserScopeSinceVersion = '0.1.6'` to keep already-released versions
from advertising a scope they could not honour.

**Found**: Inno Setup documents permitting the install-mode dialog as *also*
enabling the command-line parameters. `=dialog` alone already accepts both
switches, and it has been in the installer script all along.

**Evidence** — probe installers compiled with Inno Setup 7 and run silently:

| Probe configuration | Command | Result |
|---|---|---|
| `PrivilegesRequired=lowest`, `=dialog` | `/VERYSILENT /CURRENTUSER` | exit 0, installed per-user |
| `PrivilegesRequired=lowest`, `=dialog` | `/VERYSILENT /ALLUSERS` | switch **honoured** - tried to elevate, blocked on the UAC prompt (2 min timeout), nothing installed. Not ignored, not rejected. |
| **`PrivilegesRequired` unset (admin), `=dialog`, `{autopf}` — the exact configuration of `tandemcommander.iss`** | `/VERYSILENT /CURRENTUSER` | exit 0, installed into `%LOCALAPPDATA%\Programs\<AppName>`, **no elevation**, `HKCU` uninstall key `{AppId}_is1` written |

All probes were uninstalled through their own uninstallers; no directory,
registry key or Start Menu entry was left behind, and the user's own
machine-wide Tandem Commander 0.1.5 installation was never touched.

**Consequences**:

- `setup/tandemcommander.iss` is back to `PrivilegesRequiredOverridesAllowed=dialog`.
  It gains only a comment recording that this directive is what winget relies
  on, so nobody narrows it.
- The version gating is gone: `$UserScopeSinceVersion`, the
  `#@REQUIRES_CMDLINE_SCOPE_*` fences and the block-stripping pass in
  `Render-Template` were all removed. `publish.ps1` lost about 20 lines.
- **0.1.5 offers both scopes**, so per-user installation is available from the
  first submission rather than the next release.
- What replaces the gating is a check on the thing that can actually go wrong:
  `publish.ps1` refuses to generate if `PrivilegesRequiredOverridesAllowed` is
  missing from the installer script. Proven by commenting the directive out —
  the run fails with the intended message, and nothing is written.

Recorded because the mistake is instructive: the boundary would have shipped
correct manifests for the wrong reason, and would have delayed a working
feature by a release.

## Verification results

**Generation for 0.1.5** (quickstart §1) — passed.

```
 Version       : 0.1.5
 Release date  : 2026-08-25          (from CHANGELOG.md, not typed)
 Installer     : GitHub release (6 844 176 bytes)
 SHA256        : 0B41E35A0C86AD5CB7C10F6D5C99BEBBE32B907F47A2A1BFFD4884AC8D1C36D5
 Scopes        : machine + user
Manifest validation succeeded.
```

The downloaded asset and the local `setup\output\` copy hash identically, so
the published file is the signed build in the tree.

**Signature gate** (quickstart §2) — passed, both paths:

- `C:\Windows\explorer.exe` (5 MB, Microsoft-signed) →
  `ERROR: the installer is signed by an unexpected certificate (CN=Microsoft Windows, ...)`
- a 455 KB executable → `ERROR: the installer is only 454656 bytes - wrong file?`

In both cases `manifests\0.1.5\` was left untouched: the check runs before
anything is written.

**Installer-directive guard** (quickstart §3) — passed. With the directive
commented out, the run fails with
`ERROR: setup\tandemcommander.iss has no PrivilegesRequiredOverridesAllowed ...`
and exits 1. The file was restored and `git diff` shows only the added comment.

**Inno Setup accepts the file** — a probe compiled cleanly with Inno Setup 7,
confirming the added comment breaks nothing. The real installer was not
recompiled, deliberately: that would overwrite
`setup\output\tandemcommander-0.1.5-x64-setup.exe`, which currently matches the
published asset byte for byte and is what the manifests are hashed against.

**YAML** — the workflow and all three generated manifests parse.

## Decisions taken while implementing

**Two schema errors, found by `winget validate` on the first run** (T007):

1. `InstallerSuccessCodes: [0]` — the schema explicitly forbids `0`; success is
   implicit. The key was dropped entirely.
2. The locale field for documentation links is `Documentations`, not
   `Documentation`. Recorded in the contract §6 so it is not re-introduced.

**No `ElevationRequirement` on the user entry** — the plan proposed
`elevationNotRequired`, which is not a value the schema defines (the enum is
`elevationRequired` / `elevationProhibited` / `elevatesSelf`). Omitting the
field is also the correct behaviour: `elevationProhibited` would make winget
refuse a per-user install requested from an elevated console, which is legal,
just profile-bound.

**Comments are stripped from generated manifests.** Submitted winget-pkgs
manifests conventionally carry only the schema line. Keeping the rationale in
the templates and stripping it on generation lets the templates be as
explanatory as they need to be. The trade-off, recorded in contract §6: no line
inside a YAML block scalar may start with `#`.

**Release notes come from the changelog.** The lead paragraph of the version's
`CHANGELOG.md` section is flattened to one paragraph (Markdown emphasis, links
and inline code removed; typographic dashes and quotes folded to ASCII; capped
at 900 characters) and becomes `ReleaseNotes`, so `winget show` says what
actually changed instead of pointing at a URL.

## Deviations from the plan

- **D6 was refuted and rewritten** — see the section above.
- The plan's file layout put a `package.cfg` next to the templates, mirroring
  `tools/codesign/codesign.cfg`. Dropped: almost every value here is stable and
  belongs with the field it fills, so the templates are the single source of
  truth and there is one file fewer to keep in step.
- `-LocalFile` proved more useful than planned: it is how the signature gate
  was tested without a matching release existing.

## Open items

- **T010 / T014 / T017 are manual** and cannot be run from here: they install
  software, need an administrator shell, or need a pushed branch. Commands are
  in [quickstart.md](quickstart.md).
- **T017 also validates an assumption** that testing could not settle here:
  that winget adds no scope switch of its own for `inno` installers that would
  displace `InstallerSwitches.Custom`. If `--scope user` were to install
  per-machine, the switches must move to the `Silent` / `SilentWithProgress`
  fields.
- **T021** the first submission is irreversible and public — held for the
  user. It will be made for **0.1.6**, not 0.1.5: that is the release the
  CHANGELOG and the README announce winget with.
- **tandemcommander.org** still needs the same *Installing* wording as
  `README.md`; that lives outside this repository.
- Both the CHANGELOG entry and the README section state availability as
  following Microsoft's acceptance, so neither claims something untrue in the
  days between the release and the merge.
- Not in scope, but now cheap: `checkver` could be redirected at the GitHub
  Releases API, or dropped in favour of `winget upgrade`.
