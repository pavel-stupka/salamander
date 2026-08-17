# Contract: Category → Registry Mapping

**Feature**: 057-altap-settings-migration
**Status**: authoritative (this is the FR-004 "definitive per-category list
fixed during planning"); evidence in [research.md](../research.md)

All source paths are relative to the selected source root (e.g.
`HKCU\Software\Altap\Altap Salamander 4.0`), destination paths relative to
`HKCU\Software\Tandem Commander\0.1` (overridable for tests). Source access
is read-only everywhere. "Replace" = delete the destination scope, then
write the mapped source content (Clarifications 2026-08-17).

## Source generations

| Generation | Source `Version\Configuration` | Notes |
|------------|-------------------------------|-------|
| AS4 | ≥ 100 | Altap Salamander 4.0 line (4.0 = 103) |
| AS3 | 66–99 | 3.0–3.08 |
| AS25 | 39–65 | 2.5–2.55 beta |
| Ancient | < 39 (incl. missing key ⇒ 1) | Servant-branded era; best-effort |

Source qualifies only if its root has a `Configuration` subkey. A
`Save In Progress` value on the source root ⇒ warn "source may contain an
interrupted save" (still offered, read-only).

**Scan list**: the ~79 historical per-user roots recovered from the product's
own pre-feature-032 `SalamanderConfigurationRoots[]` (research R2) —
`Software\Altap\Altap Salamander 4.0` down through the Servant-branded 2.x
roots to `Software\Salamander`, excluding `Software\Open Salamander\5.0`.
The exact literals (including the 2.5x-era embedded spaces in build tags,
e.g. `"(DB 72)"`) are embedded in the utility verbatim from that array.

## Offered categories

| Id | Display name | Source scope | Destination scope | Verdict by generation | Item count |
|----|--------------|--------------|-------------------|----------------------|-----------|
| `hotpaths` | Directory hot paths | `Hot Paths` (slot subkeys `0`–`30`) | `Hot Paths` | AS4/AS3: verbatim · AS25 cfg<47 & Ancient: transform T1 | populated slots (subkeys with non-empty `Path`) |
| `usermenu` | User menu commands | `User Menu` (`1..n`) | `User Menu` | all: verbatim | numbered subkeys |
| `viewers-editors` | Viewer & editor associations | `Viewers`, `Alternative Viewers`, `Editors` (`1..n` each) | same three keys | all: transform F1 (+T2 when cfg<44); Ancient requires cfg ≥ 6 | rows kept after F1, across the three keys |
| `confirmations` | Confirmation prompts | `Configuration\Confirmation` (flat values) | `Configuration\Confirmation` | all: verbatim | values present |
| `colors` | Colors & panel highlighting | `Colors` (incl. `Color Scheme` value + `Panel Items Hilighting` subkey), `Custom Colors` | same | AS4/AS3/AS25: verbatim · Ancient: **skip** ("color format conversions predate Altap Salamander 2.5") | highlight rules + 1 if a non-default scheme/palette is stored |
| `viewtemplates` | Panel view templates | `View Templates` (subkeys `0`–`9`) | `View Templates` | AS4/AS3/AS25: verbatim · Ancient: **skip** (pre-2.5 template conversions not reproduced) | template subkeys |
| `viewer-settings` | Internal viewer settings | `Viewer` (flat values) | `Viewer` | all: verbatim, minus exclude X-VIEWER | values copied |
| `defaultdirs` | Per-drive default directories | `Default Directories` (values `A`–`Z`) | `Default Directories` | all: filter F2 | values passing F2 |
| `general-config` | General configuration | `Configuration` values + subkeys `Drive Special Settings`, `Copy Move Options`, `Find Options`, `Find Ignore` | same | all: exclude-list X-CONFIG; replace preserves destination `Theme Mode` | values + item subkeys copied |
| `ftp` | FTP connections (bookmarks, proxies, server types) | `Plugins Configuration\<srcKey(FTP)>` whole subtree | `Plugins Configuration\<dstKey(FTP)>` | all: verbatim subtree (incl. plugin's own `Version` value) + password rule P1 | `Bookmarks` subkeys ("n servers") |
| `plugin-configs` | Other plugin settings | `Plugins Configuration\<srcKey(p)>` for each p in PLUGSET present in source | `Plugins Configuration\<dstKey(p)>` | all: verbatim subtree per plugin (incl. its `Version` value) | plugins with a config subtree present |

`srcKey(p)`/`dstKey(p)`: resolved by joining the `DLL` value across
`Plugins\<n>` registrations in each root (numeric subkey order is
installation-specific — never match by number). When a root has no
registration for p (virgin TC), use the literal from PLUGSET.

**PLUGSET** (literal `Configuration Key` values, from each plugin's source):
`ZIP`, `7zip`, `Checksum`, `DBVIEWER`, `DISKMAP`, `File Comparator`,
`PEVIEWER`, `PictView`, `RegEdit`, `Renamer`, `UnCAB`, `UNDELETE`, `UnISO`
(and `FTP` for the `ftp` category). `TAR` is excluded (stores only its
schema `Version` — nothing user-set).

## Transforms & filters

- **T1** (`hotpaths`, source cfg < 47): double every `$` in each slot's
  `Path` value (`$` → `$$`). `Name`/`Visible` copied as-is; the TC-only
  `Icon` value simply doesn't exist in sources (defaults to 0).
- **T2** (`viewers-editors`, source cfg < 44): lowercase the extension text
  of each `Masks` value.
- **F1** (`viewers-editors`): drop rows whose `Type` is negative (positional
  plugin-viewer references — plugin tables differ between products) and rows
  whose `Masks` contains `|` (TC's loader would silently truncate the list
  there); renumber remaining rows `1..n`; report each dropped row.
- **F2** (`defaultdirs`): keep only values whose name is a single letter
  `A`–`Z` and whose REG_SZ data starts with that same letter followed by
  `:\` and is longer than 3 chars (anything else triggers modal error boxes
  in TC at every start); report dropped values.
- **P1** (`ftp` passwords, FR-010):
  - Destination `Password Manager\Use Master Password` = 1 already → never
    touch the destination `Password Manager`; strip `PasswordE` values from
    copied bookmarks/proxies (keep the rest of the entry, clear its
    `Save Password` flag) and list each in the summary as "password must be
    re-entered (Tandem Commander uses a different master password)" — unless
    the user is told in NOTES that identical master passwords would have
    worked; the utility never prompts for master passwords.
  - Source `Use Master Password` = 1, destination absent/0 → copy
    `Use Master Password` + `Master Password Verifier` **atomically** with
    the category; NOTES: "your existing master password now protects Tandem
    Commander's stored passwords."
  - Neither uses a master password → nothing extra (`PasswordS` blobs are
    portable by construction).
- **X-CONFIG** (`general-config` exclude-list; never copied):
  values `Language`, `Use Alternate Language for Plugins`,
  `Alternate Language for Plugins`, `Language Changed`,
  `Plugins.ver Version (x64)`, `Plugins.ver Version (x86)`,
  `Enable Custom Icon Overlays`, `Disabled Custom Icon Overlays`,
  `Top ToolBar`, `Middle ToolBar`, `Left ToolBar`, `Right ToolBar`
  (layout id strings only — `Show * ToolBar` visibility flags ARE copied),
  `Conversion Table`, `Main window icon index`, `Only One Instance`,
  `Last Focused Page`, `Configuration Height`,
  `Viewers And Editors Expanded`, `Packers And Unpackers Expanded`,
  `Current Tip Index`, `Theme Mode` (TC-only; also preserved on delete);
  subkeys: `Confirmation` (own category), all `* History` subkeys,
  `Working Directories`.
- **X-VIEWER** (`viewer-settings` exclude-list): window placement values
  (`Left`, `Right`, `Top`, `Bottom`, `Show`, `Save Window Position`) and
  the search-session values (`Find Text`, `HEX-mode`) — geometry/session
  state; everything else copies.

## Never copied (any category, FR-005)

Source root values `Save In Progress`, `AutoImportConfig`, `Copy Is OK`; the
source `Version` key; the `Plugins`, `Plugins Order` registration keys
(installation metadata: DLL paths, icon caches, menu cache, function masks);
configs of plugins TC does not ship (reported when present: Automation,
CHECKVER, Encrypt & Decrypt, IEVIEWER, MMVIEWER, nethood, SplitCombine,
UnARJ, UnMIME, UnRAR, WMOBILE); `Packers & Unpackers` subtree (feature-010
reset + positional indexes — reported); `Internal ZIP Packer` (no reader);
`Left Panel`, `Right Panel`, `Window`, `Find Dialog Window` (session/geometry
— reported as "excluded by design").

## Destination integrity rules (every run that writes)

1. Ensure `Version\Configuration` (REG_DWORD) = 105 — create if absent,
   never lower, never copy from source (research R4).
2. Ensure the `Configuration` subkey exists (create empty if needed) —
   without it TC ignores the whole root and overwrites it with defaults at
   first save (research R4).
3. Never write `Save In Progress`, `AutoImportConfig`, or `Copy Is OK` on
   the destination root.
4. All writes use type-exact wide-char registry APIs; value types are
   preserved byte-for-byte (TC shows a modal error per wrongly-typed value).
5. NOTES flagging (summary): any copied `Command`/`Icon`/`Initial
   Directory`/path-like value that points into a detected Altap Salamander
   installation directory; `undelete`'s `Temp Path`; the master-password
   guidance from P1; source `Save In Progress` warning.
