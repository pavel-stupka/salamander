# winget distribution

Everything needed to publish Tandem Commander in the **Windows Package
Manager** catalogue, so it can be installed and kept up to date with:

```
winget install tandemcommander
winget upgrade tandemcommander
```

The catalogue does not host the installer — it hosts a *manifest* that points
at the installer already published on GitHub Releases and records its SHA256.
Publishing a version therefore means opening a pull request against
[microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs) with three
YAML files. That is what this directory automates.

| | |
|---|---|
| Package identifier | `PavelStupka.TandemCommander` |
| Short name (moniker) | `tandemcommander` |
| Catalogue path | `manifests/p/PavelStupka/TandemCommander/<version>/` |

The identifier is permanent. Changing it after the package is accepted creates
a *different* package — installed users would not be moved to it — so it is
not something to revise casually.

## Files

| File | Purpose |
|---|---|
| `publish.ps1` | The only entry point: generates, validates and optionally submits |
| `templates/version.yaml.in` | Version manifest template |
| `templates/installer.yaml.in` | Installer manifest template — architecture, scopes, ProductCode |
| `templates/locale.en-US.yaml.in` | **All catalogue metadata**: description, tags, URLs, licence |
| `manifests/<version>/` | Generated output, committed as a record of what was submitted |

**The templates are the source of truth.** To change the description, add a
tag, or fix a URL, edit `templates/locale.en-US.yaml.in` and regenerate — never
edit a file under `manifests/`. Authoring comments in the templates are
stripped from the generated manifests, so explanations can be as long as they
need to be.

## One-time setup

Only needed before the *first* submission, or before the workflow can submit
on its own.

1. **Fork winget-pkgs.** Open <https://github.com/microsoft/winget-pkgs> and
   fork it to the GitHub account that will own the token below. `wingetcreate`
   pushes the branch to that fork and opens the pull request from it.
2. **Create a token.** GitHub → *Settings* → *Developer settings* →
   *Personal access tokens* → *Tokens (classic)* → *Generate new token*.
   The only scope needed is **`public_repo`**. Copy the value once.
3. **For automatic submission**, add it to this repository:
   *Settings* → *Secrets and variables* → *Actions* → *New repository secret*,
   name `WINGET_PAT`. Without this secret the workflow still runs and still
   generates the manifests — it simply does not submit.
4. **For local submission**, set it in the shell instead:
   `$env:WINGET_PAT = '<token>'`.

`wingetcreate` is downloaded on demand when it is not on PATH, so nothing has
to be installed. To have it permanently:
`winget install Microsoft.WingetCreate`.

## Publishing a version

Once the GitHub release for `v<version>` exists **and its installer asset is
uploaded**, publishing is one command:

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\winget\publish.ps1 -Version 0.1.6 -Submit
```

Or nothing at all: the `Publish to winget` workflow runs automatically when a
GitHub release is published and does exactly the same, provided `WINGET_PAT`
is configured. Pre-releases are skipped.

Leave `-Submit` off for a dry run — the manifests are generated and validated,
and nothing leaves the machine:

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\winget\publish.ps1 -Version 0.1.6
```

`-Version` may be omitted; it then defaults to `MyAppVersion` from
`setup\tandemcommander.iss`.

What the script does, in order:

1. Reads the release date and the lead paragraph of that version's
   `CHANGELOG.md` section (the paragraph becomes the catalogue release notes).
2. Downloads the installer from its GitHub Releases URL — the very file users
   will get, not a local build. `-LocalFile <path>` hashes a local build
   instead, for a rehearsal before the release is uploaded.
3. **Verifies the Authenticode signature** against the thumbprint in
   `tools\codesign\codesign.cfg`. An unsigned or differently signed binary
   stops the run; `-SkipSignatureCheck` overrides this for testing only.
4. Computes the SHA256 and renders the three manifests into
   `manifests\<version>\`.
5. Runs `winget validate`.
6. With `-Submit`, opens the pull request through `wingetcreate`.

Commit the generated `manifests\<version>\` directory — it is the record of
what was submitted.

### After submitting

The **first** submission of a new package is reviewed by a human moderator and
can take a few days. Later versions are usually merged automatically once the
pipeline's install/uninstall test in a Windows Sandbox passes. Watch the pull
request for validation labels; a failure there is nearly always the silent
install or the uninstall, both of which can be reproduced locally (below).

## Testing before submitting

### Install straight from the generated manifests

In an **administrator** shell:

```
winget settings --enable LocalManifestFiles
winget install --manifest tools\winget\manifests\0.1.6
winget list PavelStupka.TandemCommander
winget uninstall PavelStupka.TandemCommander
```

The install must be completely silent — no wizard, no disclaimer page, and the
application must not start afterwards. Note that this replaces whatever
Tandem Commander you already have installed, and the uninstall step removes
it; run it only when you mean to, or use the sandbox below instead.

### Reproduce the pipeline's own test

The winget-pkgs repository ships the script the validation pipeline uses. It
installs and uninstalls the package in a throwaway Windows Sandbox:

```
git clone --depth 1 https://github.com/microsoft/winget-pkgs
winget-pkgs\Tools\SandboxTest.ps1 tools\winget\manifests\0.1.6
```

## Install scope

The manifests declare **one** installer: `Scope: machine`, no switches. It
installs into `%ProgramFiles%\Tandem Commander` and asks for administrator
rights, which is what the installer does on its own.

`winget install --scope user` is not offered. A per-user entry was part of the
first submission and was removed when validation failed — but that turned out
not to be the cause (see below), so whether it would work is simply untested.
Adding it back is a change of its own: put the entry in
`templates/installer.yaml.in`, prove it with `Tools\SandboxTest.ps1` from a
winget-pkgs clone, and submit it separately.

## The installer must install silently

The catalogue installs every submitted package unattended. If `/VERYSILENT`
fails, nothing you write in the manifest can save the submission.

That is what sank the first two attempts for 0.1.6. The installer's AI
disclaimer page keeps the *Next* button disabled until a checkbox is ticked,
and a silent install still walks through the wizard's pages — so Setup met a
button it could not press and aborted with exit code 1 and
`Failed to proceed to next wizard page`. Every release from 0.1.0 to 0.1.6 was
affected; unattended installation had never worked. Fixed in 0.1.7 by guarding
`CurPageChanged` with `not WizardSilent` in `setup/tandemcommander.iss`.

**Verify this before every submission**, on the installer you are about to
publish, from an elevated shell:

```powershell
$log="$env:TEMP\tc_silent.log"; $p=Start-Process 'setup\output\tandemcommander-<version>-x64-setup.exe' -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',"/LOG=$log" -Wait -PassThru; "EXIT CODE: $($p.ExitCode)"; Get-Content $log -Tail 20
```

`EXIT CODE: 0` and `Installation process succeeded.` in the log. Anything else
means the submission will fail, whatever the manifest says. Run it elevated —
from a normal shell the UAC prompt cannot be answered and you get exit code 2,
which looks like a defect but is not one.

## Troubleshooting

**`the installer is not validly signed`** — the release asset was built
without `sign`. Rebuild with `build.cmd full release sign setup`, re-upload the
asset, and run again. Never publish an unsigned installer: SmartScreen warns
on it and the catalogue makes it far more widely downloaded.

**`the installer reports version 'x' but y is being published`** — a warning,
not an error: the downloaded file's version resource disagrees with
`-Version`. Usually the wrong `-LocalFile`, or an asset uploaded to the wrong
tag.

**`could not download the release asset`** — the release exists but the
installer is not attached to it yet, or the tag is not `v<version>`.

**`setup\tandemcommander.iss has no PrivilegesRequiredOverridesAllowed`** —
that directive was removed or commented out. Restore it; see *Install scopes*
above for why the manifests depend on it.

**`winget validate rejected the manifests`** — a template edit broke the
schema. The message names the field. The schema for each file is linked from
its first line, and editors with a YAML language server will flag mistakes as
you type.

**`no GitHub token`** — `-Token` was not passed and `WINGET_PAT` is not set;
see *One-time setup*.

**The pull request pipeline fails on installation** — reproduce it with
`SandboxTest.ps1` above. Almost always the installer, not the manifest.
