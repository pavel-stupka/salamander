# Data Model — Small hardening batch (075)

No persisted state is created or changed by this feature: no registry value, no
configuration field, no file format. The "data" of the feature is its **record**
— the per-defect entries the protocol requires — and the four values the fixed
code handles.

## 1. Defect item

One row per D1–D6, kept in `fix-log.md`.

| Field | Meaning | Source |
|---|---|---|
| `id` | D1 … D6 | spec inventory |
| `site` | `file:line` at HEAD, re-checked at implementation time (protocol A0) | research R0 |
| `class` | out-of-bounds write · unset read · missing argument check · torn text · unbounded copy · environment-dependent verdict | spec inventory |
| `status_at_head` | present / changed / already fixed | research R0 |
| `consumers` | every reader/writer of the changed symbol, found by own `rg`, each classified (protocol A2) | research R1–R6, re-derived by the fixer |
| `change` | the diff in one sentence + the byte-identity argument | plan Design |
| `not_touched` | neighbours seen and deliberately left (with reason) | research R5, plan "Explicitly not changed" |
| `proof_before` | the mechanical failure, pasted (command, output) | quickstart S<n> |
| `proof_after` | the mechanical pass, pasted | quickstart S<n> |
| `per_item_path` | `no` for all six (recorded, so the reviewer's B8 is answered) | research |
| `review` | `findings/review-D<n>.md` + verdict ACCEPTED / REJECTED | reviewer |
| `changelog` | the entry text, or `hygiene — no entry` | research R9 |
| `disposition` | **fixed and accepted** · verify-closed (D6 only, if chosen) | FR-001 |

### Lifecycle

```
recorded (069 §3 / 074 fix-log)
   └─► confirmed-at-HEAD ──► fixed ──► proven (before fails, after passes)
                                          └─► reviewed ──► ACCEPTED ──► closed
                                                     └──► REJECTED ──► reworked (back to fixed)
   └─► verify-closed (D6 only; evidence: the Node floor written into the runner header)
```

A row may not skip a state; "closed" without `proof_before` or without a review
file is a defect of the record (protocol B1).

## 2. Values the fixed code handles

### Conversion name (D1, D2, D3)

- One entry of `convert\<set>\convert.cfg`, `name=files`; bytes are the file's
  own (legacy code page for the Central-European names), **plugin-facing**
  (`EnumConversionTables`, `GetConversionTable`) and persisted by dbviewer and
  filecomp — never re-encoded (069 F-P4-01).
- Length: unbounded (`DupStr` of the line); longest shipped: 33 bytes.
- Lookup by name → index: `GetCodeType`, contract "found ⇒ index ≥ 1, not
  found ⇒ 0 + FALSE, tables unloaded ⇒ FALSE and the out-parameter untouched"
  (the last case is what D2 must survive).
- Name by index: `GetCodeName(index, buffer, bufferLen)`, contract after D1:
  "always terminated, never past `buffer[bufferLen-1]`; TRUE iff the whole name
  fit (`strlen(name) < bufferLen`)".

### Viewer title (D4)

- Composition: `<name prefix ≤ 259 B>` + `" - "` + `LoadStrU8(IDS_VIEWERTITLE)`
  + `" - [<coding>]"`; UTF-8 throughout after feature 069.
- Invariant after D4: the prefix never ends inside a multi-byte sequence when
  the source was longer than the clamp; a source of ≤ 259 bytes is copied
  byte-for-byte.
- Sink: `SetWindowTextW` via strict `SalU8ToWAlloc`; fallback `SetWindowTextA`
  with the original bytes (never blanked).

### Comparator header text (D5)

- `CFileHeaderWindow::Text[MAX_PATH]`, UTF-8 path (interface 104), painted
  through `SplU8ToW` → `DrawTextW`, fallback `DrawTextA`.
- Invariant after D5: `strlen(Text) ≤ MAX_PATH-1`; no torn tail when
  truncated; `TextLen == strlen(Text)`.

### Test verdict (D6)

- `run_tests.cmd` exit code and last line: `RESULT: all codeview checks passed`
  ⇔ all three harnesses exited 0; `RESULT: FAILURES` otherwise.
- Invariant after D6: the verdict is a function of the sources only, for any
  Node ≥ 20.10.

## 3. Validation rules carried into tasks

- Every code diff hunk maps to exactly one `id` (FR-001).
- `proof_before` and `proof_after` are literal tool output, not prose.
- `consumers` lists are produced by the fixer *and* independently by the
  reviewer; a consumer present in one and absent in the other blocks ACCEPTED.
- `changelog` for D1/D3/D5 is the single shared hardening line; for D2 and D6
  it is `hygiene — no entry`.
