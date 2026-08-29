# Remaining Work — Feature 072 (winget distribution)

**As of 2026-08-29.** The feature itself is done and shipped: Tandem Commander
0.1.7 is submitted to the catalogue as `PavelStupka.TandemCommander`
([microsoft/winget-pkgs#426090](https://github.com/microsoft/winget-pkgs/pull/426090)),
the tooling is in `tools/winget/`, and the workflow submitted that pull request
by itself when the GitHub release was published.

Everything below is follow-up work, ordered by what it costs users. Nothing
here blocks the feature.

---

## Gate: is #426090 merged?

**Check this first — it decides whether the rest is worth starting.**

A new package waits on a human moderator; expect days. While the pull request
is open, do **not** change `tools/winget/templates/` and do **not** publish a
release: the workflow submits automatically on release publication, and a
second, unreviewed change during moderation is how the first attempt went
wrong.

Once merged (allow a few hours for the index):

```
winget install tandemcommander
winget show PavelStupka.TandemCommander
```

After that, the cost of a mistake drops sharply — a failed *update* pull
request is closed and resubmitted, with no bearing on whether the package
itself is accepted.

---

## P1 — Upgrading over a running instance aborts the install

**Affects real users the moment the package is in the catalogue.**

With Tandem Commander open, a silent install fails:

```
RestartManager found an application using one of our files: Tandem Commander, File Manager
Some applications could not be shut down.
Defaulting to Abort for suppressed message box (Abort/Retry/Ignore)
EXIT CODE: 5   (changes rolled back)
```

`winget upgrade` passes `/SUPPRESSMSGBOXES`, so the Abort/Retry/Ignore prompt
is answered with **Abort**. Anyone who upgrades with the program open gets a
failure. This is not a regression — the installer has always behaved this way;
it simply never mattered before, because upgrading meant running the installer
by hand and seeing the prompt.

**The fix belongs in the application, not the installer**: respond to the
Restart Manager's shutdown request (`WM_QUERYENDSESSION`, and
`RegisterApplicationRestart` if the session should come back afterwards) so
Setup can close the program and restart it. `salmon` (the bug reporter) was
also listed as holding files and needs the same treatment.

**First step**: reproduce with the program open, using quickstart §2b's
command, and confirm exit 5. Then decide whether the panels' state should be
preserved across the restart — that is the design question, not the API.

Worth a feature of its own; scope is `src/`, not `setup/`.

---

## P2 — Per-user installation is untested, not impossible

`winget install --scope user` is not offered. The manifest entry was part of
the first submission, was blamed for the validation failure and removed — and
then the machine-only manifest failed identically, which refuted that. The
real defect (the disclaimer page aborting silent installs, fixed in 0.1.7)
masked everything. **Whether a per-user entry works has never actually been
tested.**

What is known: Inno Setup accepts the switches.
`PrivilegesRequiredOverridesAllowed=dialog` implies `commandline`, and a probe
built with the installer's exact privilege configuration took
`/VERYSILENT /CURRENTUSER`, exited 0, installed into
`%LOCALAPPDATA%\Programs\<AppName>` with no elevation and wrote the `HKCU`
uninstall key `{AppId}_is1`.

**No manual manifest editing is needed** — the generator has no per-scope
logic left, so the template is the whole definition. But the workflow submits
on the next release publication, so the entry must be proven *before* it lands
on `main`.

**Procedure — needs no new release**, because `publish.ps1` works against the
already-published asset:

1. On a branch, add back to `tools/winget/templates/installer.yaml.in`:
   ```yaml
     - Architecture: x64
       Scope: user
       InstallerUrl: <same as the machine entry>
       InstallerSha256: {{SHA256}}
       InstallerSwitches:
         Custom: /CURRENTUSER
   ```
   (and `Custom: /ALLUSERS` on the machine entry, if both are wanted)
2. `publish.ps1 -Version <current released version>` — generates and validates
3. `Tools\SandboxTest.ps1` from a winget-pkgs clone against that directory —
   the environment that rejected it twice. Windows Sandbox must be enabled;
   it is not available on GitHub runners, so this cannot be automated.
4. Passes → merge and submit as a pull request of its own. Fails → drop the
   branch; nothing reached the catalogue.

---

## P3 — `checkver` points at upstream and there is no update feed

The legacy `checkver` plugin still checks Open Salamander's site, and is
disabled in `plugins.cfg` anyway. With winget in place, updates are handled
for anyone who installs from the catalogue — but not for anyone who downloaded
the installer from the website.

Two ways out, and the choice is a product decision:

- point `checkver` at the GitHub Releases API, or
- drop it and treat winget as the update channel, saying so on the website.

Noted in `architecture/10-plugin-maintenance-outlook.md:90`.

---

## P4 — Node.js 20 deprecation warning in every workflow run

`actions/checkout@v4` and `actions/upload-artifact@v4` run on Node 20, which
GitHub has deprecated; the runner forces Node 24 and prints a warning on every
run. It is cosmetic today and will not stay that way.

All four workflows are affected — `winget-publish.yml`, `pr-msbuild.yml`,
`auto-label-author.yml`, `pr-comments-guard.yml` (the last two through
`actions/github-script@v7`). Bump them together, so the repository does not
end up with two conventions.

---

## Not remaining, recorded so it is not re-litigated

- **The version boundary for scope switches** (`$UserScopeSinceVersion`) was
  removed and must not come back: Inno Setup's `dialog` override implies
  `commandline`, so every released version accepts the switches. See plan.md
  D6.
- **The `PrivilegesRequiredOverridesAllowed` assertion** in `publish.ps1` was
  removed with the per-user entry. If P2 reinstates the entry, reinstate the
  assertion with it — it exists to stop the manifests advertising an install
  mode the installer would reject.
- **Manifest schema 1.10.0 is accepted** by the catalogue, although the pull
  request template mentions 1.12.
