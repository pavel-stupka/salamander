# Review — D3 · `CSalamanderGeneral::GetConversionTable` (`src/zip.cpp`)

**Reviewer**: independent agent; did not write the fix.
**Charter**: find a regression, not approve one.
**Diff reviewed**: `git diff -- src/zip.cpp` on branch
`075-fix-small-hardening` (base `c554f4d`) — one hunk, **10 insertions /
0 deletions**, diff md5 `d1390f1f53aecca0991ede4e5462d2df`.
**Protocol**: `contracts/fix-protocol.md` Parts B, C and D; spec FR-004,
FR-009; research R3; plan Design D3.

The change adds a `conversion == NULL` guard (5 lines) plus a 5-line comment
directly after the existing `table == NULL` guard. Nothing is deleted or moved.

---

## B1 · Consumers, re-enumerated independently

Own sweep over the whole tree, no path filter
(`rg "GetConversionTable" src/ --include=*.cpp --include=*.h --include=*.c`),
`src/plugins/` included:

| Site | Kind | Argument passed as `conversion` | Can it be NULL? |
|---|---|---|---|
| `src/zip.cpp:3301` | the definition | — | — |
| `src/plugins.h:2125` | core-side declaration of the implementation class | — | — |
| `src/plugins/shared/spl_gen.h:2201` | the abstract, plugin-facing declaration | — | — |
| `src/plugins/dbviewer/renmain.cpp:324` | plugin | `DefaultCoding` — fixed member array, guarded by `if (DefaultCoding[0] != 0)` | **no** |
| `src/plugins/dbviewer/renmain.cpp:441` | plugin | local `char conversion[205]` filled by `lstrcpy`/`lstrcat` | **no** |
| `src/plugins/dbviewer/renmain.cpp:474` | plugin (`SelectConversion`) | the parameter — but the function opens with `if (conversion == NULL) { … }` and the call is in the `else` | **no** |
| `src/plugins/filecomp/textio.cpp:560`, `:729` | plugin | `ASCII8InputEncTableName` — a fixed `char[…][101]` in the options struct (`filecomp.cpp:279–280` reads it from the registry into that array) | **no** |
| `src/plugins/unmime/parser.cpp:836` | plugin | local `char conversion[200]` (`parser.cpp:799`) | **no** |
| `src/plugins/demoplug/menu.cpp:307` | plugin, **inside a `/* … */` block** — dead sample code | local `char conversion[200]` | **no** |

Two remaining sites (`codetbl.cpp:809`, `:875`) are comments.

**No in-tree caller can pass NULL** — so for the shipped product the guard is
purely defensive, exactly as the spec states ("no shipped plugin passes NULL").
The service is nevertheless plugin-facing (`CSalamanderGeneralAbstract`), so
third-party or future plugins are the real consumer set, and for them:

* **before**: NULL survived the `table` guard, `CodeTables.Init` succeeded, and
  `CCodeTables::GetCodeType` reached its lookup loop, where
  `CodingNameEqual(n, c)` dereferences `*c` at `codetbl.cpp:782` → access
  violation **inside the core, on the plugin's thread**. (`SalU8ToW(NULL, …)`
  at `:817` returns 0 and is not the crash point — it merely skips the
  legacy-spelling branch, so the loop is reached with `coding == NULL`.)
* **after**: `FALSE`, one `TRACE_E`, `CodeTables` not touched at all (the guard
  is before `Init`).

That is a strict improvement, and it is the only behavioural difference.

## Documented contract — does the header need to change? **No.**

`src/plugins/shared/spl_gen.h:2194–2201` (Czech, verbatim):

> vraci konverzni tabulku 'table' (buffer min. 256 znaku) pro konverzi
> 'conversion' … **vraci TRUE pokud byla konverze nalezena (jinak neni obsah
> 'table' platny)**

The documented postcondition is a two-valued one: TRUE ⇒ found, FALSE ⇒ the
content of `table` is not valid. A NULL name returning FALSE satisfies it
literally — nothing in the comment promises anything for a NULL argument, and
no plugin can distinguish "not found" from "refused" today either (the existing
`table == NULL` guard already returns FALSE the same way). **No header edit is
needed, so C9 is not in play.**

`git diff --stat -- src/plugins/shared/` → **empty output** (verified for the
whole working tree, not just this file). `LAST_VERSION_OF_SALAMANDER` is still
`106` (`spl_vers.h:246`, file unmodified). Contract Part D is satisfied as
written.

## B2 · Shape and placement of the new guard

Post-image, `zip.cpp:3301–3327`:

```
CALL_STACK_MESSAGE2("CSalamanderGeneral::GetConversionTable(, , %s)", conversion);
if (table == NULL)      { TRACE_E("Invalid parametr (table==NULL) in …!");      return FALSE; }
if (conversion == NULL) { TRACE_E("Invalid parametr (conversion==NULL) in …!"); return FALSE; }
parent = (parent == NULL ? MainWindow->HWindow : parent);
…
```

Same `if (x == NULL)` form, same brace style, same `TRACE_E` sentence including
the house misspelling *parametr* (kept, per research R3, so both guards stay
findable with one `rg`), same `return FALSE`, placed **immediately after** the
twin — the A3 instruction was "the `table == NULL` block at `zip.cpp:3304–3308`"
and that is what was copied. No re-ordering of the existing guard.

## The `CALL_STACK_MESSAGE2` question — safe, and unchanged

It is still the **first** statement, before both guards; the diff does not
touch that line (10 insertions, 0 deletions). Whether it is *safe* with a NULL
`%s` matters only for the new NULL path, so I checked it rather than assuming:

* `callstk.h:199` — `CALL_STACK_MESSAGE2(p1, p2)` expands to
  `CCallStackMessage _m(FALSE, (__CallStk_T ? 0 : printf(p1, p2)), p1, p2)`;
  `__CallStk_T` is documented as *always TRUE*, so the `printf` is never
  executed (it exists only for compile-time format checking).
* `callstk.cpp:1019` — the constructor does `va_start` and calls
  `CCallStack::Push(format, args)`, which formats **eagerly** with
  `_vsnprintf_s(…, _TRUNCATE, format, args)` (`callstk.cpp:499`) inside a
  `__try/__except (EXCEPTION_EXECUTE_HANDLER)` that recovers by printing
  `"exception in: <format>"`.
* MSVC's `_vsnprintf_s` renders a NULL `%s` as `(null)`; it does not trip the
  invalid-parameter handler. Verified empirically rather than asserted — a
  probe compiled with this repo's toolchain (VS2022, `/MDd`, the debug CRT,
  which is the stricter one) using the exact format string of this call site:

  ```
  ret=50 buf=[CSalamanderGeneral::GetConversionTable(, , (null))]
  ```

  (scratch probe, not committed to the repo).

So the trace line handles NULL without faulting, it did so before the fix too,
and the twin `RemoveFilesFromCache` (`zip.cpp:3280–3285`) uses the identical
macro-then-guard order. **Nothing about it changed; leaving it first was
correct.**

## B4 · Byte identity for every non-NULL `conversion`

*"Any non-NULL name: identical result and table bytes; the shared header diff is
empty."*

The diff inserts one `if` whose condition is `conversion == NULL`. For every
non-NULL pointer that condition is false, control falls straight through to the
unchanged sequence `parent = …; Init; GetCodeType; GetCode; return ret;` —
those five lines are not in the diff (0 deletions confirms it textually). No
statement was reordered, no variable added, no early exit inserted on a
non-NULL path. `table`'s 256 bytes are written only by `CodeTables.GetCode`,
which is reached under exactly the same conditions as before. **Byte-identical
by construction**, including for the empty-string name `""` (not NULL → old
path → whatever the lookup did before, it still does).

The `parent == NULL` normalisation and the `MainWindow` dereference now happen
strictly *after* one more comparison; both are still reached on every non-NULL
call.

## B5 · Failure paths

| Path | Before | After |
|---|---|---|
| `table == NULL` | FALSE + trace | unchanged (guard untouched) |
| `conversion == NULL` | access violation in `CodingNameEqual` | FALSE + trace, tables untouched |
| `Init` fails (allocation) | FALSE, `GetCodeType`/`GetCode` skipped | unchanged |
| name not found | FALSE, `table` untouched | unchanged |
| `TRACE_E` compiled out (Release) | — | the guard still returns FALSE; the twin behaves the same |

## B6 · Buffers

No copy, no clamp, no conversion in the change. `CCodeTablesData::Name` is not
read by the added code, so C11 is untouched; the 256-byte `table` contract is
not altered.

## B7 · Earlier scenarios touched

None. The feature-069 legacy-spelling lookup inside `GetCodeType`
(`codetbl.cpp:800–825`) is untouched, so a plugin that stores a legacy-encoded
name and hands it back still resolves exactly as it did after 069. The
plugin-facing freeze (FR-009 / 069 cluster B-5) is respected: no argument
meaning, encoding or return value changed for any input a plugin sends today.

## B8 · Per-item path

None.

## C12 · Scope

The hunk is confined to `GetConversionTable`. The obvious neighbouring
temptations were **not** taken, correctly:

* `int codeType;` at `:3321` is still uninitialised (it is safe here — the
  `if (ret)` chain guarantees `GetCodeType` runs before `GetCode` — and it
  belongs to no item);
* `EnumConversionTables` (`:3289`), `GetWindowsCodePage` (`:3329`, which passes
  `codePage` to `GetWinCodePage` with no NULL check at all) and
  `RecognizeFileType` (`:3337`) are untouched.

Working tree confirms one file/one hunk for this item:
`git diff --numstat -- src/zip.cpp` → `10  0  src/zip.cpp`.

## Provenance note checked

`fix-log.md` records a `warning C4244` in `zip.cpp` after the change and proves
it pre-existing by a control build showing the same warning ten lines earlier —
exactly the ten lines this hunk adds above it. That reasoning is arithmetically
consistent with the diff I reviewed (10 insertions, 0 deletions, all above the
warning site at ~5786/5796). No new warning is attributable to this fix.

## Conclusion

A five-line guard copied from its twin two lines above, behind a condition that
is false for every argument any shipped caller can produce. Every in-tree
caller was re-enumerated and none can pass NULL; the documented contract in
`spl_gen.h` already covers a FALSE return, so the shared header stays byte-for-byte
unchanged and the interface version stays 106; the trace macro's position and
its NULL handling are unchanged and demonstrably safe. I could not construct an
input whose behaviour differs, other than the NULL that used to crash.

**VERDICT: ACCEPTED**
