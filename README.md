# Tandem Commander

Tandem Commander is a fast, keyboard-friendly two-panel file manager for Windows. It is based on [Open Salamander](https://github.com/OpenSalamander/salamander), the GPLv2 open-source release of the long-lived Altap Salamander file manager. Everything about the original project — its history, features, documentation, and community — lives in the upstream repository; this README covers what makes Tandem Commander different and how to build it.

**Website**: [tandemcommander.org](https://tandemcommander.org) 

**Issues**: [github.com/tandemcommander/tandemcommander/issues](https://github.com/tandemcommander/tandemcommander/issues)

## Installing

Download the signed installer from the [Releases page](https://github.com/tandemcommander/tandemcommander/releases/latest) and run it. Windows 11 or newer, 64-bit.

From version 0.1.6 the application is also published in the Windows Package Manager catalogue, so once Microsoft has accepted the submission it can be installed and kept up to date with:

```
winget install tandemcommander
winget upgrade tandemcommander
```

Add `--scope user` to install into your own profile without administrator rights.

## Why This Project Exists — A Personal Note

Whenever I have worked on Windows, there has always been a Salamander close at hand. First Servant Salamander, later Altap Salamander — in my view one of the finest pieces of software I have ever worked with. But the last released version of Altap Salamander dates back to 2019, and it still carries a few aches inherited from the original versions: incomplete encoding support, missing long directory and file path support, and quite a few others. At the same time, many of my friends and family members use Altap Salamander every day, both at work and at home.

The moment Salamander was released as Open Salamander, I had a clear vision: I would adapt it for myself so I could simply keep using it. That vision quickly ran into hard reality — even though I am a very experienced software engineer, developer, and architect, I was missing the one factor a project of this scale demands above all else: time.

That has changed, essentially within the last few months, with the overall shift in how software can now be — not programmed, but *created* — with agentic systems. To be clear, I am emphatically not talking about "vibe coding"; I consider that a bit of a buzzword and I do not like it. The term I would choose is **Full Agentic Spec-Driven Development**: clearly defined procedures not just for producing code, but for producing the entire project — documentation and artifacts included. This project is deliberately built with the spec-driven approach on top of GitHub SpecKit, using the best agentic models available at the time — currently Fable 5, with reviews also carried out by GPT-5.6 Sol and others.

Tandem Commander is, of course, not perfect. But thanks to this approach I ended up with a genuinely usable tool — one that I hope can serve others as well.

## A New Era of Development

Tandem Commander explores what happens when a mature, quarter-century-old C++ codebase meets the new era of agentic programming. Development follows Spec-Driven Development principles built on [GitHub SpecKit](https://github.com/github/spec-kit): every change begins as a written specification that is clarified, planned, and decomposed into tasks before any code is touched. The implementation itself is carried out by a combination of agentic coding frameworks using the best models available at the time — currently Anthropic Fable 5.

> **A note on naming**: since version 0.1.0 the application itself carries the Tandem Commander identity — the binary is `tandemcommander.exe`, the window titles, About dialog, and icons use the new name and visual style, and configuration lives under its own registry root (`HKCU\Software\Tandem Commander`), fully separate from any Open Salamander installation. Source files, internal identifiers, and the solution name (`salamand.sln`) intentionally keep their upstream names. The HTML help is not yet rebranded.

## About the Name: Tandem Commander

The original name *Servant* Salamander carried a philosophy I fully identify with: the program is there to **serve** the user, never the other way around. Still, a new name was needed — I did not want to trade on the original one, and I also wanted the project to sit more recognizably among the other two-panel file managers, the "commanders".

I read *Commander* accordingly: it is a tool through which the **user commands the files** — not a program that commands the user. And *Tandem* simply felt fitting and unique — two panels working side by side, in tandem.

## Thank You, Open Salamander Authors

None of this would exist without the people who built Servant Salamander, Altap Salamander, and finally Open Salamander. Over a quarter of a century they created and refined a program that countless people — me, my friends, my family — have trusted with their daily work, and it shows in every detail: the speed, the keyboard-first design, the plugin architecture, the sheer care in the code this repository inherited. Their decision to release it under an open-source license was an act of real generosity that gave this remarkable software a second life. Tandem Commander stands entirely on their shoulders, and I hope it honors what they built. To all the original authors and contributors, listed in [AUTHORS](AUTHORS): thank you.

## Building

### Prerequisites

- Windows 11 or newer
- [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) (any edition) with the **Desktop development with C++** workload
- Windows 10/11 SDK (the projects use the latest SDK installed with the workload)
- Optional: the `OPENSAL_BUILD_DIR` environment variable to choose the build output directory — the value must end with a backslash (e.g. `D:\Build\TandemCommander\`). When unset, the build defaults to `.\build\` under the repository root.
- Optional: [Git](https://git-scm.com/downloads), and [PowerShell 7.4+](https://learn.microsoft.com/en-us/powershell/scripting/install/installing-powershell-on-windows) to run the `normalize.ps1` source formatter

### Build commands

Run `build.cmd` from the repository root:

```batch
build.cmd                :: incremental Debug x64 build
build.cmd rebuild        :: full clean + rebuild
build.cmd release        :: Release x64 build
build.cmd full           :: complete build: app + plugins + language modules,
                         ::   plus runtime data files and plugin registration
build.cmd full release   :: complete Release x64 build
```

Arguments can be combined in any order (`build.cmd help` shows the full usage). The set of plugins that is compiled and shipped is controlled by [`plugins.cfg`](plugins.cfg) in the repository root — one `name=on|off` line per plugin.

## Release, Code Signing & Installer

Release builds are code-signed and packaged **strictly on demand** — a plain
`build.cmd full release` never signs anything, never contacts a timestamp
server, and behaves exactly like a development build. Signing and installer
packaging are extra arguments:

```batch
build.cmd full release sign          :: complete Release build, then sign every
                                     ::   shipped binary (exe, dll, spl, slg)
build.cmd full release sign setup    :: one-command signed release: signed build
                                     ::   + signed Inno Setup installer
setup\build_setup.cmd                :: unsigned installer from an existing
                                     ::   Release tree (development test)
setup\build_setup.cmd sign           :: sign the Release tree if needed, then
                                     ::   build a signed installer + uninstaller
```

### Additional prerequisites for releases

- The maintainer's code-signing certificate installed in the Windows
  certificate store (current user, `My`); its SHA-1 thumbprint and the
  timestamp authority are committed in
  [`tools/codesign/codesign.cfg`](tools/codesign/codesign.cfg) — the private
  key never enters the repository
- [Inno Setup 7](https://jrsoftware.org/isinfo.php) for the installer
  (`ISCC.exe` is located automatically; it does not need to be on `PATH`)

### How signing works

The signing core is `tools\codesign\sign_release.ps1` (Windows PowerShell
5.1). It sweeps the Release output tree, signs every PE artifact
(`*.exe`, `*.dll`, `*.spl`, `*.slg`) with the configured certificate —
SHA-256 digests, RFC 3161 timestamp — and ends with a verification pass. The
sweep is **idempotent**: files already signed by the configured certificate
are skipped, so re-running after a network hiccup only finishes what is
missing, and a re-run over a fully signed tree completes in seconds. Files
signed by an *older* certificate are re-signed automatically.

```batch
:: sign an existing build without rebuilding:
powershell -NoProfile -ExecutionPolicy Bypass -File tools\codesign\sign_release.ps1 ^
    -Root "build\tandemcommander\Release_x64"

:: audit signing state without modifying anything:
powershell -NoProfile -ExecutionPolicy Bypass -File tools\codesign\sign_release.ps1 ^
    -Root "build\tandemcommander\Release_x64" -VerifyOnly
```

The signed installer is produced by `setup\build_setup.cmd sign`, which first
runs the same sweep (a signed installer can never package unsigned binaries)
and then compiles `setup\tandemcommander.iss` with `/DSIGN=1`, so Inno Setup
signs both the installer and the uninstaller it deploys. The result lands in
`setup\output\`.

Release output trees contain only distribution files: linker byproducts
(`.pdb`, `.lib`, `.exp`) are redirected outside the tree at build time (PDBs
are preserved under the `obj\` intermediate root for crash-dump
symbolication), and the installer excludes those file types independently as
a safety net.

### Certificate rotation

1. Install the new certificate into the Windows certificate store.
2. Update `thumbprint` in `tools\codesign\codesign.cfg` (one line).
3. Re-run any signing command — every artifact still carrying the old
   certificate is re-signed; timestamps keep previously released binaries
   valid after the old certificate expires.

### Publishing to winget

Released versions are also published to the **Windows Package Manager**
catalogue as `PavelStupka.TandemCommander`, so users can install and update
with `winget install tandemcommander` / `winget upgrade tandemcommander`.

The catalogue stores a manifest that points at the installer already published
on GitHub Releases, so publishing a version means opening a pull request
against [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs) —
which is one command once the release asset is uploaded:

```batch
powershell -NoProfile -ExecutionPolicy Bypass -File tools\winget\publish.ps1 ^
    -Version 0.1.6 -Submit
```

Without `-Submit` the manifests are only generated into
`tools\winget\manifests\<version>\` and validated, which is the safe way to
review them first. The script downloads the published installer, **verifies
its Authenticode signature** against `tools\codesign\codesign.cfg`, and takes
the release date and release notes from `CHANGELOG.md`. The
`Publish to winget` GitHub workflow runs the same script automatically when a
release is published.

Catalogue metadata (description, tags, URLs) lives in
`tools\winget\templates\` — see
[`tools/winget/README.md`](tools/winget/README.md) for the one-time token
setup, local install testing, and troubleshooting.

## Development Process

Features are developed one at a time through the SpecKit workflow: **specify → clarify → plan → tasks → implement**. Each feature lives in the [`specs/`](specs/) directory with its full paper trail — specification, implementation plan, task breakdown, research notes, and contracts — committed alongside the code, so the repository records not only what changed but why. Project-wide rules (build reproducibility, backward compatibility, incremental modernization, Windows platform commitment, plugin architecture preservation) are codified in the project constitution at `.specify/memory/constitution.md`.

## Repository Structure

| Directory | Purpose |
|-----------|---------|
| `src/` | C++ source code: core application, shared libraries (`src/common/`), plugins (`src/plugins/`) |
| `src/vcxproj/` | Visual Studio solution (`salamand.sln`) and project files |
| `specs/` | Spec-Driven Development artifacts: one directory per feature |
| `architecture/` | Architecture documentation: build pipeline, dependencies, plugin API |
| `convert/` | Character conversion tables |
| `doc/` | Licenses and third-party notices |
| `help/` | User manual source (HTML Help) |
| `tools/` | Build utilities |
| `translations/` | UI translations |

See the [`architecture/`](architecture/) documents for a much deeper analysis.

## License

Tandem Commander, like the Open Salamander project it derives from, is open-source software licensed under [GPLv2](doc/license/license_gpl.txt) and later. Individual files and libraries carry [different but compatible licenses](doc/third_party.txt). Contributors are listed in [AUTHORS](AUTHORS).
