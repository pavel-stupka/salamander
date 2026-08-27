# Data Model — Source & Configuration File Viewer (codeview)

**Date**: 2026-08-26 · Entities from `spec.md` § Key Entities, refined by the
Phase 0 decisions (D2–D16). No database — state lives in generated C++
tables, embedded resources, the registry, and per-window memory.

## 1. Language definition (generated, committed — D10)

One row per identifiable format (~225 with grammar + named no-grammar
formats).

| Field | Type | Notes |
|---|---|---|
| `id` | stable ASCII slug | Shiki grammar name where one exists (`cpp`, `yaml`); Linguist-derived slug otherwise (`vs-solution`) |
| `displayName` | UTF-8 | shown in status bar & language picker (English; not translated — proper names) |
| `grammarChunk` | resource name or **none** | `none` ⇒ plain-text rendering (FR-003); build check: named chunk must exist (FR-008) |
| `aliases` | list | Shiki aliases + Linguist aliases (picker search) |

**Validation**: every `grammarChunk` resolves to an embedded resource;
licence manifest row exists for every chunk (D3).

## 2. Name→language mapping (generated, committed — D10)

Ordered per FR-005; first hit wins, ties fall to heuristics.

| Table | Key | Value | Example |
|---|---|---|---|
| `exactNames` | lower-cased file name | language id | `dockerfile → docker`, `cmakelists.txt → cmake` |
| `patterns` | file-name pattern | language id | `dockerfile.* → docker`, `.env.* → dotenv` |
| `multiDotSuffixes` | longest-first suffix | language id | `.d.ts → typescript`, `.blade.php → blade` |
| `extensions` | extension | language id **or** ambiguity-rule id | `.cpp → cpp`, `.h → RULE_H` |
| `heuristics` | rule id | ordered content probes → language id | `RULE_H`: `@interface|@implementation` → objective-c, else cpp |
| `shebangs` | interpreter name | language id | `python3 → python` (env-form + version-suffix normalised) |
| `modelines` | ft/mode name | language id | `ft=yaml → yaml` (first/last 5 lines) |
| `signatures` | first-bytes probe | language id or **binary** | `<?xml → xml`; `0x47`@188-stride → binary (MPEG-TS) |

**State**: immutable at runtime; per-view override (FR-007) shadows the
result without persisting.

## 3. Claimed-type registry (generated, committed — D14)

| Field | Type | Notes |
|---|---|---|
| `familyId` | enum (≤ 8) | e.g. `CODE_CORE`, `SCRIPTS`, `DATA_CONFIG`, `XML_BASED`, `WEB`, `BUILD_CI`, `DOCS_ADJACENT`, `PLAINTEXT` (`*.txt;*.log` — clarification #5) |
| `maskRow` | string ≤ 200 bytes | one `AddViewer` call per family; `;`-separated masks; hard cap 259 (D14) |
| `order` | int | registration order — last registered lands highest in Options ▸ Viewers |

**Invariants** (tested): union of rows = generated claim list; intersection
with other shipped plugins' masks = ∅ (FR-010); no `*.md;*.markdown`,
`*.csv/dbf/tsv`, `*.*`; each row ≤ cap. Mask changes after v1 only via the
config-version upgrade protocol (contract `claimed-types.md`).

## 4. Colour scheme (embedded resources — D16)

| Field | Type | Notes |
|---|---|---|
| `id` | stable ASCII | e.g. `github-dark` (persisted value) |
| `nameResId` | string resource | localisable label; pinned in ui-overrides (D18) |
| `polarity` | light \| dark | drives follow-application slots and `color-scheme` |
| `themeJson` | resource | VS Code theme JSON (MIT set of 12) |
| `chromeColors` | derived | window/gutter/status accents + `DefaultBackgroundColor` derived from the theme's editor colours |

**Selection state**: `activeScheme` = explicit id or `follow` (default);
`schemeLight`/`schemeDark` per-polarity slots (mdview `EffectiveTheme`
pattern). Switch = page attribute flip; no reload (FR-014).

## 5. Viewer session/window (runtime, per window)

| Field | Notes |
|---|---|
| `filePath` (UTF-8), `fileSize`, `mtime` | intake identity; re-intake on next/prev (FR-041) |
| `intakeResult` | `text{encoding, eolStyle, invalidCount, lineIndex}` \| `binary` \| `oversize` |
| `encodingOverride` | user selection from the product code-table list (D9); re-decodes |
| `language` | detected id + `overridden?` (FR-007) |
| `band` | `highlighted` \| `plain(reason)` \| declined-before-open |
| `findState` | term, options, matches[], current (JS-side; host mirrors n/N) |
| `viewState` | wrap, lineNumbers on/off, zoom %, scroll anchor (line), selection |
| `enumFilesSourceUID` / `CurrentIndex` | panel enumeration handle for next/prev |

**Lifecycle**: `CanViewFile` (sniff ≤ 8 KB, ≤ 50 ms) → decline | `ViewFile` →
thread spawn → intake → serve `/text` → page renders plain → progressive
highlight → interactive. Window reuse on next/prev swaps content without
navigation. Independent per window (edge case: two windows).

## 6. Plugin configuration (registry, plugin private key)

| Value | Type | Default | Clamp |
|---|---|---|---|
| `Version` | DWORD | 1 | upgrade protocol anchor |
| `ColorScheme` / `FollowAppTheme` / `SchemeLight` / `SchemeDark` | SZ/DWORD/SZ/SZ | follow=1, github-light, github-dark | unknown id → default |
| `FontFamily` / `FontSize` | SZ / DWORD | Cascadia Mono / 0 (=page default) | size 6–72 |
| `TabWidth` | DWORD | 4 | 1–16 |
| `HighlightLimitKB` | DWORD | 1024 | 64–20480 |
| `ViewerLimitMB` | DWORD | 20 | 1–256 |
| `MaxLineLength` | DWORD | 20000 | 1000–100000 |
| `LineNumbers` / `Wrap` | DWORD | 1 / 0 | — |
| `ZoomPercent` | DWORD | 100 | 50–300 |
| `SavePosition` / `WindowPlacement` | DWORD / BINARY | 1 / — | mdview pattern |
| `KeepReady` | DWORD | 1 | 065 mirror (FR-034) |

## 7. Licence manifest (generated, committed — D3)

Per shipped asset: `{asset, upstream repo, licence SPDX, resolution note}`.
Audit re-run is a test; GPL-3.0/GNU/unresolved ⇒ generation fails.

## 8. Hostile-content corpus (test data — D17)

Files under `test/corpus/hostile/`: script/markup injection, entities,
`javascript:`/`data:` URLs, bidi overrides, U+2028/9, lone surrogates,
oversized single lines, binary look-alikes. Adding a file = adding a test
case (R6.3); pass criterion: literal display, no request beyond
document/assets/text, no navigation, unchanged title.
