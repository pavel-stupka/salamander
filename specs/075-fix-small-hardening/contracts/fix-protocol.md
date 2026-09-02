# Contract — the per-fix protocol for feature 075

**Binding on every change in this feature.** This is feature 069's
`contracts/fix-protocol.md` applied unchanged, plus the parts that are specific
to these six sites. Where this file is silent, 069's Parts A (fixer), B
(reviewer) and C (standing invariants) apply verbatim —
`specs/069-finish-encoding-fixes/contracts/fix-protocol.md`.

The reason it is binding for a batch this small is in the 069 record: of four
review batches, two were rejected for regressions the fixes themselves
introduced, and none of those fixes looked risky either.

---

## Part A — Fixer's procedure, per item

**A0 · Still defective at HEAD?** Re-open the site at the commit you branch
from (research R0 holds the `640b94a` state; the tree may have moved). If the
site is fixed, record *verify-closed* with the evidence line and stop.

**A1 · Re-read the spec's FR for the item, not the handoff sentence.** The FRs
narrow the change (e.g. FR-003 forbids altering the loaded-not-found outcome;
FR-005 forbids touching a name of ≤ 259 bytes).

**A2 · Trace the chain and enumerate consumers yourself.** Research R1–R6
lists what was found at planning time; **produce your own `rg` list** and
classify each consumer per 069 A2 (strict-UTF-8 · legacy-ANSI-and-working ·
tolerant sink · plugin-facing). The planning list is a floor, not the answer.

**A3 · Fix minimally, in the house shape, copying the named twin:**

| Item | Twin to copy |
|---|---|
| D1 | `lstrcpyn` as used throughout `codetbl.cpp` |
| D3 | the `table == NULL` block at `zip.cpp:3304–3308` |
| D4 | `cmdshell.cpp:232–234` — clamp, then trim **only if** `strlen(src) >= clamp` |
| D5 | `lstrcpynA` at `controls.cpp:104`; the walk-back mirrors `salunicode.cpp:612–630` locally |
| D2, D6 | no twin — an initialiser; a flag |

**A4 · Never blank, never skip.** D4's legacy fallback (`viewer3.cpp:89`) and
D5's narrow-draw fallback (`controls.cpp:103–106`) stay exactly as they are. A
copy that does not fit yields a terminated prefix, never an empty string.

**A5 · Prove the check fails first.** For this feature every proof is a
recorded scenario (`quickstart.md` S1–S6): run it on the stashed tree, paste the
failure; unstash, run again, paste the pass. No `saltests` check is added — none
of the sites is reachable from the test program — and this is stated in the
record rather than worked around.

**A6 · Fill the fix record** (`data-model.md` §1) in `fix-log.md`.

**A7 · Hand the diff and the record to a reviewer who did not write it.**

## Part B — Reviewer's checklist (unchanged from 069)

B1 consumers re-enumerated independently · B2 per-surface verdict
(unchanged / corrected / regressed) · B3 nothing refuted was changed · B4 byte
identity argued per site, not asserted · B5 failure paths · B6 buffers
(3-byte WTF-8 units, clamps on boundaries) · B7 earlier scenarios touched ·
B8 per-item path (answer: none) · B9 verdict ACCEPTED / REJECTED →
`findings/review-D<n>.md`.

**B4 for this feature, site by site** — the reviewer must be able to write
each of these from the diff alone:

| Item | The identity that must hold |
|---|---|
| D1 | for `strlen(name) < bufferLen`: same bytes, same TRUE; for `strlen(name) > bufferLen`: same `bufferLen-1` bytes + NUL, same FALSE; `strlen(name) == bufferLen`: **intended change** (no overflow, FALSE); `Name` bytes untouched |
| D2 | with tables loaded: identical menu, identical default item (entry or *none*) |
| D3 | any non-NULL name: identical result and table bytes; the shared header diff is empty |
| D4 | any source ≤ 259 bytes: `caption` byte-identical; the encoding suffix and the *Viewer* word unchanged; the fallback `SetWindowTextA` still reached on decode failure |
| D5 | any text < 260 bytes: `Text` byte-identical, `TextLen` identical; the paint path untouched |
| D6 | with Node 22+: identical verdict and identical harness output; a deliberately broken worker still fails |

## Part C — Standing invariants that apply here

From 069 Part C: **C1** (core is ANSI-built), **C2** (WTF-8 internal names;
`SalU8ToW` strict), **C4** (`LoadStr` ANSI / `LoadStrU8` UTF-8), **C5** (tolerant
sinks' fallback is load-bearing — D4, D5), **C7** (`/J`: `char` is unsigned —
the walk-back tests `& 0xC0` on `unsigned char` anyway), **C9** (interface 106,
shared headers unchanged — D3, D5), **C10** (the clusters B-1…B-5 are out of
scope; a plugin caption's encoding is B-5 and is *why* D4 is guarded, not a
reason to fix it).

Added for this feature:

| # | Invariant |
|---|---|
| C11 | `CCodeTablesData::Name` bytes are never changed, re-encoded or trimmed (069 F-P4-01); D1 copies, nothing more. |
| C12 | One commit touches one item. A diff that reaches a neighbouring site — `CFilecompThread`'s `strcpy`s, the nine `IDS_VIEWERTITLE` sites, `GetCodeType`'s contract — is REJECTED regardless of merit; the neighbour goes to the handoff. |
| C13 | Nothing under `src/plugins/codeview/web/` changes for D6; the data harness (`check_data.py`) output is identical before and after. |
| C14 | No `saltests` count change in either direction. |

## Part D — Plugin-facing surface note (D3)

`CSalamanderGeneralAbstract::GetConversionTable(HWND parent, char* table, const char* conversion)`
— documented (`spl_gen.h:2194–2201`): returns TRUE when the conversion was
found, otherwise `table` is not valid. After D3, `conversion == NULL` returns
FALSE with a `TRACE_E`, which is inside the documented contract; the header is
**not** edited (C9). A future header pass may add the sentence "a NULL
`conversion` returns FALSE" as a comment-only change.
