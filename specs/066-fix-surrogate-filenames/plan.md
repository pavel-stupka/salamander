# Implementation Plan: Fix File Operations on Names with Unpaired Surrogates

**Branch**: `066-fix-surrogate-filenames` | **Date**: 2026-08-22 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/066-fix-surrogate-filenames/spec.md`

## Summary

Windows file names are raw sequences of 16-bit units and may contain unpaired
UTF-16 surrogates (`temp\fixtures-041\Lone�surrogate.txt` carries a lone
`U+D800`). Such names cannot be encoded as valid UTF-8, and the feature-004
UTF-8 path layer destroys them at intake: `SalConvertFindDataW`
(`src/common/salfileio.cpp:61`) falls back to a lenient conversion that bakes
`U+FFFD` into `CFileData::Name`, so every later operation recomposes a path
that does not exist on disk — delete, copy, and move all fail with
"file not found" while the panel happily lists the file.

**Fix**: adopt **WTF-8** as the internal name encoding — a strict superset of
UTF-8 that additionally encodes each unpaired surrogate as its 3-byte sequence
(`ED A0 80`–`ED BF BF`), making the wide→UTF-8→wide round trip lossless for
*every* name Windows permits while remaining byte-identical to UTF-8 for all
valid Unicode names (zero change for 99.99 % of files). The change is
concentrated in the house converter pair `SalWToU8` / `SalU8ToW`
(`src/common/salunicode.cpp`), plus a handful of "valid UTF-8, else ANSI"
transitional probes that must recognize WTF-8 (registry write facade, core
display probes). Precedent: Rust's `OsStr` uses WTF-8 on Windows for exactly
this problem.

## Technical Context

**Language/Version**: C++ (C++20, `/std:c++latest`), MSVC v143 (VS2022)
**Primary Dependencies**: Pure WinAPI; internal shared libs (`src/common/`); no new external dependencies
**Storage**: Windows Registry via the feature-004 facade (`SalRegQueryValueExW8` / `SalRegSetValueExW8`); NTFS/exFAT/FAT/network file systems as managed objects
**Testing**: `saltests` console exe (`src/saltests/saltests.cpp`, CHECK harness, exit code = failure count; Debug-only project, output `%OPENSAL_BUILD_DIR%tandemcommander\Debug_x64\saltests\saltests.exe`) + fixture-based manual validation (quickstart.md)
**Target Platform**: Windows 11+ (x64)
**Project Type**: Desktop application (two-panel file manager)
**Performance Goals**: No measurable panel-listing slowdown — the strict WinAPI fast paths in both converters stay first; the custom WTF-8 codec runs only after the fast path fails (i.e., only for names/text that are invalid today)
**Constraints**: Converters are process-wide primitives — byte output for valid Unicode input MUST remain identical to the current release (config values, comparisons, and caches depend on it); `SalU8ToW` MUST keep failing on all *other* malformed input because the feature-004/063 "valid UTF-8, else ANSI" transitional heuristics rely on that failure
**Scale/Scope**: 2 core converter functions + display variant in `src/common/salunicode.cpp`; ~4 verified interaction sites (`salfileio.cpp`, `salpath.cpp`, `salamdr6.cpp` registry write probe, `gui.cpp`/`salamdr4.cpp` display probes); test additions in `src/saltests/`; new fixture set `temp\fixtures-066\` (not shipped)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment | Status |
|-----------|------------|--------|
| I. Build Reproducibility | No build-system change; new tests join the existing `saltests` project. | PASS |
| II. Backward Compatibility | Pure defect fix. WTF-8 is byte-identical to UTF-8 for every valid Unicode name, so stored config, comparisons, and all existing behavior for ordinary names are unchanged (spec FR-008). No registry layout change; a surrogate-bearing path stored in config becomes *correct* UTF-16 instead of silently corrupted. | PASS |
| III. Incremental Modernization | Localized change to the converter core plus targeted probe fixes; no refactor of adjacent code. | PASS |
| IV. Windows Platform Commitment | Pure WinAPI + a small self-contained codec; no new dependencies. | PASS |
| V. Plugin Architecture Preservation | Plugin ABI untouched (interface version unchanged). Plugin-shared helpers keep their strict converters; a surrogate name crossing the ABI behaves no worse than today's `U+FFFD` name (both unusable by plugins — not a regression). Documented in the contract. | PASS |
| VI. UI Consistency | No new dialogs or controls. Display of the affected names stays a per-unit replacement glyph. | PASS |
| Release Documentation | CHANGELOG entry required under the version that ships this fix (user-facing symptom: "files with names containing unpaired surrogates could not be copied, moved or deleted"). | NOTED |

**Post-Phase-1 re-check**: design introduces no new projects, no ABI change,
no config migration — all gates still PASS.

## Project Structure

### Documentation (this feature)

```text
specs/066-fix-surrogate-filenames/
├── plan.md              # This file
├── research.md          # Phase 0 output — root cause, encoding decision, site survey
├── data-model.md        # Phase 1 output — name entities and invariants
├── quickstart.md        # Phase 1 output — build, fixtures, validation
├── contracts/
│   └── name-encoding-wtf8.md   # Binding contract for the internal name encoding
└── tasks.md             # Phase 2 output (/speckit-tasks — NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
src/
├── common/
│   ├── salunicode.h         # SalWToU8/SalU8ToW/SalU8ToWDisplay declarations + contract comments
│   ├── salunicode.cpp       # THE fix: WTF-8 encoder/decoder behind the strict fast paths
│   ├── salfileio.cpp        # SalConvertFindDataW — U+FFFD fallback becomes dead/last-resort
│   └── salpath.cpp          # SalPathToWExtAlloc / full-path round trip (heals via converters)
├── salamdr6.cpp             # SalRegSetValueExW8 — write-side validity probe must accept WTF-8
├── gui.cpp                  # CStaticText::SetText UTF-8 probe (info line, dialogs) → WTF-8-aware
├── salamdr4.cpp             # remaining core "valid UTF-8, else ANSI" probes (surveyed in tasks)
└── saltests/
    └── saltests.cpp         # codec round-trip tests; existing line-70 expectation updated

temp/
└── fixtures-066/            # generated fixture set (surrogate-name files/folders; never shipped)
```

**Structure Decision**: single-project change inside the existing solution; no
new projects, no new files except the fixture directory. The converter core
(`src/common/salunicode.cpp`) is the single point where the encoding contract
is implemented; call-site changes are limited to probes that test "is this
valid UTF-8" by calling the WinAPI directly instead of the house converters.

## Complexity Tracking

No constitution violations — table not needed.
