# Contract: Claimed Types — Registration, Generation, Upgrade

**Status**: binding. Implements spec FR-008…FR-012 and research D10/D14.

## 1. Generation (dev-side, committed outputs)

- Source of truth: GitHub Linguist at a **pinned revision** (`languages.yml`
  + `heuristics.yml`) joined to the shipped grammar set, plus two committed
  overlays: `overlay-editor.json` (VS Code conventions: firstLine, extra
  extensions) and `overlay-windows.json` (`.rc/.rc2/.rh/.dlg`, `.iss/.isl`,
  `.sln/.props/.targets/.vcxproj`, `.reg`, `.inf`, `.slt`→ini, …).
- `tools/codeview/gen_langmap.py` is deterministic: same pinned inputs ⇒
  byte-identical `langmap.cpp`, mask rows, and `langmap-manifest.json`
  (FR-008; regeneration diff = review artifact).
- A language whose grammar is excluded (licence, D3) or absent is emitted
  with `grammarChunk = none` → opens plain with its name shown (FR-003);
  it is **still claimed** if it is a source/config format.

## 2. What is claimed / never claimed

- Claimed: the generated source + config + structured-text set (~780 masks,
  ~225 languages; family groups per data-model §3), plus `*.txt;*.log` as
  the `PLAINTEXT` family (clarification #5).
- **Never claimed**: `*.md;*.markdown` (mdview), `*.csv;*.dbf;*.tsv`
  (dbviewer), pictview's registered masks (incl. `.pyx`, `.st`, `.dtx` —
  conceded in v1), `*.*`. Enforced by a generated-time and test-time
  intersection check against the other shipped plugins' `AddViewer` strings
  (FR-010); the check fails the harness, not just warns.

## 3. Registration mechanics (`Connect`)

- 8 **families**, each registered as one or more `AddViewer("<masks>", FALSE)`
  rows of ≤ 200 bytes. The hard product cap is 259 bytes: `LoadViewers` reads
  the stored value into `char[MAX_PATH]` and `break`s on failure, so one
  over-long row discards itself **and every row below it**
  (`src/salamdr2.cpp:2708-2745`). At the shipped coverage this is ~48 rows;
  the house precedent is the picture viewer's 11
  (`src/plugins/pictview/pictview.cpp:1037-1047`). The generator enforces the
  cap and emits rows family by family so a family's rows stay contiguous.
- Rows insert at index 0 ⇒ register in reverse priority order (the family
  that must win ties goes last). `PLAINTEXT` registers **first** (lands
  lowest of the plugin's rows).
- Installation-only semantics (`force=FALSE`): rows are added once, on the
  load where the plugin gains viewer function; the user's later edits,
  deletions and reordering are permanent (FR-011 for free).

## 4. Upgrade protocol (post-v1 mask changes)

- `CURRENT_CONFIG_VERSION` starts at 1. A later mask change ships as:
  `if (ConfigVersion < N) { ForceRemoveViewer(oldMask); AddViewer(newRow, TRUE); }`
  — additive, deduplicated by the core against existing rows; never
  re-issues rows the user deleted (the force path adds masks only if not
  present, and only the delta version adds them once).
- "Restore default file types" (configuration dialog) re-issues all current
  default rows via the force path and touches nothing else (FR-012).

## 5. Runtime decline (completes the claim policy)

`CanViewFile` returns FALSE (next viewer in the user's list opens — by
default the built-in viewer) for: binary content per the sniff rule
(data-model §5 `intakeResult`), size > viewer limit, unreadable file,
unpaired-surrogate names (feature 066), engine unavailable is handled in
`ViewFile` via the internal-viewer fallback (mdview pattern). Sniff budget:
first 8 KB, ≤ 50 ms (FR-027).

## 6. Tests

- Mask/row invariants (§2, §3 caps) — harness.
- Upgrade scenario: seeded registry with user-modified list → simulated
  upgrade → list intact; deleted plugin row stays deleted (SC-007).
- First-open hint (FR-D12 → spec US4): first view shows the dismissable
  Alt+F3 / Options ▸ Viewers hint once.
- Decline matrix: 2 GB `.sql`, `.ts` MPEG, `.h` WinHelp, lone-surrogate
  name → built-in viewer, < 1 s, no dialog (SC-006).
