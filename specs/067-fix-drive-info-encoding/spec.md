# Feature Specification: Fix Garbled Numbers in Drive Information Dialog

**Feature Branch**: `067-fix-drive-info-encoding`
**Created**: 2026-08-24
**Status**: Draft
**Input**: User description: "V okně s informacemi zobrazení informací o jednotce (viz klávesová zkratka CTRL+F1) se špatně zobrazují čísla u volného, resp. využitého místa. Je tam nějaká chyba v kódování - viz obrázek ./temp/informace_o_jednotce.png"

**Evidence**: [informace_o_jednotce.png](informace_o_jednotce.png) — screenshot of the
defect on a Czech Windows 11 system with the Czech UI language active.

## Problem Summary

In the Drive Information dialog (Ctrl+F1), the exact byte counts for
**Used space** ("Využité místo"), **Free space** ("Volné místo") and
**Capacity** ("Kapacita") display a stray `Â` character in front of every
digit-group separator, e.g.:

| Shown today (wrong) | Expected |
|---------------------|----------|
| `967Â 709Â 523Â 968 bajtů` | `967 709 523 968 bajtů` |
| `32Â 476Â 786Â 688 bajtů` | `32 476 786 688 bajtů` |
| `1Â 000Â 186Â 310Â 656 bajtů` | `1 000 186 310 656 bajtů` |

Other values in the same dialog are unaffected: the cluster count
(`244 186 111`) and the rounded sizes (`901 GB`, `30,2 GB`) display correctly.

The defect appears only when **both** of these hold: the Windows locale uses a
non-ASCII digit-grouping separator (Czech, Russian, French and many others use
a no-break space), **and** the localized text combined with the number contains
non-ASCII characters (e.g. Czech "bajtů"). English UI with an ASCII separator
never shows it — which is why the defect went unnoticed. It is a
**text-composition defect of the same class as feature 052** (strings of mixed
provenance combined into one displayed value); the value itself is computed
correctly, only its on-screen rendering is garbled.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Correct byte counts in the Drive Information dialog (Priority: P1)

A user on a Czech (or any non-English) Windows system selects a drive in a
panel and presses Ctrl+F1. The dialog shows the used space, free space and
capacity as exact byte counts. All three numbers must read naturally — digit
groups separated by the locale's separator, followed by the correctly spelled
localized unit word — with no stray characters anywhere in the value.

**Why this priority**: This is the reported defect, visible in a
frequently used informational dialog. Garbled numbers look broken and erode
trust in the correctness of the values themselves.

**Independent Test**: On a Czech Windows system with Czech UI, open Ctrl+F1
for any local NTFS drive and compare the three byte counts character by
character with the same drive's Properties dialog in Windows Explorer.

**Acceptance Scenarios**:

1. **Given** a Czech Windows system with the Czech UI language active,
   **When** the user opens the Drive Information dialog for a local drive,
   **Then** Used space, Free space and Capacity show digit groups separated
   only by the locale separator (rendered as a space), with no `Â` or any
   other unexpected character.
2. **Given** the same dialog, **When** the user reads the unit word after each
   number, **Then** it is spelled correctly including diacritics (e.g.
   "bajtů"), and the grammatical plural form matches the number.
3. **Given** an English UI on a system with an ASCII digit-grouping separator
   (e.g. `1,000,186,310,656 bytes`), **When** the user opens the same dialog,
   **Then** the output is unchanged from today (already correct — no
   regression).

---

### User Story 2 - Correct grouped numbers on every product surface (Priority: P2)

The same number-plus-localized-text composition is used across the product —
directory size dialogs, panel status information, progress windows, plugin
dialogs. A user working in any shipped UI language must never see garbled
digit-group separators anywhere a large number is displayed.

**Why this priority**: The defect lives in shared formatting, not in the one
dialog; fixing only Ctrl+F1 would leave the identical symptom on other
surfaces. Product precedent (features 052, 058, 066) is to fix the defect
class at its root, product-wide.

**Independent Test**: With Czech UI, exercise the other surfaces that display
exact byte counts or large counts (e.g. directory size / occupied-space
dialogs, file counts in confirmation dialogs) and verify no stray characters
around digit-group separators.

**Acceptance Scenarios**:

1. **Given** Czech UI, **When** any dialog or status area displays an exact
   byte count or item count with digit grouping, **Then** the separator
   renders as the locale prescribes with no stray characters.
2. **Given** any of the 8 shipped languages, **When** the same surfaces are
   exercised, **Then** no garbled separators appear in any of them.

---

### User Story 3 - Robust across Windows separator settings (Priority: P3)

Windows lets the digit-grouping separator be customized and varies it by
locale — comma, period, space, no-break space, narrow no-break space,
apostrophe, or a user-defined string of several characters. Whatever the
user's separator setting, grouped numbers must render exactly that separator.

**Why this priority**: Lower frequency than the shipped-locale cases above,
but the same fix must not hard-code assumptions that break on less common
separators.

**Independent Test**: Change the digit-grouping separator in Windows regional
settings (e.g. to an apostrophe, then to a multi-character string), restart
the application, and verify the Drive Information numbers use exactly the
configured separator.

**Acceptance Scenarios**:

1. **Given** a locale whose separator is a non-ASCII character (no-break
   space, narrow no-break space, typographic apostrophe), **When** grouped
   numbers are displayed, **Then** the separator renders correctly.
2. **Given** a user-customized separator up to the maximum length Windows
   permits, **When** grouped numbers are displayed, **Then** the full
   separator string appears between groups, unmangled.

---

### Edge Cases

- Numbers below 1 000 (no separator needed) must remain untouched.
- Singular/plural unit forms (Czech "1 bajt" / "2 bajty" / "5 bajtů") must
  each render correctly — the plural-form selection must not be disturbed by
  the fix.
- Fallback texts used when no language module is loaded (built-in English
  strings) must keep working.
- Values produced by the shared formatting for plugins (archivers, FTP, etc.)
  must render correctly in plugin dialogs too.
- The rounded companion values ("901 GB", "30,2 GB") and the decimal separator
  in them are correct today and must stay correct.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The Drive Information dialog MUST display Used space, Free
  space and Capacity byte counts with the user's locale digit-grouping
  separator rendered exactly — no stray characters before, after, or instead
  of the separator — in every shipped UI language.
- **FR-002**: The localized unit word displayed with each number MUST render
  correctly, including diacritics, with the grammatically correct plural form
  for the value.
- **FR-003**: The correction MUST be made in the shared number/size
  formatting so that every **core-application** surface composing a grouped
  number with localized text is fixed, not just the reported dialog. All
  surfaces sharing the formatting path — including plugin-side consumers —
  MUST be enumerated and verified as part of the work; plugin-internal
  rendering defects found by that enumeration are recorded as documented
  follow-up work rather than fixed here, because fixing them from the shared
  layer was shown to introduce regressions on plugin surfaces (see
  research.md R3/R6). Plugin-visible output MUST remain byte-identical.
- **FR-004**: Rendering MUST be correct for any digit-grouping separator
  Windows can supply — ASCII or non-ASCII, single- or multi-character up to
  the length Windows permits — and MUST fall back safely (plain space) if the
  separator cannot be obtained.
- **FR-005**: Displays that are correct today MUST remain byte-for-byte
  identical: pure numeric values (e.g. cluster count), rounded sizes
  ("901 GB"), and all output on English UI with ASCII separators.
- **FR-006**: The fix MUST be display-only: no change to computed values, no
  change to stored configuration, no migration.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a Czech Windows system with Czech UI, the three byte counts
  in the Drive Information dialog match Windows Explorer's rendering of the
  same values character for character (modulo Explorer's wording) — zero
  unexpected characters, verified against the evidence screenshot scenario.
- **SC-002**: A sweep of the affected surfaces across all 8 shipped languages
  finds 0 garbled digit-group separators.
- **SC-003**: The full automated test suite passes, and new automated
  coverage exists that fails on the pre-fix behavior (grouped number combined
  with non-ASCII localized text under a non-ASCII separator) and passes after.
- **SC-004**: Output on English UI with an ASCII separator is unchanged
  (verified by comparison before/after the fix).

## Assumptions

- The defect is display-only. The underlying values (used/free/capacity) are
  computed correctly — confirmed by the correct rounded GB values shown beside
  the garbled byte counts.
- English UI with ASCII locale separators was never affected; preserving its
  output byte-for-byte is a hard constraint, not a goal to trade off.
- In-scope surfaces are those that compose grouped numbers with localized
  text through the product's shared formatting **in the core application**.
  Unrelated encoding defects discovered on other surfaces during the work —
  including pre-existing defects inside plugins — are recorded but fixed
  separately (consistent with how features 052/058/066 were scoped). The
  plugin API's output bytes are deliberately frozen at today's behavior in
  this feature.
- The 3 disabled languages (Simplified Chinese, Russian, Ukrainian) are not
  release-verification targets, but the fix must not depend on language: it
  must be correct by construction for any language module.
- Baseline platform is Windows 11 per the constitution; no behavior differences
  by Windows edition are expected for this fix.
