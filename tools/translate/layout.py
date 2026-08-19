"""Widens controls whose translated text no longer fits.

Translations are routinely longer than their English source -- Czech, German and
Romanian especially -- so a control sized for English clips the translation.
The Translator's own validator reports this (``--check-layout``), but reporting
several hundred findings for a human to fix by hand is not a plausible workflow.

This pass fixes the easy majority automatically and leaves the rest to a human:

* only **left-aligned text controls** are touched (statics, checkboxes, radio
  buttons, group boxes) -- these render left-to-right from a fixed origin, so
  growing them rightwards is safe;
* a control is grown only into **genuinely free space**: the gap to the nearest
  neighbour whose vertical extent overlaps it, or to the dialog's right margin;
* nothing is ever moved, shrunk, or grown beyond the dialog.

Width is estimated, not measured: the real width depends on the dialog font and
DPI, which are not available here. The estimate is deliberately generous, and
anything it cannot satisfy is left alone for ``--check-layout`` to report.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path

from .slt import Row, Section
from .validate import split_shortcut

#: Right-hand margin kept clear inside the dialog, in dialog units.
MARGIN = 4

#: Average glyph advance in dialog units for the 8pt MS Shell Dlg the product
#: uses. A dialog-unit x is a quarter of the average character width by
#: definition, so ~4 is the baseline; the fudge covers accented and wide glyphs.
UNITS_PER_CHAR = 4.05

#: CJK glyphs are full-width -- roughly double.
UNITS_PER_WIDE_CHAR = 7.6

#: Extra units a radio button or checkbox spends before its text begins: the
#: glyph itself plus the gap to the label (feature 063). Without it every
#: translated radio caption was judged "fits" while its first word was clipped.
GLYPH_ALLOWANCE = 12

#: What a closed drop-down combobox actually occupies vertically. Its resource
#: ``cy`` is the DROPPED-list height (often 100+), and taking that literally
#: turned every dropdown into a full-height wall in the free-space scan below,
#: blocking the widening of everything to its lower left (feature 063).
CLOSED_DROPDOWN_CY = 14


def estimate_width(text: str) -> int:
    """Rough rendered width of ``text`` in dialog units."""
    body, _ = split_shortcut(text)
    # The accelerator marker is not drawn, and "&&" draws as one character.
    body = body.replace("&&", "\x00").replace("&", "").replace("\x00", "&")
    for esc in ("\\n", "\\r", "\\t"):
        body = body.replace(esc, "")
    narrow = sum(1 for c in body if ord(c) < 0x1100)
    wide = len(body) - narrow
    return int(narrow * UNITS_PER_CHAR + wide * UNITS_PER_WIDE_CHAR) + 2


@dataclass
class Widening:
    dialog: int
    control: int
    old_cx: int
    new_cx: int
    text: str


@dataclass(frozen=True)
class ControlClasses:
    """Per-dialog control classification from a module's master ``.rc``.

    The ``.slt`` format carries no control classes, but two widening decisions
    need them (feature 063): ``checkable`` controls (radio/checkbox) render a
    glyph before the text and need :data:`GLYPH_ALLOWANCE` on top of the text
    estimate; ``dropdown`` comboboxes store their dropped-list height as ``cy``
    and must not wall off the free-space scan (:data:`CLOSED_DROPDOWN_CY`).
    Both maps key ``dialog resource id -> control ids``.
    """

    checkable: dict[int, frozenset[int]] = field(default_factory=dict)
    dropdown: dict[int, frozenset[int]] = field(default_factory=dict)


_RC_DEFINE = re.compile(r"#define\s+(\w+)\s+(\d+)")
_RC_DIALOG = re.compile(r"^(\w+)\s+DIALOG(?:EX)?\b")
_RC_STMT = re.compile(
    r"^\s*(CONTROL|LTEXT|RTEXT|CTEXT|EDITTEXT|COMBOBOX|LISTBOX|PUSHBUTTON|"
    r"DEFPUSHBUTTON|GROUPBOX|ICON|AUTOCHECKBOX|AUTORADIOBUTTON|CHECKBOX|"
    r"RADIOBUTTON|STATE3|END|BEGIN)\b")
_RC_CHECKABLE_STYLE = re.compile(
    r"\bBS_(?:AUTO)?(?:RADIOBUTTON|CHECKBOX|3STATE)\b|\bBS_AUTO3STATE\b")
_RC_CHECKABLE_STMT = re.compile(
    r"^\s*(AUTOCHECKBOX|AUTORADIOBUTTON|CHECKBOX|RADIOBUTTON|STATE3)\b")


def _rc_control_id(stmt: str, defines: dict[str, int]) -> int | None:
    """The control id of one logical RC statement, or None."""
    # CONTROL "text",ID,... / AUTOCHECKBOX "text",ID,... / COMBOBOX ID,...
    m = re.match(r'^\s*\w+\s+(?:"(?:[^"]|"")*"\s*,\s*)?(\w+)\s*,', stmt)
    if m is None:
        return None
    token = m.group(1)
    if token.isdigit():
        return int(token)
    return defines.get(token)


def load_control_classes(rc_path: Path, rh_path: Path) -> ControlClasses:
    """Parse a module's master ``lang.rc`` (+ ``lang.rh``) for control classes.

    Lenient by design: anything unparsed simply yields no classification, and
    :func:`widen` then behaves exactly as before feature 063 for that control.
    """
    if not rc_path.is_file() or not rh_path.is_file():
        return ControlClasses()
    defines: dict[str, int] = {}
    for m in _RC_DEFINE.finditer(rh_path.read_text(encoding="utf-8-sig", errors="replace")):
        defines[m.group(1)] = int(m.group(2))

    checkable: dict[int, set[int]] = {}
    dropdown: dict[int, set[int]] = {}
    dialog: int | None = None
    depth = 0
    stmts: list[str] = []
    for line in rc_path.read_text(encoding="utf-8-sig", errors="replace").splitlines():
        dm = _RC_DIALOG.match(line)
        if dm is not None:
            dialog = defines.get(dm.group(1))
            depth = 0
            continue
        if dialog is None:
            continue
        stripped = line.strip()
        if stripped == "BEGIN" or stripped == "{":
            depth += 1
            continue
        if stripped == "END" or stripped == "}":
            depth -= 1
            if depth <= 0:
                dialog = None
            continue
        if depth < 1:
            continue
        # join wrapped statements: a line not starting a new statement continues
        # the previous one
        if _RC_STMT.match(line) or not stmts:
            stmts.append(line)
        else:
            stmts[-1] += " " + stripped
        stmt = stmts[-1]
        cid = _rc_control_id(stmt, defines)
        if cid is None:
            continue
        is_checkable = (_RC_CHECKABLE_STMT.match(stmt) is not None or
                        ('"Button"' in stmt and _RC_CHECKABLE_STYLE.search(stmt) is not None))
        is_dropdown = ((stmt.lstrip().startswith("COMBOBOX") or '"ComboBox"' in stmt) and
                       re.search(r"\bCBS_DROPDOWN(?:LIST)?\b", stmt) is not None)
        if is_checkable:
            checkable.setdefault(dialog, set()).add(cid)
        if is_dropdown:
            dropdown.setdefault(dialog, set()).add(cid)

    return ControlClasses(
        checkable={d: frozenset(s) for d, s in checkable.items()},
        dropdown={d: frozenset(s) for d, s in dropdown.items()},
    )


def _overlaps_vertically(a: Row, b: Row, b_height_cap: int | None = None) -> bool:
    ay, ah = a.numbers[2], a.numbers[4]
    by, bh = b.numbers[2], b.numbers[4]
    if b_height_cap is not None and bh > b_height_cap:
        bh = b_height_cap
    return ay < by + bh and by < ay + ah


def widen(section: Section, classes: ControlClasses | None = None) -> list[Widening]:
    """Grow clipped controls in one dialog, in place. Returns what changed."""
    if not section.rows or section.kind != "DIALOG":
        return []

    checkable = classes.checkable.get(section.number, frozenset()) if classes else frozenset()
    dropdown = classes.dropdown.get(section.number, frozenset()) if classes else frozenset()

    dialog_cx = section.rows[0].numbers[0]
    controls = section.rows[1:]
    changes: list[Widening] = []

    for row in controls:
        if len(row.numbers) != 6:  # not a control row
            continue
        cid, x, y, cx, cy = row.numbers[:5]
        if not row.text.strip():
            continue

        needed = estimate_width(row.text)
        if cid in checkable:
            needed += GLYPH_ALLOWANCE
        if needed <= cx:
            continue

        # A control only ever grows into space nothing else occupies. A dropdown
        # combobox blocks only its closed height, not its dropped-list height.
        limit = dialog_cx - MARGIN
        for other in controls:
            if other is row or len(other.numbers) != 6:
                continue
            ox = other.numbers[1]
            cap = CLOSED_DROPDOWN_CY if other.numbers[0] in dropdown else None
            if ox > x and _overlaps_vertically(row, other, cap):
                limit = min(limit, ox - 2)

        new_cx = min(needed, limit - x)
        if new_cx > cx:
            row.numbers[3] = new_cx
            changes.append(Widening(section.number, cid, cx, new_cx, row.text))

    return changes


# ---------------------------------------------------------------------------
# Accelerator de-duplication
# ---------------------------------------------------------------------------

def _accel_index(body: str) -> int | None:
    """Index of the accelerator letter (the char after a single ``&``)."""
    i = 0
    while i < len(body) - 1:
        if body[i] == "&":
            if body[i + 1] == "&":
                i += 2
                continue
            return i + 1
        i += 1
    return None


def _candidates(body: str, taken: set[str]) -> list[int]:
    """Positions of usable replacement letters, word-initial ones first.

    ASCII letters and digits come before accented ones: an accelerator the user
    cannot type on their own keyboard layout is not much of an accelerator.
    """
    plain = body.replace("&", "")
    word_start: list[int] = []
    inner: list[int] = []
    prev_sep = True
    for i, ch in enumerate(plain):
        if not ch.isalnum():
            prev_sep = True
            continue
        if ch.lower() not in taken:
            (word_start if prev_sep else inner).append(i)
        prev_sep = False

    def rank(i: int) -> tuple[int, int]:
        ch = plain[i]
        return (0 if ch.isascii() else 1, i)

    return sorted(word_start, key=rank) + sorted(inner, key=rank)


def dedupe_accelerators(section: Section, frozen_rows: set[int] | None = None) -> list[str]:
    """Give every accelerated row in one section a distinct accelerator letter.

    Windows cycles between controls sharing an accelerator instead of activating
    one, so a duplicate is a real keyboard-navigation defect (spec SC-006) - and
    machine translation produces them routinely, because it picks each word
    without seeing the rest of the dialog. Moving the marker onto another letter
    of the text the translator already chose changes no wording, so it is done
    automatically.

    The assignment is solved for the whole section at once (bipartite matching,
    Kuhn's algorithm) rather than row by row: with a dialog of ~20 accelerated
    controls the later rows often have no free letter left, while an earlier one
    could have moved. Each row prefers its current letter, then the initial of
    each word, then any other letter; ASCII before accented, since an
    accelerator the user cannot type is no accelerator. Rows that end up
    unmatched keep what they had and are reported by
    :func:`validate.duplicate_accelerators`.

    Rows listed in ``frozen_rows`` (indices into ``section.rows``) are never
    modified: a human translator's accelerator choice is part of the human
    translation and moving it would violate the do-not-touch guarantee
    (feature 055, FR-003). Their letters become fixed obstacles the movable
    (machine-translated) rows are deduplicated around; a conflict between two
    frozen rows is left alone and reported by
    :func:`validate.duplicate_accelerators`.

    Returns a description of each changed row, for the coverage report.
    """
    frozen_rows = frozen_rows or set()
    frozen_letters: set[str] = set()
    for ri, row in enumerate(section.rows):
        if ri not in frozen_rows:
            continue
        body, _tail = split_shortcut(row.text)
        at = _accel_index(body)
        if at is not None:
            frozen_letters.add(body[at].lower())

    rows: list[tuple[Row, str, str, list[int], str]] = []
    for ri, row in enumerate(section.rows):
        if ri in frozen_rows:
            continue
        body, tail = split_shortcut(row.text)
        at = _accel_index(body)
        if at is None:
            continue
        plain = body.replace("&", "", 1)
        # Position of the current accelerator inside 'plain' (one '&' removed).
        rows.append((row, plain, tail, _candidates(body, frozen_letters), at - 1))

    # letter -> row index it is currently assigned to
    owner: dict[str, int] = {}
    assigned: dict[int, int] = {}  # row index -> position in its plain text

    def try_assign(idx: int, blocked: set[str]) -> bool:
        row, plain, _tail, spots, at_plain = rows[idx]
        # Preference: keep exactly the letter the translation already marks (so a
        # section with no duplicates is left byte-for-byte alone, and a
        # hand-curated choice survives), then other spellings of the same
        # letter, then any free letter.
        current = plain[at_plain].lower() if at_plain < len(plain) else ""
        order = [at_plain] if at_plain < len(plain) else []
        order += [p for p in range(len(plain))
                  if p != at_plain and plain[p].lower() == current]
        order += [p for p in spots if p not in order]
        for pos in order:
            letter = plain[pos].lower()
            if letter in blocked or letter in frozen_letters:
                continue
            # The visited set is shared across the whole augmentation attempt
            # (standard Kuhn), not copied per branch: a per-branch copy re-explores
            # every letter permutation and turns a section with more accelerated
            # rows than free letters -- salamand's large menus -- from
            # milliseconds into hours (feature 055).
            blocked.add(letter)
            held = owner.get(letter)
            if held is None or try_assign(held, blocked):
                owner[letter] = idx
                assigned[idx] = pos
                return True
        return False

    for idx in range(len(rows)):
        try_assign(idx, set())

    changed: list[str] = []
    for idx, (row, plain, tail, _spots, _at) in enumerate(rows):
        pos = assigned.get(idx)
        if pos is None:
            continue
        new_text = plain[:pos] + "&" + plain[pos:] + tail
        if new_text != row.text:
            changed.append(f"{row.entry_id}: {row.text!r} -> {new_text!r}")
            row.text = new_text
    return changed
