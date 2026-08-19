"""Refreshes control geometry in committed ``.slt`` files from a template.

Sibling of :mod:`translate.merge`, and deliberately much smaller: it copies the
*numbers* of every dialog row from the English template and leaves every quoted
*text* exactly as committed.

Why this exists as its own tool. Per-language control geometry lives in the
committed ``.slt``, not in the dialog template -- ``-quiet-import-slt`` writes
those coordinates into the built module, so the ``.slt`` overrides ``lang.rc2``
for every language. After a template is re-laid-out, each language therefore
needs its geometry refreshed. ``merge`` does that as a side effect, but it also
re-translates every entry that is still an English fallback, which changes text.
When the intent is *only* the layout fix, that is both unwanted and unnecessary:
this pass is offline, deterministic, and cannot alter a single character of
translated text.

The result is what ``merge`` would produce geometrically, so a later ``merge``
run is idempotent with respect to layout.

Usage::

    python -m translate.relayout --module sftp                 # all enabled languages
    python -m translate.relayout --module sftp --dry-run
    python -m translate.relayout --module sftp --language czech
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import REPO_ROOT
from .config import ConfigError, load_enabled_modules, load_languages
from .layout import ControlClasses, load_control_classes, widen
from .slt import load


def default_templates_dir() -> Path:
    build = Path(REPO_ROOT) / "build" / "tandemcommander" / "translator" / "templates"
    return build


def relayout_file(template_path: Path, target_path: Path, write: bool,
                  classes: ControlClasses | None = None) -> tuple[int, int, list[str]]:
    """Copy dialog geometry from the template into the target ``.slt``.

    Returns ``(rows_changed, controls_widened, problems)``. Text is never read
    from the template, so it cannot be modified here. Nothing is written when a
    problem is reported, so a structural mismatch leaves the file untouched.
    """
    template = load(template_path)
    target = load(target_path)

    tpl_sections = {s.key: s for s in template.sections}
    changed = 0
    widened = 0
    problems: list[str] = []

    for section in target.sections:
        if section.kind != "DIALOG":
            continue  # menus and string tables carry no geometry
        tpl = tpl_sections.get(section.key)
        if tpl is None:
            problems.append(f"{target_path.name}: no template counterpart for {section.key}")
            continue
        if len(tpl.rows) != len(section.rows):
            # a structural change: this tool only refreshes numbers, so refuse
            problems.append(
                f"{target_path.name}: {section.key} has {len(section.rows)} rows, "
                f"template has {len(tpl.rows)} -- run merge instead"
            )
            continue
        for tpl_row, row in zip(tpl.rows, section.rows):
            if len(tpl_row.numbers) != len(row.numbers):
                problems.append(f"{target_path.name}: {section.key} row shape differs")
                break
            # every field except the trailing state is geometry (or the control
            # id, which must match the template anyway)
            new_numbers = list(tpl_row.numbers[:-1]) + [row.numbers[-1]]
            if new_numbers != row.numbers:
                row.numbers = new_numbers
                changed += 1
        widened += len(widen(section, classes))

    if problems:
        return 0, 0, problems
    if write and (changed or widened):
        target.write(target_path)
    return changed, widened, []


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--module", required=True, help="module whose dialogs were re-laid-out")
    ap.add_argument("--language", action="append", dest="languages",
                    help="restrict to this language folder (repeatable)")
    ap.add_argument("--enabled-only", action="store_true",
                    help="skip languages switched off in languages.cfg")
    ap.add_argument("--dry-run", action="store_true", help="report, write nothing")
    ap.add_argument("--templates", type=Path, default=None,
                    help="template directory (default: the build output)")
    args = ap.parse_args(argv)

    templates_dir = args.templates or default_templates_dir()
    template_path = templates_dir / f"{args.module}.slt"
    if not template_path.is_file():
        print(f"error: no template at {template_path}", file=sys.stderr)
        print("       run: src\\vcxproj\\build_langs.cmd --export-templates --module "
              f"{args.module}", file=sys.stderr)
        return 1

    try:
        modules = {m.name: m for m in load_enabled_modules()}
        languages = load_languages(include_disabled=True)
    except ConfigError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    if args.module not in modules:
        print(f"error: {args.module} is not an enabled module", file=sys.stderr)
        return 1
    # control classes from the module's master .rc: radios/checkboxes get the
    # glyph allowance, dropdowns stop walling off the free-space scan (063)
    rh_path = modules[args.module].rh_path
    classes = load_control_classes(rh_path.with_name("lang.rc"), rh_path)
    if args.languages:
        wanted = set(args.languages)
        languages = [l for l in languages if l.folder in wanted]
        if not languages:
            print("error: no matching language", file=sys.stderr)
            return 1
    elif args.enabled_only:
        languages = [l for l in languages if l.enabled]
    # Default: every registered language, like `rebrand` and unlike `merge`. This
    # pass spends nothing and translates nothing, and leaving a disabled language
    # on stale geometry would hand whoever re-enables it a broken layout.

    total_changed = 0
    failures: list[str] = []
    print(f"template: {template_path}" + ("  (dry run -- nothing written)" if args.dry_run else ""))
    for language in languages:
        target = language.directory / f"{args.module}.slt"
        if not target.is_file():
            print(f"  {language.folder:20s} (no {args.module}.slt)")
            continue
        changed, widened, problems = relayout_file(template_path, target, not args.dry_run, classes)
        if problems:
            failures.extend(problems)
            print(f"  {language.folder:20s} REFUSED")
            for p in problems:
                print(f"      {p}")
            continue
        total_changed += changed
        print(f"  {language.folder:20s} {changed:4d} row(s) re-geometried, {widened} widened")

    print(f"\nrows changed: {total_changed}")
    if failures:
        print(f"REFUSED for {len(failures)} section(s) -- nothing written for those files")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
