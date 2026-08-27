# codeview generators (feature 070)

Developer-side tooling for the Code Viewer plugin. **The product build never
runs any of this** — it consumes the committed output under
`src/plugins/codeview/`. Regeneration is a deliberate act whose diff is
reviewed like source (spec FR-008).

## What is generated

| Output (committed) | By | What it is |
|---|---|---|
| `src/plugins/codeview/web/shiki/` | `build_web.py` | the highlighting engine bundle, 224 language modules, 12 themes |
| `src/plugins/codeview/web/assets.rc2` | `build_web.py` | the `RCDATA` block that embeds every asset in `codeview.spl` |
| `src/plugins/codeview/web/assets_table.inc` | `build_web.py` | URL → resource id + MIME table used by `webglue.cpp` |
| `src/plugins/codeview/web/licence-manifest.json` | `build_web.py` | per-asset licence, source and exclusion record |
| `src/plugins/codeview/web/shipped-languages.json` | `build_web.py` | the grammars that survived the audit (input to the next step) |
| `src/plugins/codeview/langmap.{h,cpp}` | `gen_langmap.py` | name→language tables and the Viewers mask rows |
| `src/plugins/codeview/test/langmap-manifest.json` | `gen_langmap.py` | the same data for the harness |

Hand-written and **not** generated: `web/viewer.html`, `web/viewer.css`,
`web/viewer.js`, `web/worker.js`.

## Inputs, all pinned or committed

- `pins.json` — npm package versions and the Linguist revision. Changing a
  version here is the only way the generated data moves.
- `resolved-licences.json` — manual licence resolutions for grammars whose
  package metadata names none, each with the URL the licence text was read
  from (verified 2026-08-26).
- `overlay-editor.json`, `overlay-windows.json` — corrections and additions on
  top of Linguist: editor conventions, and the Windows/dev formats Linguist
  does not know (`.rc`, `.iss`, `.sln`, `.reg`, …).
- `ambiguity.json` — the FR-006 disambiguation table (`.h`, `.m`, `.ts`, …).
  Each rule's probe is implemented in `src/plugins/codeview/intake.cpp`.

## Running them

Requires Node and Python 3.13 and network access (npm registry).

```bash
# 1. assets + licence audit  (a few minutes; downloads the pinned packages)
python tools/codeview/build_web.py --work /path/to/workdir --keep

# 2. language map + masks     (reuses the same work dir's node_modules)
python tools/codeview/gen_langmap.py --work /path/to/workdir

# 3. verify the result
python src/plugins/codeview/test/check_data.py
```

`build_web.py` **fails** rather than shipping anything it cannot licence:
an asset whose licence is GPL-3.0-only, unknown, or absent from
`resolved-licences.json` is excluded, and if that leaves fewer than
`minimum_grammars` (200, from SC-001) the run aborts.

## Why the output is committed

The product build must stay offline and reproducible (constitution I). The
generated files are therefore part of the source tree, and regenerating from
the same pinned inputs must produce them byte for byte — that property is what
`check_data.py` and code review rely on.
