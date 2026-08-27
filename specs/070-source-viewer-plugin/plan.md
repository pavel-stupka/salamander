# Implementation Plan: Source & Configuration File Viewer (codeview)

**Branch**: `070-source-viewer-plugin` | **Date**: 2026-08-26 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/070-source-viewer-plugin/spec.md`

## Summary

New viewer plugin **codeview** (`src/plugins/codeview/`, `codeview.spl`): the
primary F3 viewer for ~780 file-name patterns / ~225 source and configuration
formats, rendering read-only syntax-highlighted views in the shared WebView2
engine (feature 065 contract). Highlighting runs as the plugin's own bundled
JavaScript (scripts enabled on this plugin's controllers only — clarification
2026-08-26) using **Shiki** (`@shikijs/core` + JavaScript RegExp engine) with
licence-filtered TextMate grammars and ~12 MIT-licensed VS Code themes (light
and dark). The plugin is built structurally as a second mdview: thread-per-
window viewer, `CanViewFile` decline cascade (binary / >20 MB / surrogate
names → built-in viewer), session keeper, and — mandated by the shared-engine
contract — the WebView2 options helper, UDF, keeper and host are **lifted
from mdview into `src/common/`** and consumed by both plugins. Language
identification and the claimed-mask list are generated at development time
from pinned GitHub Linguist data bridged to the shipped grammar set, and
committed.

## Technical Context

**Language/Version**: C++ (C++20, `/std:c++latest`), MSVC v143 (VS2022); page-side JavaScript (ES2022 modules, runs in the Evergreen WebView2 runtime)
**Primary Dependencies**: WebView2 SDK (vendored, `src/common/dep/webview2/`, v1.0.4078.44); Shiki `@shikijs/core` + `@shikijs/engine-javascript` + `tm-grammars`/`tm-themes` (vendored as prebuilt, committed ESM chunks; bundled dev-side, pinned versions); `linguist-languages` + Linguist `heuristics.yml` (dev-side generator input, pinned revision — not shipped)
**Storage**: Windows Registry — plugin private key under `HKCU\Software\Tandem Commander\0.1\Plugins` (scheme, font, tab width, limits, keep-ready, zoom, window placement); no new files outside the plugin tree
**Testing**: standalone plugin test harness (`src/plugins/codeview/test/`, mdview/sftp precedent): detection-table tests, hostile-content corpus, encoding matrix, mask-intersection check, licence audit; existing `saltests` untouched; manual quickstart scenarios
**Target Platform**: Windows 11+ x64/x86, WebView2 Evergreen runtime (OS component; graceful fallback to built-in viewer when absent)
**Project Type**: desktop-app plugin (`.spl` + `.slg` language module), two new projects in `salamand.sln`
**Performance Goals**: warm open of ≤ 100 KB file → text visible ≤ ~0.3 s (065 budget, FR-036/SC-003); first screen highlighted within the same budget; find first match on 1 MB ≤ 200 ms; binary/oversize classification ≤ 50 ms reading ≤ 8 KB
**Constraints**: zero network at runtime (default-deny interception + CSP); all assets inside the signed `.spl`; GPLv2-or-later-compatible assets only (GPL-3.0 excluded — clarification 2026-08-26); zero background work before first plugin use (065 FR-001 parity); plugin ABI (interface 106) unchanged; mdview user-visible behaviour unchanged
**Scale/Scope**: ~240 grammar files (~10 MB vendored), 65→~12 shipped themes, ~780 masks/~225 languages, files ≤ 20 MB (viewer limit), highlight ≤ 1 MB & ≤ 20 000-char lines (defaults, configurable)

## Constitution Check

*GATE: evaluated against Constitution v3.1.0 before Phase 0; re-checked after Phase 1.*

| Principle | Verdict | Evidence |
|---|---|---|
| I. Build Reproducibility | **PASS** | The product build stays offline and single-command: all web assets (page, Shiki engine, grammar/theme chunks) are committed prebuilt and embedded as resources. The dev-side bundling/generation step (`tools/codeview/`) is pinned, scripted and documented (same class as `tools/brand/gen_icons.py` and the translation tooling — developer-side, never invoked by the build). |
| II. Backward Compatibility | **PASS** | Nothing regresses: built-in viewer untouched and always reachable (Alt+F3, decline cascade); user Viewers lists never reordered/resurrected (FR-011, `AddViewer` one-shot semantics); no config migration; mdview behaviour unchanged (lift is behaviour-preserving, retested). |
| III. Incremental Modernization | **PASS** | mdview is touched only to extract the shared WebView2 host code (behaviour-preserving move); no adjacent refactoring. |
| IV. Windows Platform Commitment | **PASS** | Pure WinAPI + WebView2 (Windows OS component, never distributed). All vendored assets licence-audited GPLv2-compatible; GPL-3.0-only grammars/themes excluded (clarification); Apache-2.0 items acceptable via "or later", flagged in `doc/third_party.txt`. |
| V. Plugin Architecture Preservation | **PASS** | Self-contained functionality delivered as a plugin; interface 106 unchanged; shared host code lives in `src/common/` as compiled-in sources (no new plugin API, no `LAST_VERSION_OF_SALAMANDER` bump — core-hosted keeper service remains deferred per `architecture/11` §2.5). |
| VI. UI Consistency | **PASS** | Native dialogs (`DIALOGEX`, `DS_SHELLFONT`, `FONT 8, "MS Shell Dlg"`), feature-049 dark dialog pattern, mdview's `darkmenu` for menus, native status bar; no `ICC_STANDARD_CLASSES`, no manifest, no subclassing of standard controls. |
| Release Documentation | **PASS (deferred to ship)** | `CHANGELOG.md` entry + version/build bump in the same change when the feature ships. |

**Post-Phase-1 re-check**: PASS — the design artifacts introduce no violation;
Complexity Tracking stays empty.

## Project Structure

### Documentation (this feature)

```text
specs/070-source-viewer-plugin/
├── plan.md              # This file
├── research.md          # Phase 0: consolidated decisions D1–D18
├── data-model.md        # Phase 1: entities & state
├── quickstart.md        # Phase 1: build & validation guide
├── contracts/
│   ├── webview-host-sharing.md   # the mdview → src/common lift (binding)
│   ├── rendering-lockdown.md     # scripts-on security contract (binding)
│   ├── claimed-types.md          # mask registration & generation contract
│   └── host-page-interface.md    # virtual host, resources, message schema
├── checklists/requirements.md
└── research/            # five specification-phase reports (inputs)
```

### Source Code (repository root)

```text
src/common/webhost/                  # NEW — lifted from src/plugins/mdview/webview.{h,cpp}
├── webhost.h                        # CTcWebHost (parameterised host), options helper,
├── webhost.cpp                      #   UDF helper, availability gate, lockdown routine
├── webkeeper.h                      # session keeper (parameterised window class,
└── webkeeper.cpp                    #   per-plugin arm/disarm) — compiled into each plugin

src/plugins/mdview/                  # CHANGED — consumes src/common/webhost/ (webview.cpp
│                                    #   shrinks to mdview-specific glue); no behaviour change
src/plugins/codeview/                # NEW plugin
├── codeview.cpp                     # plugin entry, CPluginInterface, Connect (AddViewer rows)
├── codeview.h / codeview.rh / codeview.rh2
├── viewer.h / viewer.cpp            # CViewerWindow, thread-per-window, CanViewFile/ViewFile,
│                                    #   find dialog, menus, status bar, next/prev file
├── intake.h / intake.cpp            # binary sniff, encoding detect/decode, size gates
├── langmap.h / langmap.cpp          # generated: masks + name→language table + heuristics
├── webglue.h / webglue.cpp          # codeview host config (virtual host, interceptor data,
│                                    #   web-message schema, accelerator map)
├── config.cpp                       # load/save/clamp registry config, configuration dialog
├── codeview.rc / codeview.rc2 / codeview.def / versinfo.rh2 / precomp.h/.cpp
├── res/plugico.bmp
├── web/                             # committed prebuilt page assets (embedded as resources)
│   ├── viewer.html / viewer.css / viewer.js       # page shell, virtual lines, find, themes
│   ├── shiki/                       # engine + per-language ESM chunks (licence-filtered)
│   └── themes/                      # ~12 VS Code theme JSONs (MIT)
├── lang/                            # lang.rc / lang.rc2 / lang.rh (english.slg)
├── vcxproj/                         # codeview.vcxproj, codeview.props,
│                                    #   lang_codeview.vcxproj, lang_codeview.props
└── test/                            # harness: detection tests, hostile corpus,
                                     #   encoding matrix, mask-intersection, licence audit

tools/codeview/                      # NEW dev-side tooling (never run by the build)
├── gen_langmap.py                   # linguist(pinned) + overlays → langmap.cpp + masks
├── build_web.py                     # pinned npm deps → esbuild → web/ chunks + licence audit
└── README.md                        # reproducibility: pinned versions, regen instructions

plugins.cfg                          # +codeview=on
src/vcxproj/salamand.sln             # +codeview, +lang_codeview projects
tools/translate/uicontext.py         # +_DOMAINS["codeview"]
translations/<lang>/codeview.slt     # 8 languages via the two-stage refresh
translations/ui-overrides.json       # pin the plugin display name
doc/third_party.txt                  # Shiki, tm-grammars/tm-themes, per-grammar notices
architecture/11-webview2-integration.md  # repointed to src/common/webhost/
```

**Structure Decision**: single-solution desktop app; the feature adds one
plugin directory pair (`codeview` + its lang module), one shared source
library (`src/common/webhost/`, compiled into consuming plugins like
`src/plugins/shared/*.cpp` — no new binary), and dev-side generators under
`tools/codeview/`. mdview changes are confined to consuming the lifted code.

## Complexity Tracking

*No constitution violations — table intentionally empty.*
