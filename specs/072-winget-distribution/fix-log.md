# Fix log: 072-winget-distribution

Running record. A ticked task in [tasks.md](tasks.md) means the entry here
says *verified*.

## Status by task

| Task | State | Note |
|---|---|---|
| T001 Branch + log | done | branched from `main` |
| T002 Decisions | done | plan.md D1-D6 |
| T003 version template | done | |
| T004 installer template | done | one machine entry, no switches |
| T005 locale template | done | 16 tags, WebView2 note |
| T006 publish.ps1 | done | 5.1, ASCII, ~350 lines |
| T007 Generate + validate 0.1.5 | **verified** | `Manifest validation succeeded.` |
| T008 Commit manifests | done | `tools/winget/manifests/0.1.6/` |
| T009 Signature gate | **verified** | both rejection paths |
| T010 Real install | **open (manual)** | needs an administrator shell |
| T011 Workflow | done | YAML parsed; not yet run |
| T012 tools/winget/README.md | done | |
| T013 README.md subsection | done | |
| T014 Workflow dry run | **verified** | runs #1 and #2, both green |
| T015 .iss directive | **premise refuted** | change made, then reverted - see below |
| T016 Directive guard | **removed again** | it guarded nothing once the scope entry went |
| T017 Both scopes installed | **refuted** | per-user entry failed validation, withdrawn |
| T018 CLAUDE.md | done | |
| T019 This log | done | |
| T020 CHANGELOG | done | written with the 0.1.6 bump |
| T021 First submission | **done, one round of rework** | winget-pkgs#426038 |
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

## First submission

Opened 2026-08-29 for **0.1.6**, not 0.1.5: 0.1.6 is the release whose
CHANGELOG and README announce winget, and it was published while this feature
was still in progress.

- Pull request: **microsoft/winget-pkgs#426038**
- Installer SHA256 `88AEE5CF60B3459D59979D1A3C01FF9AF7CA8C38EABC18D167773924CE2841CB`,
  8 002 608 bytes, signature verified against `codesign.cfg` before generating
- CLA signed on the pull request (one-off; later versions do not need it)
- Checks 01 Pull Request Validation, 02 Manifest Validation, 03 URLs
  Validation, 04 URL Domain Validation, 06 Catalog Content Verification and
  `license/cla` green; 05 Manifest Policy Validation **neutral**, which is the
  expected outcome for a new package rather than a failure; 07-10 running at
  the time of writing
- Labelled `New-Package`, so it waits on a human reviewer

**Manifest schema 1.10.0 was accepted** by check 02, although the pull request
template mentions 1.12 — the concern noted during implementation was unfounded.

The `manifests/0.1.5/` directory was removed: it was generated while building
the tooling, was never submitted, and keeping it would suggest a release that
never reached the catalogue. The verification runs recorded below were made
against it and stand as written.

## The per-user entry failed validation and was withdrawn

The first submission carried two installer entries — `Scope: machine` with
`Custom: /ALLUSERS` and `Scope: user` with `Custom: /CURRENTUSER`. On
microsoft/winget-pkgs#426038 checks 01–07 and 10 passed, but **08 Installation
Validation failed** after 6m37s and the validator applied
`Validation-Shell-Execute`, followed by `Needs-Author-Feedback` and
`Validation-Guide`. Check 09 was skipped as a consequence.

**Cause.** The catalogue's pipeline runs manifests in an elevated context
(winget-pkgs issue 72224). A `/CURRENTUSER` install then lands in the
administrator's profile, and the package is not detected afterwards. The Inno
Setup half was never the problem and had been verified: `dialog` implies
`commandline`, and a probe with the installer's exact privilege configuration
took `/VERYSILENT /CURRENTUSER`, exited 0 and wrote the `HKCU` uninstall key
with no elevation.

**Fix.** The template now emits one machine-scope entry with no
`InstallerSwitches` and no `ElevationRequirement` — the canonical shape for an
Inno package in this catalogue. Only the installer manifest changed, so the
correction went onto the pull request branch as a single file edit rather than
a new submission. The `PrivilegesRequiredOverridesAllowed` assertion in
`publish.ps1` was removed with it: the manifests no longer depend on that
directive, so it guarded nothing. The directive itself stays in
`tandemcommander.iss`, with its comment corrected to say that nothing outside
Setup depends on it today.

**What this cost, and why.** T017 was recorded in the plan as an *unverified
assumption* — "winget may add a scope switch of its own that displaces
`InstallerSwitches.Custom`" — and the manifest was submitted with it anyway,
in the one pull request that also had to get a brand-new package accepted.
That is the actual mistake: not the technical guess, which was reasonable, but
carrying it into a first submission instead of getting the package in
machine-only and proposing per-user separately. Per-user installation was also
not part of the original request; it entered the design as an option raised
during clarification.

**User-facing texts were wrong too, briefly.** `--scope user` had been promised
in `README.md`, in the `CHANGELOG.md` entry for 0.1.6 and in the release notes
already published on the GitHub release page. All three were corrected to say
the package installs for all users.

## Workflow, verified in production

Both halves of the automation proved themselves on 2026-08-29 without being
staged for it.

**Run #1 fired by itself** when the v0.1.6 GitHub release was published, at a
point when `WINGET_PAT` did not yet exist. It finished green having generated
and validated the manifests and submitted nothing — exactly the intended
degradation. Had that guard not worked, `winget-pkgs` would have received a
second pull request for 0.1.6 alongside the manual one, for a moderator to
close.

**Run #2** was the deliberate `workflow_dispatch` dry run with `submit`
unchecked. It settled three things that had been unverifiable locally:

- the GitHub runner **can** validate the Certum certificate chain — the
  Authenticode check passes there, so a signed release is not rejected in CI;
- `winget` **is** available on `windows-latest`, so the schema is validated on
  the runner too. The "winget is not available here" fallback was written on
  the opposite assumption and stays as a safety net, not as the normal path;
- the run produced `SHA256 88AEE5CF...41CB`, byte-identical to the local run
  and to the submitted manifest.

Consequence to remember: with the secret now in place, the **next** published
release submits on its own. Running `publish.ps1 -Submit` by hand afterwards
would open a duplicate pull request; the script is the fallback for when the
workflow fails, not a second channel.

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

- **T010 and T017 are manual** and cannot be run from here: they install
  software and need an administrator shell. Commands are in
  [quickstart.md](quickstart.md). T014 is done — see *Workflow, verified in
  production* above.
- **T017 also validates an assumption** that testing could not settle here:
  that winget adds no scope switch of its own for `inno` installers that would
  displace `InstallerSwitches.Custom`. If `--scope user` were to install
  per-machine, the switches must move to the `Silent` / `SilentWithProgress`
  fields.
- **T021 is done** — see *First submission* below.
- **tandemcommander.org** still needs the same *Installing* wording as
  `README.md`; that lives outside this repository.
- Both the CHANGELOG entry and the README section state availability as
  following Microsoft's acceptance, so neither claims something untrue in the
  days between the release and the merge.
- Not in scope, but now cheap: `checkver` could be redirected at the GitHub
  Releases API, or dropped in favour of `winget upgrade`.
