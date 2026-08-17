# Data Model: Altap Salamander Settings Migration Utility

**Feature**: 057-altap-settings-migration
**Phase**: 1 (design)

All state is transient (in-memory during one wizard run) except the three
output artifacts (backup, restore script, summary). There is no persistent
utility state — re-runs start from scratch by design (FR-009, US4).

## Entities

### SourceConfiguration

One discovered Altap/Servant Salamander per-user configuration.

| Field | Type | Rules |
|-------|------|-------|
| `RootPath` | registry path (HKCU-relative) | From the scan list in [contracts/category-mapping.md](contracts/category-mapping.md); MUST exist |
| `ProductName` | string | Display name derived from the root path (e.g. "Altap Salamander 4.0") |
| `Generation` | enum `Servant25x` \| `AS3x` \| `AS40` | Drives per-category compatibility verdicts |
| `ConfigVersion` | int (nullable) | Read from the source's version marker key if present; refines `Generation` |
| `LastWriteTime` | timestamp (nullable) | Root key last-write time; display only |
| `Access` | constant | **Read-only. No code path opens any source key with write access** (FR-006) |

Uniqueness: `RootPath`. Ordering: newest `Generation`/version first (W3
default = first).

### SettingCategory

One selectable unit of migration. The full closed list lives in
[contracts/category-mapping.md](contracts/category-mapping.md); this entity
describes its shape.

| Field | Type | Rules |
|-------|------|-------|
| `Id` | slug | Stable identifier (e.g. `hotpaths`, `ftp-bookmarks`) used by tests |
| `DisplayName` | string | Wizard label (e.g. "Directory hot paths") |
| `SourceSubtrees` | registry paths | Relative to source root; may be several (e.g. colors = 3 keys + 1 value) |
| `DestSubtrees` | registry paths | Relative to destination root; same cardinality after mapping |
| `Verdict` | enum per source `Generation` | `Verbatim` \| `Transform` \| `Skip(reason)` — fixed at design time in the mapping contract, not computed at runtime |
| `Transform` | named rule (nullable) | Required iff `Verdict = Transform`; defined in the mapping contract |
| `ItemCountRule` | rule | How W4 counts items (e.g. subkey count, populated-slot count) |
| `Presence` | enum, computed | `Present(count)` \| `Empty` \| `Absent` in the selected source |
| `Offered` | bool, computed | `Presence = Present` AND `Verdict ≠ Skip` |

State transitions (per run): `Offered` → user toggles → `Selected` →
(`Transferred` \| `Partial(skippedItems[])` \| `Failed(error)`). Categories
never move between runs — no memory of previous migrations (idempotence comes
from wholesale replacement, not bookkeeping).

### MigrationPlan

The confirmed work order produced by W4–W6.

| Field | Type | Rules |
|-------|------|-------|
| `Source` | SourceConfiguration | Exactly one |
| `Selected` | SettingCategory[] | ≥ 1 (zero selection exits, code 3) |
| `Confirmed` | bool | Writes begin only after explicit `y` (FR-012); backup precedes first write (FR-007) |

Validation: destination writable (W2), processes not running re-checked
post-confirmation (FR-008).

### MigrationBackup

| Field | Type | Rules |
|-------|------|-------|
| `RegFile` | file path | `reg.exe` export of the ENTIRE destination root, taken before the first write; export failure aborts the run (exit 12) with zero destination writes |
| `RestoreScript` | file path | Generated `.cmd`: deletes the current destination root, imports `RegFile`; double-click runnable; restores the exact pre-run state (SC-003) including the "root did not exist" case (then the script only deletes) |
| `Timestamp` | in filenames | One backup set per run; runs never overwrite a previous run's backup |

### MigrationSummary

| Field | Type | Rules |
|-------|------|-------|
| `PerCategory` | list | Every SELECTED category: `TRANSFERRED(n)` \| `PARTIAL(n of m, skipped items + reasons)` \| `SKIPPED(reason)` \| `FAILED(error)` — no silent drops (FR-011, SC-005) |
| `NotOffered` | list | Categories present-but-skipped or absent, with reasons |
| `Notes` | list | Master-password guidance (FR-010), paths pointing into the Altap installation (edge case), any flagged item |
| `Artifacts` | paths | Backup, restore script, and its own `.txt` path |

Written to console AND `.txt` identically (W8).

## Relationships

```
SourceConfiguration 1 ──< offered >── * SettingCategory (verdicts vary by Generation)
MigrationPlan 1 ── 1 SourceConfiguration
MigrationPlan 1 ──< selected >── * SettingCategory
MigrationPlan 1 ── 1 MigrationBackup (created before first write)
MigrationPlan 1 ── 1 MigrationSummary (always produced when writes were attempted)
```

## Invariants (from spec)

1. **Source immutability** (FR-006, SC-004): no source key is ever opened
   writable; verified by the test harness comparing a source export before
   and after every scenario.
2. **Backup-before-write** (FR-007): the destination `.reg` export exists and
   is non-empty (or the root verifiably did not exist) before the first
   destination write of a run.
3. **Category atomic replacement** (FR-009, Clarifications): transferring a
   category = delete the destination subtree(s), then write the mapped source
   content. Unselected categories' subtrees are never opened for writing.
4. **No cross-contamination** (FR-005): the destination's version marker key
   and any installation-metadata values (per the mapping contract's filter
   lists) are never written by any category transfer.
5. **Deterministic re-run** (SC-006): running twice with the same selection
   yields byte-identical destination subtrees for the selected categories.
