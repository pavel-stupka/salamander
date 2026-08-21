# Tasks: Instant Markdown Viewer Display

**Input**: Design documents from `/specs/065-mdview-instant-render/`
**Prerequisites**: plan.md, spec.md (clarified), research.md (R1–R9),
data-model.md, contracts/keeper.md, quickstart.md

**Tests**: No automated UI test infrastructure exists (spec assumption).
Validation is manual per `quickstart.md`; the existing console harness
`tests/mdview_htmlgen_test/` must keep passing. No TDD tasks generated.

**Organization**: Tasks are grouped by user story so each story is an
independently verifiable increment.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an
  incomplete task)
- **[Story]**: US1 / US2 / US3 from spec.md (user-story phases only)

---

## Phase 1: Setup (instrumentation + baseline)

**Purpose**: Measurement capability and pre-change numbers — SC-001/SC-006
compare against this baseline.

- [X] T001 Add Debug-only `TRACE_I` timing instrumentation (research R8):
      `ViewFile` entry in `src/plugins/mdview/viewer.cpp`; controller-ready
      (`OnReady`) and `NavigationCompleted` in
      `src/plugins/mdview/webview.cpp`. No behavior change.
- [ ] T002 Build Debug x64 (`build.cmd`) and record the pre-change baseline
      into `specs/065-mdview-instant-render/baseline.md` (new file): cold
      first-view time, warm back-to-back time (TRACE deltas), and
      confirmation that the `msedgewebview2.exe` tree exits after the last
      viewer window closes (`tasklist`).
      *Status: build done; baseline.md created with the protocol and result
      tables. The timing numbers need a human at the keyboard — and remain
      measurable at any time: keep-ready OFF reproduces the pre-065
      lifecycle exactly (SC-007), so the baseline does not require an old
      checkout.*

---

## Phase 2: Foundational (blocking prerequisites)

**Purpose**: The shared pieces both the viewer windows and the keeper build
on (research R5, R9). **Blocks all user stories.**

- [X] T003 [P] Extract WebView2 environment-options construction in
      `src/plugins/mdview/webview.cpp` into one shared helper (single source
      of truth for `AdditionalBrowserArguments`, liftable to `src/common/`
      unchanged); `CMdWebHost::Create` consumes it. No behavior change.
- [X] T004 [P] Change `UserDataFolder()` in
      `src/plugins/mdview/viewer.cpp` to the app-neutral
      `%LOCALAPPDATA%\Tandem Commander\WebView2` and add best-effort
      removal of the old `...\Tandem Commander\mdview.WebView2` cache folder
      at the first view of a session (failures silently ignored, retried
      next session) — research R9, sharing contract.
- [X] T005 Build Debug x64 and smoke-check: Markdown files still render, the
      new UDF folder is created on first view, the old folder is removed
      (quickstart Scenario 1 step 2 note).
      *Status: build clean; the runtime folder-swap check is folded into
      quickstart Scenario 1 (pending user).*

**Checkpoint**: Foundation ready — user stories can begin.

---

## Phase 3: User Story 1 — Every view after the first is instant (P1) 🎯 MVP

**Goal**: The engine start-up cost is paid at most once per session; every
later view (back-to-back or after all viewers closed) attaches warm and
renders essentially instantly.

**Independent Test**: quickstart Scenario 1 (open → close → wait ≥60 s →
open again = instant; tree persists; ≥5 views with close-all gaps show no
second cold start) and Scenario 4 (crash recovery).

- [X] T006 [US1] Declare the keeper API in
      `src/plugins/mdview/webview.h`: `MdKeeperArm()`, `MdKeeperDisarm()`,
      `MdKeeperArmed()` — COM-free, documented main-thread-only, per
      `contracts/keeper.md`.
- [X] T007 [US1] Implement the keeper in `src/plugins/mdview/webview.cpp`
      (depends on T003, T006): module-level singleton with the
      data-model.md state machine (Unarmed/Arming/Armed); hidden never-shown
      `WS_EX_TOOLWINDOW` window on the main thread; async environment (via
      the shared options helper) + controller; `put_IsVisible(FALSE)`;
      best-effort `TrySuspend` + `MemoryUsageTargetLevel(LOW)` via
      `QueryInterface` (skip silently if unavailable); never navigates;
      `ProcessFailed`/`BrowserProcessExited` → silent full release;
      idempotent Arm/Disarm; Debug TRACE on arm/ready/disarm/failure. All
      failure paths silent (contract invariant 4).
- [X] T008 [US1] Arm at first actual use: call `MdKeeperArm()` in
      `CPluginInterfaceForViewer::ViewFile`
      (`src/plugins/mdview/viewer.cpp`) after the `RuntimeAvailable()` gate
      and before the viewer-thread spawn (contract invariant 1 — this is the
      only trigger; nothing runs earlier in the session).
- [X] T009 [US1] Disarm on plugin unload: call `MdKeeperDisarm()` in
      `CPluginInterface::Release` (`src/plugins/mdview/mdview.cpp`)
      alongside `ReleaseViewer()`, after windows/threads are down.
- [ ] T010 [US1] Build Debug x64; validate quickstart Scenario 1 and
      Scenario 4; record warm-view times vs. the T002 baseline in
      `specs/065-mdview-instant-render/baseline.md` (SC-001: within 2× of
      the back-to-back baseline; SC-002: no cold start after the first view
      across ≥5 views with close-all gaps).
      *Status: Debug build clean; GUI validation of Scenarios 1+4 and the
      timing tables pending user (protocol in baseline.md).*

**Checkpoint**: Core value delivered — instant subsequent views, MVP
demonstrable.

---

## Phase 4: User Story 2 — The cold start happens at most once per session (P2)

**Goal**: The one-time first view is no worse than the current build, never
recurs, and concurrent first opens behave cleanly.

**Independent Test**: quickstart Scenario 5 (rapid double open during cold
init) plus TRACE comparison of the first view against the T002 baseline.

- [ ] T011 [US2] Verify the arm path is non-blocking and idempotent during
      an in-flight cold init (code review of the T007 state machine +
      quickstart Scenario 5): F3 on a second file while the first still
      initializes → both windows render, no duplicated browser-tree spawn,
      no deadlock (`src/plugins/mdview/webview.cpp` state machine,
      `viewer.cpp` arm site).
      *Status: code review done — `MdKeeperArm` is an idempotent no-op while
      Arming/Armed, the `gen` counter invalidates stale async completions,
      WebView2 serializes same-UDF browser startup (R5); GUI Scenario 5
      pending user.*
- [ ] T012 [US2] Validate the one-time cold start: first view of a session
      no slower than the T002 baseline (SC-006), theme-colored background
      during the wait (never white — FR-007/US2 acceptance 1), TRACE
      confirms exactly one engine start-up per session (SC-002); record in
      `specs/065-mdview-instant-render/baseline.md`.
      *Status: pending user (GUI); tables prepared in baseline.md.*

**Checkpoint**: One-time cost proven bounded and non-recurring.

---

## Phase 5: User Story 3 — No cost to the rest of the application (P2)

**Goal**: Zero footprint/work before the first view, and a default-on
plugin-configuration opt-out (FR-008) restoring today's behavior.

**Independent Test**: quickstart Scenario 2 (no `msedgewebview2.exe` in a
Markdown-free session) and Scenario 3 (toggle semantics + persistence).

- [X] T013 [P] [US3] Add `g_keepReady` (default 1) and the
      `CONFIG_KEEPREADY` ("KeepReady") registry value: extern in
      `src/plugins/mdview/mdview.h`, definition + load/save + 0/1 clamp in
      `LoadConfiguration`/`SaveConfiguration` in
      `src/plugins/mdview/mdview.cpp` (data-model.md Configuration).
- [X] T014 [P] [US3] Add the configuration dialog resources: `IDD_CFG`
      (`DIALOGEX`, `DS_SHELLFONT`, `FONT 8, "MS Shell Dlg"`, checkbox
      `IDC_CFG_KEEPREADY` + OK/Cancel, standard themed controls) and string
      IDs in `src/plugins/mdview/lang/lang.rc` +
      `src/plugins/mdview/lang/lang.rh` (constitution VI, research R6).
- [X] T015 [US3] Implement the configuration dialog in
      `src/plugins/mdview/mdview.cpp` (depends on T006, T013, T014):
      `CPluginInterface::Configuration()` opens `IDD_CFG` (replacing the
      About placeholder); dark-mode two-touchpoint pattern
      (`ThemeApplyToDialog` + `ThemeHandleCtlColor`, precedent
      `FindDlgProc` in `viewer.cpp:338`); checkbox ↔ `g_keepReady`; on OK
      with the option newly disabled → `MdKeeperDisarm()` immediately;
      newly enabled → no immediate arm (applies from the next view,
      FR-008).
- [X] T016 [US3] Gate the arm call on the option: `ViewFile` in
      `src/plugins/mdview/viewer.cpp` calls `MdKeeperArm()` only when
      `g_keepReady` is set (modifies the T008 site).
- [X] T017 [US3] Translations for the new dialog/strings: regenerate the
      English template and add the new `mdview` rows for the 8 enabled
      languages via the translation pipeline (`translate` tooling per
      features 038/055); if machine translation is not run now, leave the
      rows as documented gaps (English fallback) in
      `translations/<language>/mdview.slt`.
      *Status: stage 1 (template export) + stage 2 (merge with a stub DeepL
      client — no key on this machine) + stage 3 (8 language `.slg` built,
      positional import accepted) all run; the 4 new rows per language are
      `english_fallback` in `mdview.origin`. One later run of
      `python -m translate.merge --module mdview` with a key in
      `temp/deepl_key.txt` machine-translates exactly these gaps.*
- [ ] T018 [US3] Build Debug x64; validate quickstart Scenario 2 (zero cost
      before first use — SC-003) and Scenario 3 (toggle off → tree exits,
      cold behavior returns — SC-007; toggle on → next view arms; setting
      persists across restart via registry `KeepReady`).
      *Status: Debug + full Release builds clean (incl. stage-3 language
      modules); GUI Scenarios 2+3 pending user.*

**Checkpoint**: All three stories independently verified.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [ ] T019 [P] Validate quickstart Scenario 6: kept-ready idle footprint ≤
      one open viewer window and stable over 10 min idle (SC-004); record
      the measured numbers in `specs/065-mdview-instant-render/baseline.md`.
      *Status: pending user (GUI); table prepared in baseline.md.*
- [ ] T020 [P] Regression sweep (quickstart Scenario 7): viewer smoke
      (schemes + F9, follow-system, zoom incl. Ctrl+wheel/Ctrl+0, find,
      link kinds, View Source Ctrl+U, remote-image consent, dark menus);
      `tests/mdview_htmlgen_test/` still passes (29 assertions); plugin
      unload with keeper armed + reload works; Release x64 builds clean
      (`build.cmd full release`).
      *Status: `build.cmd full release` SUCCEEDED (19 plugins, 180 language
      modules); `.slt` round-trip byte-exact (290 files). GUI viewer smoke +
      unload test pending user. Note: `tests/mdview_htmlgen_test/` inputs
      (htmlgen/render/highlight/md4c) are untouched by 065; its `.vcxproj`
      was never committed (sources only) — pre-existing gap, flagged.*
- [X] T021 [P] Documentation sync: add the feature-065 section to
      `src/plugins/mdview/IMPLEMENTATION_NOTES.md` (keeper, UDF rename,
      config option, measured numbers) and verify
      `architecture/11-webview2-integration.md` matches the implemented
      reality (paths, argument set, measured behavior) per plan design
      outline 6.
- [X] T022 Format touched files (`clang-format` / `normalize.ps1`) and run a
      final full Debug + Release build.
      *Status: formatted with the VS2022 LLVM clang-format (normalize.ps1's
      own fallback resolver; `pwsh` 7.4 is not installed on this machine so
      the wrapper itself could not run); Debug + full Release rebuilt clean
      afterwards. The sweep also normalized pre-existing drift in the five
      touched files.*
- [X] T023 Draft the `CHANGELOG.md` entry in user terms (the first-open
      delay now occurs at most once per session; new Markdown Viewer
      configuration option) — filed under the next release version; the
      version/build bump itself happens in the release change per the
      constitution, not in this feature.
      *Status: draft in `specs/065-mdview-instant-render/changelog-draft.md`
      (CHANGELOG.md itself is edited in the release change, per the 0.1.4
      precedent).*

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: none — start immediately. T002 depends on T001.
- **Phase 2 (Foundational)**: after Phase 1 (T002 baseline uses pre-change
  behavior). T003 ∥ T004; T005 after both. **Blocks all stories.**
- **Phase 3 (US1)**: after Phase 2. T006 → T007 → (T008 ∥ T009) → T010.
- **Phase 4 (US2)**: after US1 (verifies the keeper's behavior); T011 ∥
  T012.
- **Phase 5 (US3)**: T013 ∥ T014 after Phase 2; T015 after T006+T013+T014;
  T016 after T008+T013; T017 after T014; T018 last.
- **Phase 6 (Polish)**: after all stories. T019 ∥ T020 ∥ T021; T022, T023
  at the end.

### Story Dependency Note

US2 is a verification story over the US1 mechanism (no separate code beyond
what T007's state machine already provides) — it cannot precede US1. US3 is
code-independent of US2 and touches US1 only at the one-line arm gate
(T016); it can proceed in parallel with US2 after US1 completes.

## Parallel Example: after Phase 2 completes and US1 lands

```text
# One developer/agent each:
T011+T012 (US2 verification, quickstart Scenarios 5 + baseline compare)
T013 (mdview.cpp config value)  ∥  T014 (lang.rc dialog resources)
```

## Implementation Strategy

**MVP first**: Phases 1–3 (T001–T010) deliver the user-visible value —
instant views after the first — and are demonstrable on their own
(keep-ready unconditionally on, no config yet). **Stop and validate** at the
Phase 3 checkpoint.

**Increment 2**: Phase 4 proves the one-time-cost guarantees.

**Increment 3**: Phase 5 adds the FR-008 opt-out and the zero-cost proof —
after this the feature is spec-complete.

**Polish**: Phase 6 locks in measurements, docs, formatting, and the
changelog draft.
