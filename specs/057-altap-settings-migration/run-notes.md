# Implementation Run Notes — feature 057

**Date**: 2026-08-17

## Automated suite (T028)

`utils\test\run_migration_tests.cmd` — **98 checks passed, 0 failed** (final
run after all fixes). Scenario coverage: S1 selective transfer, S2 replace
semantics + backup/restore byte-roundtrip, S3 source-immutability wrapper on
every scenario, S4 cancellation/refusal paths (exit codes 2/3/11), S5
old-generation source with transforms T1+T2, S6 multiple sources, S7 re-run
idempotence (byte-identical exports), S8 skip transparency (exclude-lists,
never-copy set, NOTES flagging), S9 master-password rules (both FR-010
directions: strip + report when the destination has its own master password;
atomic verifier-pair copy when only the source uses one).

Notable defects caught and fixed during implementation:

1. The W2 destination-writability probe originally used `CreateSubKey`, i.e.
   wrote to the registry before the W6 confirmation (contract violation) —
   replaced by opening the deepest existing ancestor with write access.
2. `reg.exe` stderr redirected inside PowerShell 5.1 (`*> $null`) becomes a
   terminating error under `$ErrorActionPreference='Stop'` — all reg.exe
   invocations now run through `cmd /c` with cmd-level redirection.
3. Backup files could be overwritten by two runs within the same second —
   timestamp now gets a uniqueness suffix.
4. W7 progress counts for colors / view templates / general configuration
   reported raw copied-value counts (e.g. "81 entries" for a 3-entry
   category) — now recomputed with the same item rule the W4 checklist uses.
5. Harness: the source-immutability snapshot was taken before fixture import,
   comparing `$null` to real content — snapshot moved after imports.
6. `Get-RegValueSafe` returned REG_BINARY data through the PowerShell
   pipeline, which unrolls `byte[]` into `Object[]` — the master-password
   verifier then failed its type check and the FR-010 pair copy silently
   degraded to "verifier record missing". Fixed with the no-unroll comma
   (`return ,$v`); caught by the S9 scenario added precisely because FR-010
   had no automated coverage.

## Real-data smoke (T029, safe half)

Ran the wizard against the **real** `HKCU\Software\Altap\Altap Salamander
4.0` profile on the development machine (config version 103, 21 subkeys)
into a **scratch destination** (`TCMIG_DEST_ROOT` override — the real Tandem
Commander root untouched):

- exit 0; 9 categories offered with plausible counts (24 confirmations,
  10 view templates, 130 config values, 6 real FTP bookmarks, 13 plugin
  configs); hot paths and user menu correctly shown as empty.
- viewer/editor category correctly PARTIAL (real plugin-viewer rows skipped
  and named).
- "Not transferable" correctly listed the real profile's foreign plugins
  (Automation, CHECKVER, Encrypt & Decrypt, IEVIEWER, MMVIEWER, nethood,
  SplitCombine, UnARJ, UnMIME, UnRAR, WMOBILE), archiver settings, panel
  session state and histories.
- The real source exported byte-identical before and after the run.
- Scratch destination and temp artifacts deleted afterwards.

## Remaining manual step (T029, user half)

The release-gate UI verification requires migrating into the **real** Tandem
Commander configuration and checking the result in the running app (hot
paths menu, FTP connect dialog, master-password prompt), then exercising the
generated restore script — deliberately left to the user, since it replaces
their live Tandem Commander settings (the utility's backup/restore makes it
reversible). Procedure: quickstart.md, "Manual smoke".
