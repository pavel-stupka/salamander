# Contract — the per-fix protocol

**Feature**: 069-finish-encoding-fixes · **Binding on every change in this
feature** · derived from spec FR-001–FR-010 and the 068 charter
(`specs/068-encoding-regression-review/charters.md`, "Regression reviewer")

This is the interface between the fixer and the reviewer. A fix that skips a
step is not "mostly done" — it is rejected, because the three regressions this
protocol caught in feature 068 all looked complete to their authors.

---

## Part A — The fixer's procedure

**A0 · Still defective at HEAD?**
Open every `file:line` the finding names and confirm the defect is present.
Three items of the handoff were already fixed by X01–X09 (research.md R1). If
the site is fixed: record **verify-closed** with the evidence line and stop —
no code change.

**A1 · Re-read the verdict, not the finding.**
`specs/068-encoding-regression-review/findings/verdicts-V*.md`. The verifiers
narrowed or corrected most claims. Write down, in the fix record:
- the **confirmed** part (this is what may be changed),
- the **refuted** part (this may **not** be changed — a change here is a
  regression by definition, because the current behaviour is correct),
- the **latent** part (changeable only as a provable no-op for all eight
  shipped languages and shipped configurations; say why it is a no-op).

**A2 · Trace the whole chain, producer to sink.**
For every value the fix touches: the producer (what API or resource it comes
from, therefore its encoding), every intermediate hop, the sink. Then
enumerate **every other consumer** of that value, buffer, global or window —
found by your own `rg`, not from the finding — and classify each:

| Class | Meaning | What the fix owes it |
|---|---|---|
| strict-UTF-8 | a facade / `Sal*` helper that rejects non-UTF-8 | it is the reason the defect is visible; it must receive UTF-8 after the fix |
| legacy-ANSI-and-working | a consistent ANSI chain that works today (plugin `.spl` loading, `lang\*.slg`, `convert\` tables, `plugins.ver`) | it must keep receiving what it receives today, or be converted in the same commit |
| tolerant sink | `Sal*U8` sink / `CStaticText` / `CMessageBox` — tries wide, falls back to the A call | it renders ACP bytes correctly today; do not assume it was broken |
| plugin-facing | anything a plugin sends or receives (`src/zip.cpp` forwarders, shared headers) | bytes must be identical (FR-005); if they cannot be, the item is deferred (FR-012) |

A fix that converts one hop while leaving an adjacent hop on the legacy code
page is the **DC-09** trap — both 068 rejections were this.

**A3 · Fix minimally, in the house shape** (research.md R3 S1–S5). Use the
existing helpers; do not invent a parallel mechanism; do not touch adjacent
code. Where a twin already exists in the tree (`fileswn8.cpp`'s
`SHFileOperationW`, `dialogs3.cpp`'s `ExtractIconExW`, `plugins2.cpp:1049`'s
`SalListViewSetItemTextU8`), copy that shape verbatim.

**A4 · Never blank, never skip.** On conversion failure the code must do what
the pre-fix code did — the narrow call with the original bytes. Blanking text
or skipping an operation is a regression, not a fix (FR-004). Buffer sizing
follows the same rule: accented text grows up to 3× in UTF-8, so every
fixed-size buffer on the path is re-checked, and a clamp cuts only on a
character boundary.

**A5 · Write the check and prove it fails first** (FR-008):
- unit test in `src/saltests/saltests.cpp` when the logic is reachable from the
  test exe (it links only `src/common/salclip.cpp`, `salfileio.cpp`,
  `salpath.cpp`, `salunicode.cpp`);
- else a `tools/check_encoding.py` rule, when the defect has a grep-able shape;
- else a written manual scenario in `quickstart.md`.
Proof is mechanical and pasted into the record: `git stash` → run → it
fails/fires → `git stash pop` → run → it passes/is clean.

**A6 · Fill the fix record** (`data-model.md` "Fix record"): items, chain,
change, not-touched, affected surfaces, byte-identity argument, timing if a
per-item path, check with its proof, changelog text or "hygiene — no entry".

**A7 · Hand the diff and the record to a reviewer who did not write the fix.**

---

## Part B — The reviewer's checklist (charter: find a regression)

The reviewer works from the diff and the record, read-only, and must answer
every line. "Looks fine" is not a verdict.

1. **Consumers, re-enumerated independently.** `rg` every changed symbol,
   resource id, control id and window yourself. Any consumer missing from the
   record is a defect of the record — say so.
2. **Per-surface verdict**: **unchanged** (prove the bytes/behaviour are the
   same for every input that worked before — ASCII, English UI, valid-ACP
   plugin input, the error paths), **corrected** (the verdict's scenario now
   renders/behaves right), **regressed** (any input that worked before now
   differs).
3. **The refuted list.** Confirm the fix changed nothing the verifier refuted.
4. **Byte identity.** English UI / ASCII identical (ASCII `LoadStrU8 ==
   LoadStr`; a W call on ASCII == the A call — argue per site, do not assert);
   plugin-facing identical (`src/plugins/shared/` diff comment-only,
   `LAST_VERSION_OF_SALAMANDER` unchanged, no forwarder behaviour change).
5. **Failure paths.** Conversion failure, allocation failure, `free()` on every
   path, HANDLES bookkeeping (`SalFindFirstFile` registers its own handle —
   never wrapped in `HANDLES_Q`, closed with `HANDLES(FindClose(h))`), thread
   context.
6. **Buffers.** Every fixed-size buffer on the path re-checked against 3-byte
   WTF-8 units; no new truncation; clamps on character boundaries; documented
   `MAX_PATH` limits kept where they are deliberate.
7. **Earlier scenarios.** Which 058/062/063/066/067/068 quickstart scenarios
   touch the changed code, and can this change alter them?
8. **Per-item path?** If the changed code runs once per listed item, the record
   must carry the before/after timing; flag it if missing.
9. **Verdict**: **ACCEPTED** (no regressed surface, record complete) or
   **REJECTED** (name the regressed surface or the missing evidence).
   Output: `findings/regression-X<nn>.md`.

---

## Part C — Standing invariants (any fix violating one is rejected)

| # | Invariant | Source |
|---|---|---|
| C1 | The core is built without `UNICODE`: an un-suffixed Win32 text API is the ANSI one. `HANDLES(CreateFile(…))` is still an ANSI call. | 068 R6 |
| C2 | Internal names and paths are **WTF-8**: `SalWToU8` is total; `SalU8ToW` is a strict WTF-8 decoder (0/NULL on malformed **and** on a too-small buffer — the two are indistinguishable); `SalU8ToWDisplay*` is lenient and **display-only**. | feature 066 contract |
| C3 | The plugin-shared `Spl*` helpers are strict UTF-8 by design and reject WTF-8 — that is not a bug to fix here. | feature 066 boundary notes |
| C4 | `LoadStr` is ANSI (`LoadStringA`, substitutes `?` outside the ACP); `LoadStrU8` is UTF-8. For ASCII text they are byte-identical. | `src/salamdr2.cpp` |
| C5 | The `Sal*U8` sinks and `CStaticText` fall back to the narrow call — they render ACP bytes correctly today. Their fallback is load-bearing. | `src/common/winlib.cpp` |
| C6 | `src/consts.h` comments about the facade functions are stale; the implementation in `src/common/salfileio.cpp` is ground truth. | 068 R7 |
| C7 | The product compiles with `/J`: plain `char` is unsigned. Any argument resting on a signed `char` is void. | `sal_base.props` |
| C8 | No registry migration: a MINORB release must not move configuration. | Constitution |
| C9 | Plugin interface version stays 106; shared headers change in comments only. | spec FR-005 |
| C10 | Out of scope, and a neighbour being in scope does not pull it in: clusters B-1 (ANSI dialog windows, incl. the complete Unicode command line), B-2 (code-page byte tables behind name comparison), B-3 (`GetErrorText` — **any composed message whose other half is `GetErrorText`**), B-4 (`AlterFileName`), B-5 (plugin-facing ANSI services). | spec Scope, research.md R9 |
