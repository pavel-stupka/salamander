# Tandem Commander Brand Assets — How to Swap the Graphics

This directory is the single place to change the application's graphics.
Every swap is: **replace a file here → run one command → rebuild.** No
source-code, resource-script, or project-file edits are ever needed, and no
knowledge of the app's internals is required.

## What can be replaced

| File | Where it appears | Format & size |
|------|------------------|---------------|
| `icon-master.png` | Application icon everywhere: main-window top-left corner, taskbar, `tandemcommander.exe` in Explorer, crash reporter (salmon), installer, uninstaller | PNG, **square**, edge ≥ 256 px (1024×1024 recommended). All icon sizes are derived from it automatically (high-quality Lanczos downscale). |
| `icon-16.png`, `icon-24.png`, `icon-32.png`, `icon-48.png`, `icon-64.png`, `icon-128.png`, `icon-256.png` | Optional per-size overrides of the master | PNG, exactly N×N for `icon-<N>.png`. If present, the file wins over the master-derived rendering at that size — useful for hand-tuning tiny sizes. **Delete them when swapping the icon wholesale**, otherwise the old artwork stays at those sizes. Currently all seven are present: the designer's per-size renders of the full-bleed icon (feature 046). |
| `about.png` | Artwork in the About dialog (Help → About) and on the startup splash screen | PNG, alpha supported, any size (≈ 512 px on the long edge recommended). The app scales it to fit at draw time, aspect ratio preserved — no distortion, no cropping. Currently the margin+drop-shadow icon variant. |
| `hotpath-master.png` | Hot path icon gallery (feature 047): the color variants offered in Options → Configuration → Hot Paths and shown on the Hot Path Bar and in the Change Drive menu | PNG, **square**, edge ≥ 256 px, transparent background. Keep the fill white/light with a dark outline — the RGB channels are multiplied by each gallery color (tint table in `gen_icons.py`), producing `src/res/hotpath1.ico` … `hotpath9.ico` (16/20/24/32 px). The tint order is append-only: the gallery index is persisted in user configurations. The gallery's default entry (index 0) is the system bookmark icon and is not generated here. |

## How to swap

1. Replace the file(s) above with your new artwork.
2. Regenerate the shipped assets:

   ```batch
   python tools\brand\gen_icons.py
   ```

   This rewrites `src/res/salamand.ico`, `src/salmon/res/salmon.ico`,
   `src/setup/res/setup.ico`, `src/setup/remove/icon1.ico`, the hot path
   gallery `src/res/hotpath1.ico` … `hotpath9.ico`, and copies
   `about.png` to `src/res/logo.png`. Inputs are validated first — a bad or
   missing file stops the run with an `error:` line naming the file and the
   expected property, and nothing is written. After regenerating, also copy
   `src/setup/res/setup.ico` over `setup/setup.ico` (Inno Setup icon).
3. Rebuild and check the result:

   ```batch
   build.cmd
   ```

   Then look at: window top-left icon, taskbar, the exe in Explorer,
   Help → About, and the splash screen (enable it under Options →
   Configuration → Main Window if turned off).

Commit the changed files under `tools/brand/`, `src/res/`,
`src/salmon/res/`, `src/setup/` and `setup/`.

Optional structural self-check of the committed outputs (no writes):

```batch
python tools\brand\gen_icons.py --verify
```

**Requirements**: Python 3 with Pillow (`pip install pillow`) on the
developer machine only — the build itself never runs Python; the generated
files are committed.

## Brand palette & usage (feature 046, from the Tandem design delivery)

### Orange (primary — the folder, the word "Commander")

| Role | Hex |
|---|---|
| Highlight (top gradient stop) | `#FFB35C` |
| Base / brand orange | `#F97316` |
| Deep | `#EA6A0B` |
| Shadow / folder back | `#D96A15` → `#B85306` |
| Darkest inner shadow | `#8A3E04` |
| Flap details (handle, dot) | `#FFD9AE`, `#FFC98F` |

Folder gradient: vertical `#FFB35C` → `#F97316` (55 %) → `#EA6A0B`.

### Blue / paper (secondary) and text

| Role | Hex |
|---|---|
| White sheet | `#FDFEFF` |
| Back sheet | `#EAF2FB` |
| Text lines on the sheet | `#93C5FD` |
| "Tandem" — light bg / dark bg | `#0A1424` / `#EAF2FB` |
| "Commander" — light bg / dark bg | `#EA6A0B` / `#F97316` |
| Tagline — light bg / dark bg | `#5D82B8` / `#8FA6C4` |

These are the same values the in-app wordmark uses (`TC_COLOR_*` in
`src/logo.cpp`).

### Backgrounds & rules

- Light: white `#FFFFFF` to `#F7F8FA`. Dark: deep navy — recommended
  `#0A1424`, range `#02060D`–`#131C2E`.
- Lockups have transparent backgrounds; don't place them on photos or
  saturated colors.
- Clear space: at least icon height ÷ 4 on all sides. Minimum sizes: lockup
  240 px wide, icon 32 px — below 32 px use the full-bleed per-size renders
  (already wired in as `icon-<N>.png`).
- Don't mix light/dark lockups across backgrounds, don't recolor the
  gradients, don't add outlines or extra shadows, don't rebuild the
  icon+wordmark composition yourself.
- Typography of the lockups: Archivo (Google Fonts), fallback Arial; the SVG
  text is live `<text>` — convert to outlines if pixel-identical rendering is
  needed somewhere without the font.

## What is NOT replaceable here (drawn by code)

- The "Tandem Commander" wordmark in About/splash — GDI-drawn text
  (`src/logo.cpp`, `TCDrawWordmark`), no font shipped.
- The blue→orange accent strips in About/splash —
  `src/res/gradspl.svg` / `gradabt.svg` (source: `gradient-band.svg`).
- Toolbar, panel, and file-type icons.

## Reference files (not shipped)

| File | Purpose |
|------|---------|
| `tandem-commander-icon.svg` | Master vector of the icon with margin + drop shadow (About/splash artwork source) |
| `tandem-commander-icon-full.svg` | Master vector of the full-bleed icon — the PNG renders used for the shipped `.ico` were exported from it |
| `tandem-commander-lockup-light.svg` | Horizontal lockup for light backgrounds (900×200) |
| `tandem-commander-lockup-dark.svg` | Horizontal lockup for dark backgrounds (900×200) |
| `gradient-band.svg` | Source of the shipped accent strips |

## Technical notes

- ICO layout: entries 16, 24, 32, 48, 64, 128, 256 px, all 32-bpp;
  BMP-encoded ≤ 64 px, PNG-encoded ≥ 128 px (Windows convention).
- `about.png` is decoded at runtime by Windows WIC and alpha-blended, so
  any valid PNG (including full alpha) works; there are no other
  constraints on its content.
- The red/green/blue main-window icon variants were removed in feature 035;
  the icon pipeline no longer generates `sal_r/g/b.ico`.

## Overlay badges (feature 059)

- `gen_overlay_syncpend.py` generates `src/res/syncpend.ico` — the cloud
  "sync pending" overlay badge (blue circular arrows, original artwork in
  the Windows overlay style: transparent full frame, glyph in the
  lower-left quadrant). Frames 16/32/48 px, 32-bpp BMP-encoded.
  Regenerate with `python tools/brand/gen_overlay_syncpend.py`
  (requires Pillow).
