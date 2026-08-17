# Quickstart: Validating the Migration Utility

**Feature**: 057-altap-settings-migration
**Deliverable under test**: `utils/migrate-altap-settings.cmd`

## Prerequisites

- Windows 11 (stock — no extra runtimes; Windows PowerShell 5.1 is in-box).
- Repository checked out (fixtures + harness live in `utils/test/`).
- Tandem Commander and Altap Salamander **not running** (or use
  `TCMIG_SKIP_PROCCHECK=1` for harness runs).
- All automated scenarios run against **scratch registry roots** via the
  test-only overrides (`TCMIG_SOURCE_ROOT`, `TCMIG_DEST_ROOT`,
  `TCMIG_OUT_DIR` — see [contracts/wizard-flow.md](contracts/wizard-flow.md));
  the real `HKCU\Software\Tandem Commander\0.1` is never touched by the
  harness.

## Automated suite

```bat
utils\test\run_migration_tests.cmd
```

Expected outcome: every scenario below reports `PASS`; the harness exits 0.
The harness: imports a fixture `.reg` under `HKCU\Software\TCMigTest\`,
runs the wizard with redirected stdin feeding the scenario's answers, then
asserts on registry state, exit code, and summary content, and finally
deletes the scratch keys.

## Scenarios

### S1 — Selective transfer (User Story 1 / SC-001, SC-002)

Fixture: `altap40-full.reg` (AS 4.0 source, all categories populated) +
empty destination. Input: select only hot paths + FTP bookmarks → confirm.

Assert: exit 0; destination hot-paths and FTP subtrees match the fixture's
mapped content exactly; **no other destination subtree exists**; summary
lists both categories `TRANSFERRED` with correct counts.

### S2 — Replace semantics + backup/restore (User Story 2, Q1 clarification / SC-003)

Fixture: `altap40-full.reg` + `tc-preexisting.reg` (destination already has
user-created hot paths and one FTP bookmark). Input: select hot paths only →
confirm.

Assert: destination hot paths = source's (pre-existing ones gone — replaced
wholesale); FTP subtree untouched (still the pre-existing bookmark); backup
`.reg` + `restore.cmd` exist in `TCMIG_OUT_DIR`. Then run `restore.cmd`:
destination must be byte-identical to the pre-run export (harness compares
`reg export` outputs).

### S3 — Source immutability (FR-006 / SC-004)

Wrapped around every scenario: harness exports the source subtree before and
after each run and compares byte-for-byte. Any difference = FAIL.

### S4 — Cancellation & refusal paths (FR-008, FR-012)

1. Feed `n` at the confirmation → exit 3, destination unchanged, no backup
   files created after W5's announcement is acceptable only if no write
   occurred (assert destination export unchanged).
2. Zero categories selected (`N`, `D`) → exit 3, unchanged.
3. With `TCMIG_SKIP_PROCCHECK` unset and a running `tandemcommander.exe`
   (manual scenario, see below) → exit 10 before any prompt past W2.
4. `TCMIG_SOURCE_ROOT` pointing at a non-existent key → exit 11.

### S5 — Old/minimal source (User Story 3 edge, best-effort rule)

Fixture: `altap25-minimal.reg` (ancient-generation source: hot paths only,
no FTP, no colors). Assert: W4 offers only the categories present; absent
ones appear under "Not transferable / empty" with reasons; transfer of hot
paths succeeds; summary contains no invented categories.

### S6 — Multiple sources (User Story 3)

Fixture: import BOTH `altap40-full.reg` and `altap25-minimal.reg` under the
scratch source area (harness points the scan override at the common parent).
Assert: W3 lists both, newest preselected; choosing `2` migrates the old
one's content (verify a value unique to the old fixture).

### S7 — Re-run idempotence (User Story 4 / SC-006)

Run S1, then run again selecting the same categories. Assert: second run's
destination subtrees byte-identical to the first (export compare); no
duplicated items; exit 0 both times.

### S8 — Skipped-item transparency (FR-005, FR-011 / SC-005)

Fixture: `altap40-full.reg` includes deliberately non-transferable material
(per [contracts/category-mapping.md](contracts/category-mapping.md): source
version markers, an entry for a plugin Tandem Commander does not ship, a
user-menu command pointing into the Altap installation directory). Assert:
version markers and foreign-plugin config absent from destination; the
summary names every skipped item with a reason; the Altap-path entry is
transferred but listed under `NOTES`.

## Manual smoke (release-gate style, real machine)

1. On a machine with a real Altap Salamander profile: close both apps,
   double-click `migrate-altap-settings.cmd`, migrate hot paths + FTP
   bookmarks into a real Tandem Commander install.
2. Start Tandem Commander: hot paths appear in the Hot Paths menu/bar; FTP
   bookmarks appear in the FTP connect dialog; a master-password-protected
   FTP password prompts for (and accepts) the same master password (FR-010).
3. Run the generated `restore.cmd`; start Tandem Commander again: previous
   state is back.
4. Verify Altap Salamander still starts with unchanged settings.

## Where things are

| Artifact | Location |
|----------|----------|
| Utility | `utils/migrate-altap-settings.cmd` |
| Harness + fixtures | `utils/test/` |
| Category/transform rules | [contracts/category-mapping.md](contracts/category-mapping.md) |
| Wizard I/O + exit codes | [contracts/wizard-flow.md](contracts/wizard-flow.md) |
| Entities & invariants | [data-model.md](data-model.md) |
