# Review — D2 · viewer *Coding* menu default (`src/viewer3.cpp`)

**Reviewer**: independent agent; did not write the fix.
**Charter**: find a regression, not approve one.
**Diff reviewed**: `git diff -- src/viewer3.cpp` on branch
`075-fix-small-hardening` (base `c554f4d`) — **one hunk**, 8 insertions /
1 deletion, diff md5 `13507f04a3e10e55fd957f066d0ff9de`.
**Protocol**: `contracts/fix-protocol.md` Parts B and C; spec FR-003, FR-009;
research R2; plan Design D2.

The whole change is `int defCodeType;` → `int defCodeType = 0;` plus a seven-line
comment. Nothing else in the file is touched.

---

## Scope first — is `SetViewerCaption` (D4) in this diff? **No.**

`git diff -- src/viewer3.cpp` contains exactly one hunk, at `@@ -3297,7 +3297,14 @@`.
Lines 25–92 (`CViewerWindow::SetViewerCaption`, the D4 site) are untouched;
`git diff -- src/viewer3.cpp | grep -c '^@@'` = 1. The fix log's D4 record says
*pending — waits for D2's review and commit, because both edit `src/viewer3.cpp`*,
which matches. **No C12 scope violation.**

---

## B1 · `GetCodeType`'s out-parameter contract, re-verified independently

`CCodeTables::GetCodeType` (`src/codetbl.cpp:792–842`) read in full. It has
exactly **three** exits, and I found no fourth:

| Exit | Line | `codeType` |
|---|---|---|
| `!Loaded` | `:795–799` — `TRACE_E`, `return FALSE` | **not assigned** |
| name matched | `:835–836` — `codeType = i + 1; return TRUE` | assigned |
| loop exhausted | `:840–841` — `codeType = 0; return FALSE` | assigned |

Everything between `:800` and `:826` (the feature-069 legacy-spelling block:
`SalU8ToW` → `WideCharToMultiByte` → `strcmp`) is straight-line code with no
`return`, no `goto`, no exception path — `SalU8ToW` and `WideCharToMultiByte`
return status codes and the block only sets a local `codingAlt`. The `for` loop
at `:827` has one `return TRUE` inside it and falls through to `:840`.

**Verdict: the claim holds — `!Loaded` is the only non-assigning path.** The
fix is therefore complete for the defect as stated: no other reachable path can
leave `defCodeType` unset.

(One neighbouring, *unfixed* instance of the same shape, recorded here and
deliberately **not** asked for: `src/zip.cpp:3321` declares `int codeType;`
uninitialised in `GetConversionTable`. It is not a defect — `GetCodeType` there
is called only when `Init()` returned TRUE, i.e. `Loaded`, and `GetCode` is
guarded by the same `ret`. Under C12 it belongs to no item and must stay
untouched.)

## B1b · Is the comment's claim about `InitMenu` true?

`CCodeTables::InitMenu` (`codetbl.cpp:621–680`) does begin with
`if (!Loaded) { TRACE_E(…); return; }` at `:624–628`, before any
`InsertMenuItem`. So on the unloaded path it inserts nothing.

**Precise, and a small imprecision in the new comment.** At the moment
`SetMenuDefaultItem` runs (`viewer3.cpp:3309`) the menu is empty *on the first
`WM_INITMENU` only*. The `if (firstTime)` block at `:3312–3381` runs **after**
that call and appends six items regardless of `Loaded` — `CM_VIEWER_CODING_UTF8`,
`CM_RECOGNIZE_CODEPAGE`, two separators, `CM_SETDEFAULT_CODING`,
`CM_NEXTCODING`, `CM_PREVCODING`. On the second and later menu openings with
`!Loaded`, `InitMenu` returns early again but the submenu is **not** empty.

This does not weaken the fix. The ids are `6070`, `6071`, `6079`, `6080`,
`6101` (`src/resource.rh2:619–647`) and `CM_CODING_MIN` is `871`
(`resource.rh2:279`), so `CM_CODING_MIN + 0 = 871` matches **no** item that can
exist in that menu when the tables are not loaded: `SetMenuDefaultItem` fails
and leaves the default alone — which is the intended "defined state" of FR-003.
Before the fix, the same call received `871 + <indeterminate>` and *could* land
on one of `6070…6101` (any garbage in `5199…5230`), silently making *Next
coding* or *Set default coding* the bold item. The defect is real and the fix
removes it.

Suggested (non-blocking) wording correction: the comment says "InitMenu above
has then already returned with an empty menu"; accurate would be "…has returned
without inserting anything (the menu then holds at most this handler's own
appended commands, none of which uses an id in the `CM_CODING_*` range)".

## B1c · Is `Loaded == FALSE` really only an allocation failure?

`Loaded` is written in exactly two places (`rg "Loaded" src/codetbl.cpp src/codetbl.h`):
`:435` (constructor, `FALSE`) and `:614` (`Loaded = Table != NULL;` in `Init`).
Nothing ever sets it back to FALSE.

`CCodeTables::Init` (`:576–618`) read in full:

* configured path tried first; if the object exists but
  `GetState() != ctsSuccessfullyLoaded` → `findBest = TRUE`;
* `findBest` deletes it, preloads all conversion sets, picks the best
  (`GetBestPreloadedConversion`) and constructs again with the explicit comment
  `// do not check Table->State anymore -- accept anything` (`:610`);
* `if (Table == NULL) TRACE_E(LOW_MEMORY);` (`:612–613`).

So a **missing, empty or damaged `convert.cfg` still yields `Loaded == TRUE`** —
the fallback is real, as the spec claims. `Loaded == FALSE` needs
`new CCodeTable` to return NULL.

The second half of that claim — "the viewer cannot reach the menu without `Init`
having run" — also checks out: `CViewerWindow`'s construction calls
`CodeTables.Init(MainWindow->HWindow)` at `src/viewer.cpp:556`, long before any
`WM_INITMENU`. So the third theoretical source of `!Loaded` (never initialised)
is not reachable here either. **No data fixture exists; the disposition "no
runtime proof, static argument + review" in `fix-log.md` is the honest one.**

## B2 · Per-surface verdict

| Surface | Verdict |
|---|---|
| Loaded + stored default resolves | **unchanged** (see B4) |
| Loaded + stored default does not resolve | **unchanged** — still `0`, still *none* |
| Not loaded, first menu open | **corrected** — `871` instead of `871 + indeterminate` |
| Not loaded, later menu opens | **corrected** — same |
| `CodePageAutoSelect` branch (`:3295`) | **unchanged** — not in the diff |
| `InitMenu`, `GetCodeType`, `Valid`, `Next/Previous` | **unchanged** — not in the diff |

## B3 · Nothing refuted was changed

Research R2 explicitly rejected "skip `SetMenuDefaultItem` when `GetCodeType`
returns FALSE", because that would also skip the *loaded-but-not-found* case
where today *none* is highlighted (FR-009). The diff adds **no** conditional:
`CodeTables.GetCodeType(...)` is still called with its result discarded, and
`SetMenuDefaultItem(subMenu, CM_CODING_MIN + defCodeType, FALSE)` is still
executed unconditionally on every pass through the `else` branch. Confirmed
line by line against the post-image at `viewer3.cpp:3307–3309`. **The rejected
alternative was not slipped in.**

## B4 · Byte identity, argued (contract Part B4 row for D2)

*"With tables loaded: identical menu, identical default item (entry or none)."*

The only observable of the changed statement is the value of `defCodeType` at
`:3309`. With `Loaded == TRUE`, `GetCodeType` **writes** `defCodeType` on both
of its remaining exits (B1), and the write happens before any read. An
initialiser whose value is overwritten before it is read cannot change the
program's observable behaviour — for `DefaultConvert` resolving to entry *i*
the argument is `CM_CODING_MIN + i + 1` exactly as before, and for a
`DefaultConvert` that no longer names a live conversion it is `CM_CODING_MIN + 0`
exactly as before. The menu itself is built by `InitMenu`, which is not in the
diff. **Byte-identical on every loaded path.**

`DefaultConvert` itself is untouched (this site only reads it), so nothing is
persisted differently either.

## B5 · Failure paths

* `GetCodeType` returning FALSE with `Loaded` — unchanged (0).
* `GetCodeType` returning FALSE without `Loaded` — now a defined `0`; the
  subsequent `SetMenuDefaultItem` fails harmlessly (id `871` not in the menu)
  and its return value was never checked, before or after.
* `subMenu == NULL` — the whole block is inside `if (subMenu != NULL)`;
  unchanged.
* No new allocation, no new call, no new failure mode introduced.

## B6 · Buffers

None. No copy, no clamp, no WTF-8 boundary in this change; C11 (name bytes) is
untouched because this site never reads `CCodeTablesData::Name`.

## B7 · Earlier scenarios touched

None. Feature 015 (the UTF-8 coding item), feature 010 (`SalInsertMenuItemU8`)
and feature 069 F-P4-01 (the legacy-spelling lookup in `GetCodeType`) all live
in code this diff does not modify; the UTF-8 item is still appended in the
`firstTime` block and still checked at `:3384`.

## B8 · Per-item path

None — no new configuration value, no registry key, no per-plugin branch.

## C-invariants

* **C9** — no shared header touched: `git diff --stat -- src/plugins/shared/`
  is empty for the whole working tree; `LAST_VERSION_OF_SALAMANDER` is still
  `106` (`spl_vers.h:246`, unmodified).
* **C12** — one hunk, one item; `SetViewerCaption` (D4), `GetCodeType`'s
  contract and the neighbouring `int codeType;` in `zip.cpp` were all left
  alone, correctly.
* **C14** — no test added or removed by this diff.

## Attempts to break it that failed

1. *Could `defCodeType` be read anywhere else?* It is declared inside the
   `else` block and dies at `:3310`; one write, one read.
2. *Could the initialiser mask a loaded-path defect?* Only if some loaded path
   left it unassigned — B1 shows there is none.
3. *Could `0` be a worse argument than the old garbage in the unloaded case?*
   No: `871` provably matches no item that can be in that menu; garbage could.
4. *Does the `else` branch depend on `firstTime`?* No — `firstTime` is captured
   before `InitMenu` and used only after `SetMenuDefaultItem`; the diff does not
   move either.

## Conclusion

The change is a single initialiser. The defect it closes is real
(`GetCodeType`'s `!Loaded` path is the only one that does not assign, and the
viewer reaches `SetMenuDefaultItem` with that value), the loaded behaviour is
byte-identical by the overwrite-before-read argument, the refuted "make it
conditional" alternative was not taken, and no neighbouring site was touched.
The only remark is a wording imprecision in the new comment ("empty menu" is
true only on the first menu opening); it does not affect the code and is not a
reason to reject.

**VERDICT: ACCEPTED**
