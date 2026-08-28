# Feature 071 — implementation log

Running record of what was done, verified and changed while implementing
`tasks.md`. A ticked task in `tasks.md` means the step here says "verified",
not merely "written".

**Started**: 2026-08-28 · **Branch**: `071-configurable-command-shell` ·
**Baseline**: 0.1.5 (build 189), `saltests` 1301/0

## Status by task

| Task | State | Note |
|------|-------|------|
| T001–T004 | done | log, `src/common/salshell.*` + `src/cmdshell.cpp` registered in both vcxproj + filters, `_WORDS` extended |
| T005–T013 | done | module, 52 saltests checks, config fields/defaults/persistence, ids, 14 strings, template — build green, saltests 1353/0 |
| T014–T017 | done | page class + impl, `OpenCommandShell()`, handler dispatch; compiled, guard clean |
| T018 | done (automated GUI run) | L01–L12 of the GUI matrix below: default parity, all three entry points, PowerShell / Windows Terminal / Git Bash in the panel directory, non-ASCII folder, UNC, archive panel, long path |
| T019–T023 | done | argument table + wrappers, wide `$[ENV]`, `SafeGetOpenFileNameW`, Custom controls/Browse/pre-fill/Validate, Custom launch branch |
| T024 | done (automated GUI run) | L13/L13b + D05–D12: `$(FullPath)` expansion in a non-ASCII folder, pre-fill, kept text, blank/bad-placeholder refusal, Browse, save, Cancel |
| T025–T026 | done | not-found marking + refusal, E1 with Help |
| T027 | done (automated GUI run) | D03/D04 (PowerShell 7 marked "(not found)", OK refused), L14 (E2 for `C:\nowhere\shell.exe`), L15 (E1 for PowerShell 7) |
| T028 | done | see Verification |
| T029–T031 | done | new topic, alias/TOC/index, updated *Opening Command Shell*; `.chm` not compiled here (no HTML Help Workshop) |
| T032 | done | guard strict `TOTAL: 0`, draft clean for new code, clang-format: 0 changes, BOM+CRLF verified |
| T033 | done (automated GUI run, light theme) | CZ01/CZ02 + `verification/page-cs.png`: Czech tree label, controls, combo items, "(nenalezeno)", validation message; dark theme not exercised (the machine runs Theme Mode 0) |
| T034 | done with two gaps | everything in quickstart §1–§6 that can be observed from outside the app is covered by the matrix below; not exercised: multi-monitor placement (single-monitor session), dark theme, Group Policy restriction, Export/Import round trip (values persist through SaveConfig/LoadConfig, export is the generic branch copy), the `.chm` compile |
| T035 | done | CLAUDE.md entry |
| T036 | not in this run | ship gate = release decision (version bump + CHANGELOG); feature 070's entry is pending too |

## Decisions taken while implementing

- **Probe enumeration contract** (`RegSubKeyString`): returns FALSE only when
  there is no subkey at `index`; a subkey without the value yields TRUE + `""`,
  so a stray subkey cannot end the walk early. The data model's
  `RegFirstSubKeyString` became index-based enumeration for exactly this
  reason — the existence check stays in the tested algorithm.
- **`$(FullPath)` executor**: a dedicated `ExecuteExpCmdShellDir` instead of
  `ExecuteExpFullPath2` — the latter returns `"C:\"` for an empty `Name`
  (`strrchr` miss), which would have given a plugin-FS panel a bogus `C:\`;
  the new executor returns `""` there and otherwise behaves identically
  (no trailing backslash, root excepted).
- **`SalGetEnvVarU8` return contract** = the API's (0 / required size / stored
  length) so the one shared `$[ENV]` site in `DoExpandVarString` keeps its
  not-found / too-long distinction unchanged.
- **Argument buffer size constant** `SAL_SHELL_ARGS_MAX` (= `USRMNUARGS_MAXLEN`)
  defined in `salshell.h` so `cfgdlg.h` needs no `usermenu.h` include;
  `salshell.h` is included from `src/precomp.h` (not `cfgdlg.h`), where the
  other common headers are.
- **Blank Custom program at launch** (registry edited by hand): shown with the
  E1 text and the *Custom program* name rather than a fourth new string.
- **Over-long command line**: no separate check (T023 suggested reusing the
  User Menu's "too long" message, which names the User Menu); `SalCreateProcess`
  fails and E2 reports the system reason instead — never silent.
- **Browse result** is set verbatim (the dialog returns an absolute path);
  `SalGetFullName` was skipped because its default buffer contract is
  `MAX_PATH` and would have silently dropped a longer pick.
- **Validation messages**: the expansion engine's own variable-error messages
  are used for bad `$(...)`/`$[...]` syntax (they name the offending variable);
  a blank program gets the new `IDS_CMDSHELL_PROGRAMREQUIRED`. The contract's
  mention of `IDS_INCORRECTSYNTAX` ("File mask is syntactically incorrect")
  would have been the wrong wording.

## Verification results

- 2026-08-28 `build.cmd` (Debug x64): green, 38 s incremental; only warnings in
  touched files are two pre-existing `C4018` in `salamdr2.cpp` (lines 1036/1044,
  blamed to the 2026-08-19 commit, unrelated to the one-line `$[ENV]` change).
- `saltests.exe`: **1353 checks, 0 failed** (1301 baseline + 52 new in
  `TestCommandShell071`: candidate order/fallback per preset, not-found,
  non-ASCII `%LOCALAPPDATA%`, `RegEnum` skipping a subkey without exe/value,
  `PackagePath` fall-through, recipes, `SalGetEnvVarU8` round trip of
  `C:\Users\Jiří 中\x`, real-probe smoke for cmd + powershell).
- `python tools\check_encoding.py --strict`: `TOTAL: 0`; `--draft --format
  list` has no hit mentioning `cmdshell`/`salshell`.
- Translations: `build_langs.cmd --export-templates --module salamand` →
  `translate.merge --module salamand --dry-run` (22 gaps × 8 languages) →
  `translate.merge` (**8,088 DeepL characters, 0 validation failures**) →
  `translate.slt --verify` (298 files byte-exact). Terminology check below.
- Translation terminology round (`translations/ui-overrides.json`, section
  `salamand`, 43 pins): the page caption (keyed `#Command Shell`, so
  `IDS_COMMANDSHELL` and the dialog caption agree) follows each language's
  existing *Command Shell* menu term — cs "Příkazový řádek" (was already),
  de "Eingabeaufforderung" (DeepL: "Befehlsshell"), fr "Interpréteur de
  commandes" (DeepL: "Shell de commande"), nl "Opdracht Shell" (DeepL left
  English), ro "Comanda Shell", sk "Príkazový riadok" (DeepL: "Príkazový
  shell"); hu/es were consistent already. The two launch messages were
  re-pinned in all 8 languages because DeepL padded `\n` with spaces (" \n \n "
  — no such row existed in the corpus before) and, in de/fr/es, used the
  informal register (corpus: 207× "Sie" vs 1× "du", 26× "vous" vs 1× "tu");
  `IDS_CMDSHELL_PRESETNOTFOUND/PROGRAMREQUIRED/BROWSETITLE/ERRTITLE` likewise
  for de/fr/es(/nl). "Windows Terminal" kept as the product name in ro/sk/es
  (cs "Terminál Windows" and fr "Terminal Windows" are Microsoft's own
  localized names and stayed). Side effect, deliberate: French `10087`
  "Interpréteur de commande" → "Interpréteur de commandes" (now equal to the
  menu item). No other pre-existing row changed in any language (checked with
  `git diff -U0`). Offline replay after pinning: 0 validation failures,
  `.slt --verify` byte-exact, pinned rows carry `human` provenance.
- `build.cmd full` (Debug x64) after the translation round: green, **189
  language modules built, version check OK across 21 modules**; all eight
  `lang\*.slg` regenerated with the new dialog and strings.
- clang-format 17.0.3 (repo `.clang-format`): 0 changes in the three new files;
  all touched files BOM + CRLF (checked by script), no mixed endings.
- Preset detection on this machine (from the real-probe smoke + quickstart
  facts): cmd `C:\WINDOWS\system32\cmd.exe`, Windows PowerShell found; Windows
  Terminal alias and Git Bash present; PowerShell 7 absent (the "not found"
  case for US3).

## GUI verification (automated, 2026-08-28 evening, after the user stopped his own instance)

Harness: `verification/Gui071.ps1` (+ `Cwd.ps1`) — starts the Debug build with
`-a <dir>`, sets the setting through the registry or the real dialog, triggers
the command by `PostMessage(CM_DOSSHELL)`, by the `Num /` key and by `Ctrl+/`
(`keybd_event` with the file panel focused), finds windows with `EnumWindows`,
drives the property sheet with `TVM_SELECTITEM`, `CB_SETCURSEL`+`CBN_SELCHANGE`,
`WM_SETTEXT`/`WM_GETTEXT` and posted button clicks, reads message boxes from
their `Static` controls, and **reads the working directory of every launched
process from its PEB** (`NtQueryInformationProcess` → `RTL_USER_PROCESS_PARAMETERS`).
The user's setting (preset 3, Czech UI) is restored at the end of every run.
Results below are the last run of each case (`gui-results.md` has the raw lines).

| Case | Result | What was observed |
|---|---|---|
| L01 Command Prompt, `CM_DOSSHELL` | PASS | `cmd.exe` cwd = panel dir (hosted by Windows Terminal — the machine's default terminal) |
| L02 Command Prompt, `Num /` | PASS | same, via the accelerator with the panel focused (in the command line box `Num /` types `/` — unchanged behaviour) |
| L03 Command Prompt, `Ctrl+/` | PASS | same, via `VK_OEM_2` |
| L04 Windows PowerShell | PASS | `powershell.exe` cwd = panel dir |
| L05 Windows Terminal | PASS | `wt.exe -d .` → default profile (Git Bash here) → `bash.exe` cwd = panel dir |
| L06 Git Bash | PASS | `git-bash.exe` → `mintty` → `bash.exe`, all cwd = panel dir |
| L07 / L08 PowerShell, Git Bash in `…\Můj disk\Nový projekt` | PASS | cwd equal incl. diacritics |
| L09 long path (281 chars) on C: | PASS | `cmd.exe` cwd = the 8.3 form `…\071long\ABCDEF~1\…` — the R19 retry |
| L09b long path (285 chars) on E: (volume without 8.3 names) | PASS | E2: "Cannot start … cmd.exe … (267) Název adresáře je neplatný … Configuration / Command Shell" — nothing launched |
| L10 PowerShell on `\\localhost\E$\…` | PASS | cwd = the UNC path |
| L11 Command Prompt on UNC | PASS | `cmd.exe` falls back to `C:\Windows` (its own documented behaviour) |
| L12 panel inside `test.zip` | PASS | cwd = the folder containing the archive |
| L13 / L13b Custom `$[SystemRoot]\System32\cmd.exe` + `/k cd > "$(FullPath)\cwd071.txt"` in the non-ASCII folder | PASS | WMI shows the expanded command line; the file holds the folder (OEM CP852 as cmd writes it) |
| L14 Custom `C:\nowhere\shell.exe` | PASS | E2 names the path, quotes "(2) Systém nemůže nalézt uvedený soubor.", points to the setting; OK + Help buttons |
| L15 PowerShell 7 (not installed) | PASS | E1 "The selected command shell program (PowerShell 7) was not found …" |
| D01 combo items | PASS | Command Prompt / Windows PowerShell / PowerShell 7 (not found) / Windows Terminal / Git Bash / Custom program |
| D02 initial state | PASS | Windows Terminal selected, *Found at* = `…\WindowsApps\wt.exe`, Custom group disabled |
| D03 / D04 not-found preset | PASS | *Found at* "(not found on this computer)"; OK refused with the message, dialog stays |
| D05 pre-fill | PASS | Windows Terminal → Custom with empty fields: program = wt.exe alias, arguments `-d .`, fields enabled |
| D06 kept text | PASS | `X:\my own.exe` / `--flag` survive preset → Custom |
| D07 blank program | PASS | "Enter the program to run as the command shell." |
| D08 `$(Bogus)` | PASS | engine message "Variable "Bogus" was not found.", dialog stays |
| D09 Browse | PASS | "Select Command Shell Program" (wide) dialog opens |
| D10–D13 save / reopen / Cancel / persist | PASS | Custom values saved on OK, shown on reopen, Cancel keeps them, registry holds them after exit |
| D14 restart with persisted Custom | PASS | `cmd.exe` cwd = panel dir |
| CZ01 / CZ02 Czech | PASS | "Příkazový řádek / … / PowerShell 7 (nenalezeno) / Terminál Windows / … / Vlastní program"; message "Vybraný program nebyl na tomto počítači nalezen …" |

Screenshots: `verification/page-en.png`, `verification/page-en-custom.png`,
`verification/page-cs.png` (light theme, 100 % scale).

**No product defect was found by the GUI run.** Every failure along the way was
in the harness (`GetWindowText` does not read another process's edit controls;
`IDOK` posted to the tree sheet bypasses its own `_TPD_IDC_OK`; a PowerShell
parameter named `$args` is always empty; an Alt tap for focus opens the menu
bar; a `.ps1` without BOM mangles diacritics; `cmd` writes OEM). Two facts worth
knowing came out of it: volume E: on this PC has 8.3 names disabled (so the
long-path retry can only help where the volume provides them — exactly what the
contract says), and console presets open inside Windows Terminal because it is
the default terminal application on this Windows 11.

## Deviations from the plan

- GUI verification was run by an automated harness instead of a person; the
  items it cannot see are listed under T034 in the status table.
- The `.chm` is not compiled here (no HTML Help Workshop on this PC) — source
  files only, as quickstart §7 anticipates.
