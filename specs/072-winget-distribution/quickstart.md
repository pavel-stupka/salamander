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
- report `Scopes        : machine + user`
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

## 3. The installer-directive guard (FR-009)

The manifests offer a per-user install only because
`setup\tandemcommander.iss` permits the Inno Setup scope switches. Nobody
edits that file with winget in mind, so the generator refuses to run without
it. To prove the guard still works, comment the directive out temporarily:

```
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "(Get-Content setup\tandemcommander.iss) -replace '^PrivilegesRequiredOverridesAllowed', ';PrivilegesRequiredOverridesAllowed' | Set-Content setup\tandemcommander.iss"
powershell -NoProfile -ExecutionPolicy Bypass -File tools\winget\publish.ps1 -Version 0.1.6
git checkout -- setup\tandemcommander.iss
```

The middle command must fail with
`ERROR: setup\tandemcommander.iss has no PrivilegesRequiredOverridesAllowed ...`.
Confirm `git status` is clean afterwards.

### Why no version boundary

The plan assumed older releases could not honour `/ALLUSERS` and
`/CURRENTUSER`, and gated them out. That was wrong: Inno Setup enables the
command-line switches for the `dialog` override mode too, which
`tandemcommander.iss` has always had. Verified with a probe installer carrying
the *exact* privilege configuration of the real one — `/VERYSILENT
/CURRENTUSER` exited 0, installed into `%LOCALAPPDATA%\Programs\<AppName>`
with no elevation, and wrote the `HKCU` uninstall key. Every released version,
0.1.5 included, therefore supports both scopes.

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

### 4b. Per-user installation **(manual)** (US3)

No special build is needed. In a **normal, non-elevated** shell:

```
winget install --manifest tools\winget\manifests\0.1.6 --scope user
winget uninstall PavelStupka.TandemCommander --scope user
```

Must complete **without any UAC prompt** and install into
`%LOCALAPPDATA%\Programs\Tandem Commander`. Then confirm the machine scope
still works from an administrator shell with `--scope machine`, installing into
`%ProgramFiles%\Tandem Commander`.

Note that a per-user and a per-machine installation can coexist; uninstall
whichever the test created.

This step also confirms the assumption that winget adds no scope switch of its
own that would displace `InstallerSwitches.Custom`. If `--scope user` were to
install per-machine, or the install were to fail with an Inno Setup command
line error, that assumption is wrong and the switches must move to the
`Silent`/`SilentWithProgress` fields instead.

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

Must: finish green, resolve the version from `setup/tandemcommander.iss`, print
`Manifest validation succeeded` or the "winget is not available here" warning,
and attach a `winget-manifests-<version>` artifact containing the three files.

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
