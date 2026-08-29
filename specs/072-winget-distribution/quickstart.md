# Quickstart: validating winget distribution

What to run and what must happen. Each step names the requirement it covers.
Steps marked **(manual)** need an administrator shell and really install
software; they cannot be automated here.

## Prerequisites

- winget (stock Windows 11) — `winget --version`
- The repository checked out; no build needed for steps 1-3
- Network access to `github.com`
- For step 5: `WINGET_PAT` set, and a fork of `microsoft/winget-pkgs` under
  the account owning it (see `tools/winget/README.md`)

---

## 1. Generate and validate for a released version (FR-002..FR-005, SC-001)

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\winget\publish.ps1 -Version 0.1.6
```

Must:

- download the asset from
  `https://github.com/tandemcommander/tandemcommander/releases/download/v0.1.6/tandemcommander-0.1.6-x64-setup.exe`
- report `SHA256 : 88AEE5CF60B3459D59979D1A3C01FF9AF7CA8C38EABC18D167773924CE2841CB`
- report `Release date  : 2026-08-29` (taken from `CHANGELOG.md`, not typed)
- report `Scope         : machine`
- write three files into `tools\winget\manifests\0.1.6\`
- print `Manifest validation succeeded.` and exit 0

Then check the generated installer manifest contains **two** entries under
`Installers:` over the same URL and hash — `Scope: machine` with
`Custom: /ALLUSERS` and `ElevationRequirement: elevatesSelf`, and `Scope: user`
with `Custom: /CURRENTUSER` and no `ElevationRequirement` (SC-003) — and that
`ProductCode` is `'{35C0B0DC-DB73-429C-AAA8-FBC41C937F66}_is1'`.

## 2. Signature gate (FR-004, SC-004)

Point the generator at another signed executable large enough to pass the
size check:

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\winget\publish.ps1 ^
    -Version 0.1.6 -LocalFile C:\Windows\explorer.exe
```

Must fail with
`ERROR: the installer is signed by an unexpected certificate (CN=Microsoft Windows, ...)`,
warn first that the file's version does not match, and leave
`tools\winget\manifests\0.1.6\` untouched — the check runs before anything is
written. A file under 1 MB is rejected earlier still, with
`the installer is only N bytes - wrong file?`.

## 3. One installer entry, machine scope (SC-003)

Check the generated installer manifest. Under `Installers:` there must be
**exactly one** entry:

```yaml
Installers:
  - Architecture: x64
    Scope: machine
    InstallerUrl: ...
    InstallerSha256: ...
```

No `InstallerSwitches`, no `ElevationRequirement`, no second entry. That is the
shape the catalogue's Installation Validation accepts for an Inno Setup
package, and the installer does per-machine installation on its own anyway.

### Why there is no per-user entry

The first submission carried a second `Scope: user` entry with
`Custom: /CURRENTUSER`, plus `/ALLUSERS` on the machine entry. It failed check
08 Installation Validation on microsoft/winget-pkgs#426038 with the label
`Validation-Shell-Execute` and was removed.

Inno Setup is not the obstacle and was verified: with
`PrivilegesRequiredOverridesAllowed=dialog` (which implies `commandline`) a
probe built with the installer's exact privilege configuration accepted
`/VERYSILENT /CURRENTUSER`, exited 0, installed into
`%LOCALAPPDATA%\Programs\<AppName>` with no elevation and wrote the `HKCU`
uninstall key. The untested half was the catalogue's pipeline, which runs
manifests elevated (winget-pkgs issue 72224) so the install lands in the
administrator's profile and is not found afterwards.

Before retrying it: add the entry back to `templates/installer.yaml.in`, prove
it with `Tools\SandboxTest.ps1` (§4c), and submit it on its own.

## 4. Real installation **(manual)** (US1, SC-002)

Administrator shell, once per machine:

```
winget settings --enable LocalManifestFiles
```

Then:

```
winget install --manifest tools\winget\manifests\0.1.6
winget list PavelStupka.TandemCommander
winget uninstall PavelStupka.TandemCommander
```

Must:

- install with **no wizard, no licence page, no disclaimer page**, and must
  **not** start the application afterwards
- appear in `winget list` as `Tandem Commander  PavelStupka.TandemCommander  0.1.6`
- uninstall silently and leave no entry in *Apps & features*

### 4b. Per-user installation — not offered

`winget install --scope user` is deliberately not available; see §3. If it is
ever retried, this is the test it has to pass, from a **normal, non-elevated**
shell:

```
winget install --manifest tools\winget\manifests\<version> --scope user
winget uninstall PavelStupka.TandemCommander --scope user
```

It must complete **without any UAC prompt**, install into
`%LOCALAPPDATA%\Programs\Tandem Commander`, and — the part that actually
failed in the catalogue — still be found by `winget list` afterwards. Run it
under §4c as well, since that is the environment that rejected it.

### 4c. The pipeline's own test **(manual, optional)**

Reproduces exactly what the winget-pkgs validation runs, in a throwaway
Windows Sandbox:

```
git clone --depth 1 https://github.com/microsoft/winget-pkgs
winget-pkgs\Tools\SandboxTest.ps1 tools\winget\manifests\0.1.6
```

## 5. Workflow (FR-007)

On this branch, *Actions* -> *Publish to winget* -> *Run workflow*, leaving
`submit` unchecked and `version` empty.

Must: finish green, resolve the version from `setup/tandemcommander.iss`,
print `Manifest validation succeeded.` and attach a
`winget-manifests-<version>` artifact containing the three files. The runner
does have `winget`, so the schema really is validated there — the fallback
warning path exists for hosts that do not.

With no `WINGET_PAT` configured, a run with `submit` checked must also finish
**green**, printing `WINGET_PAT is not configured - generating and validating
the manifests only.`

## 6. Submission (FR-006) — irreversible, public

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\winget\publish.ps1 ^
    -Version 0.1.6 -Submit
```

Opens a pull request in `microsoft/winget-pkgs`. The first submission of a new
package is reviewed by a human moderator; expect days, and expect questions
about the identifier or the publisher. Later versions usually merge
automatically once the sandbox install/uninstall test passes.

Do not run this step until steps 1-4 have passed.
