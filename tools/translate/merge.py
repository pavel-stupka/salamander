"""Builds committed translation source from template + legacy + DeepL.

For one (module, language) pair:

1. the **template** exported from the current ``english.slg`` defines the
   structure -- it is reproduced exactly, because ``.slt`` import is positional;
2. each entry is filled from the **legacy** translation where the ID matches;
3. whatever is left is **machine-translated**, together with a description of
   *where* each string is used (see :mod:`translate.uicontext`) -- without it a
   bare ``Host:`` came back as the Czech word for a talk-show host;
4. everything is validated, rebranded, and written to
   ``translations/<language>/<module>.slt`` plus an ``.origin`` sidecar.

Quota matters (the DeepL free tier bills characters sent), so gaps are collected
across *all* modules of a language, deduplicated, translated once, and then
distributed. Roughly 10% of the raw volume is repeats -- "OK", "Cancel",
"&Yes" and friends appear in almost every module.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

from . import TRANSLATIONS_DIR
from .config import (
    ConfigError,
    Language,
    Module,
    load_enabled_modules,
    load_languages,
)
from .layout import dedupe_accelerators, load_control_classes, widen
from .deepl import TARGET_CODES, Client, DeepLError, load_key
from .match import CAPTION_KEY, entry_key, index_entries, match
from .rebrand import find_residue, rebrand
from .slt import Section, SltFile, load
from .uicontext import build_contexts, load_symbols
from .validate import check, duplicate_accelerators, repair

DEFAULT_TEMPLATES = "build/tandemcommander/translator/templates"

HUMAN, MACHINE, FALLBACK, SKIP = "human", "machine", "english_fallback", "skip"


@dataclass
class Coverage:
    """Per-(language, module) outcome -- the coverage report of spec FR-015."""

    language: str
    module: str
    total: int = 0
    human: int = 0
    machine: int = 0
    fallback: int = 0
    skip: int = 0
    discarded: int = 0
    rebranded: int = 0
    widened: int = 0
    reaccel: int = 0
    invalid: list[str] = field(default_factory=list)
    dup_accel: list[str] = field(default_factory=list)

    @property
    def translatable(self) -> int:
        return self.total - self.skip

    def pct_human(self) -> int:
        return 100 * self.human // self.translatable if self.translatable else 0


def load_origin(
    language: Language,
    module: Module,
    template: SltFile | None = None,
    redo_accelerators: bool = False,
    redo_machine: bool = False,
) -> dict[str, str]:
    """Previous run's provenance, if any. Absent on the first run.

    ``redo_accelerators`` demotes previously machine-translated entries whose
    English carries an accelerator back to a gap, so they are translated again.
    Needed once, because the first implementation protected ``&`` as an ignore
    tag -- which split the word it marks and produced fragments like
    ``"Zberegti p &assphrase"``.

    ``redo_machine`` demotes *every* machine-translated entry (a superset of
    ``redo_accelerators``), so the whole machine population is re-sent -- used
    to refresh context-less translations after the context descriptions
    improved (feature 055). Human and skip entries are never demoted.
    """
    path = language.directory / f"{module.name}.origin"
    if not path.is_file():
        return {}
    try:
        origin = json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return {}

    if redo_machine:
        return {k: (FALLBACK if v == MACHINE else v) for k, v in origin.items()}

    if redo_accelerators and template is not None:
        single_amp = re.compile(r"(?<!&)&(?!&)")
        for section in template.sections:
            for i, row in enumerate(section.rows):
                k = entry_key(section, row, i)
                kk = f"{k[0]}:{k[1]}:{k[2]}"
                if origin.get(kk) == MACHINE and single_amp.search(row.text):
                    origin[kk] = FALLBACK
    return origin


# NOTE: an earlier version carried control geometry over from the legacy
# translation, to keep the resizing a human translator had done. It was removed,
# for two reasons that compound:
#
#   * the legacy layout describes the 4.0 dialog. IDD_ABOUT is 299x184 in both
#     versions, but today's has twelve controls where 4.0 had nine -- a second
#     copyright line was inserted and everything below moved down 11 units.
#     Carrying the old coordinates drew the web-address control through the
#     authors line.
#   * merging *overwrites the file it reads*, so on the next run the previous
#     output is the "legacy" input. Any bad geometry is indistinguishable from
#     deliberate translator work and perpetuates itself -- tightening the
#     heuristic did not help, because by then the two layouts genuinely matched.
#
# Geometry now always starts from the template (the current English layout) and
# is widened from the translated text by translate.layout. That is deterministic
# and idempotent: the same .slt always produces the same coordinates, whatever
# ran before.


OVERRIDES_FILE = "ui-overrides.json"


def load_overrides(language: Language, module: Module) -> dict[str, str]:
    """Hand-curated texts for one (language, module), if any.

    Lives in ``translations/ui-overrides.json``:

    .. code-block:: json

        {"sftp": {"czech": {"IDT_HOSTADDRESS": "&Adresa:", "#&New": "&Nová"}}}

    Keys are either a resource symbol from the module's ``.rh`` files, or
    ``#`` followed by the exact English text (for entries whose symbol is
    shared or absent). This exists because context cannot fix everything: a
    lone ``&New`` still needs a gender in Czech, and only a human knows which
    noun it agrees with.
    """
    path = TRANSLATIONS_DIR / OVERRIDES_FILE
    if not path.is_file():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except ValueError as e:
        raise ConfigError(f"{path}: {e}") from e
    return dict(data.get(module.name, {}).get(language.folder, {}))


def build_slt(
    template: SltFile,
    legacy: SltFile | None,
    language: Language,
    translations: dict[tuple[str, str], str],
    cov: Coverage,
    prev_origin: dict[str, str] | None = None,
    contexts: dict[tuple[str, int], str] | None = None,
    overrides: dict[str, str] | None = None,
    module: Module | None = None,
) -> tuple[SltFile, dict[str, str]]:
    """Produce the merged ``.slt`` and its per-entry provenance map.

    ``contexts`` maps a section to the description sent to the translator, and
    is what the ``translations`` keys were built from. ``overrides`` holds
    hand-curated texts keyed by ``<SYMBOL>`` or ``#<english>`` (see
    :func:`load_overrides`); they win over anything the engine produced.
    """
    plan, stats = match(template, legacy, prev_origin)
    cov.total, cov.skip, cov.discarded = stats.total, stats.untranslatable, stats.discarded

    symbols: dict[int, str] = load_symbols(module) if module is not None else {}
    # control classes for the widening pass (feature 063): radio/checkbox glyph
    # allowance + dropdowns no longer wall off the free-space scan
    classes = (load_control_classes(module.rh_path.with_name("lang.rc"), module.rh_path)
               if module is not None else None)

    legacy_index = index_entries(legacy) if legacy is not None else {}
    legacy_sections = {s.key: s for s in legacy.sections} if legacy is not None else {}
    origin: dict[str, str] = {}

    out = SltFile(
        exportinfo=list(template.exportinfo),
        translation=list(template.translation),
        sections=[],
        relayout=list(template.relayout) if template.relayout is not None else None,
        has_bom=template.has_bom,
    )

    # [TRANSLATION] identity comes from languages.cfg. set_translation_value only
    # replaces keys the template already has -- HELPDIR and SLGINCOMPLETE exist
    # for the application module but not for plugins, and inventing them would
    # not match the module's own resources.
    out.set_translation_value("LANGID", str(language.langid))
    out.set_translation_value("AUTHOR", language.author)
    out.set_translation_value("WEB", language.web)
    out.set_translation_value("COMMENT", language.comment)
    out.set_translation_value("HELPDIR", language.helpdir)
    # Always empty: an empty SLGIncomplete is how a .slg declares itself a
    # complete translation (salamdr1.cpp:200), and a non-empty one makes the
    # main window pop up a "translation is incomplete" message box at startup
    # (WM_USER_SLGINCOMPLETE, mainwnd3.cpp:5928). Suppressing that popup is a
    # deliberate product decision; provenance is still tracked per entry in the
    # .origin sidecars and reported by the coverage table, so nothing is lost
    # for a reviewer -- only the startup interruption for the user.
    out.set_translation_value("SLGINCOMPLETE", "")

    for tpl_section in template.sections:
        section = Section(kind=tpl_section.kind, number=tpl_section.number)
        frozen_rows: set[int] = set()
        for i, tpl_row in enumerate(tpl_section.rows):
            key = entry_key(tpl_section, tpl_row, i)
            source, value = plan[key]
            numbers = list(tpl_row.numbers)

            if source == SKIP:
                text, kind = value, SKIP
                numbers[-1] = tpl_row.state
            elif source == MACHINE:
                # Carried over from an earlier machine-translation run.
                text, kind = value, MACHINE
                numbers[-1] = 1
                cov.machine += 1
            elif source == HUMAN:
                text, kind = value, HUMAN
                numbers[-1] = 1
                cov.human += 1
            else:  # gap
                english = value
                ctx = (contexts or {}).get((tpl_section.kind, tpl_section.number), "")
                candidate = translations.get((ctx, english))
                if candidate is None:
                    # A run that translated the text under another context (or
                    # with none) is still better than falling back to English.
                    candidate = next(
                        (v for (c, t), v in translations.items() if t == english), None
                    )
                if candidate:
                    candidate = repair(english, candidate)
                    verdict = check(english, candidate)
                    if verdict.ok:
                        text, kind = candidate, MACHINE
                        numbers[-1] = 1
                        cov.machine += 1
                    else:
                        text, kind = english, FALLBACK
                        numbers[-1] = 0
                        cov.fallback += 1
                        cov.invalid.append(f"{english[:60]!r}: {verdict}")
                else:
                    text, kind = english, FALLBACK
                    numbers[-1] = 0
                    cov.fallback += 1

            # Hand-curated override wins over engine output (and over a
            # carried-over machine entry): it is the only way to fix a label
            # the engine cannot get right from context alone.
            if overrides:
                symbol = symbols.get(tpl_row.entry_id) if tpl_row.entry_id is not None else None
                forced = None
                if symbol is not None and symbol in overrides:
                    forced = overrides[symbol]
                elif f"#{tpl_row.text}" in overrides:
                    forced = overrides[f"#{tpl_row.text}"]
                # Applied even when the engine happened to produce the same
                # text: the pin is a human decision, and recording it as
                # 'human' is what keeps it frozen for the accelerator dedup
                # and out of future --redo-machine populations.
                if forced is not None and (forced != text or kind != HUMAN):
                    if kind == MACHINE:
                        cov.machine -= 1
                    elif kind == FALLBACK:
                        cov.fallback -= 1
                    elif kind == HUMAN:
                        cov.human -= 1
                    text, kind = forced, HUMAN
                    numbers[-1] = 1
                    cov.human += 1

            rebranded = rebrand(text)
            if rebranded != text:
                cov.rebranded += 1
                text = rebranded

            section.rows.append(type(tpl_row)(numbers=numbers, text=text))
            if kind in (HUMAN, SKIP):
                # A human translation is untouchable in full, accelerator
                # included; a skip row reproduces the template. Neither may be
                # rearranged by the accelerator dedup below (feature 055).
                frozen_rows.add(len(section.rows) - 1)
            origin[f"{key[0]}:{key[1]}:{key[2]}"] = kind

        # Grow controls the translation outgrew, before the accelerator check.
        if section.kind == "DIALOG":
            cov.widened += len(widen(section, classes))

        # Windows cycles between controls that share an accelerator instead of
        # activating one, so a duplicate within a dialog or menu is a real
        # keyboard-navigation defect (spec SC-006), and machine translation
        # produces them routinely - it picks each word without seeing the rest
        # of the dialog. Moving the marker to another letter of the same word
        # changes no wording, so it is done automatically; whatever cannot be
        # resolved that way is still reported for a human to reword.
        if section.kind in ("DIALOG", "MENU", "STRINGTABLE"):
            cov.reaccel += len(dedupe_accelerators(section, frozen_rows))
            dups = duplicate_accelerators([r.text for r in section.rows])
            if dups:
                cov.dup_accel.append(
                    f"{section.kind} {section.number}: {', '.join(dups)}"
                )

        out.sections.append(section)

    return out, origin


def collect_gaps(
    templates: dict[str, SltFile],
    language: Language,
    modules: list[Module],
    redo_accelerators: bool = False,
    retranslate: bool = False,
    redo_machine: bool = False,
    contexts_by_module: dict[str, dict] | None = None,
) -> list[tuple[str, str]]:
    """``(context, english)`` pairs needing translation for this language.

    The same English text appearing in two different dialogs yields two pairs:
    that is deliberate, because the right word can differ (a "Host:" field
    label and a "Host" column heading are not the same thing).
    """
    pairs: set[tuple[str, str]] = set()
    for module in modules:
        template = templates[module.name]
        contexts = (contexts_by_module or {}).get(module.name) \
            or build_contexts(module, template)
        legacy_path = language.directory / f"{module.name}.slt"
        legacy = load(legacy_path) if legacy_path.is_file() else None
        plan, _ = match(
            template,
            None if retranslate else legacy,
            None if retranslate
            else load_origin(language, module, template, redo_accelerators,
                             redo_machine),
        )
        for key, (src, value) in plan.items():
            if src != "gap":
                continue
            pairs.add((contexts.get((key[0], key[1]), ""), value))
    return sorted(pairs)


def run(
    languages: list[Language],
    modules: list[Module],
    templates_dir: Path,
    dry_run: bool,
    key_file: Path | None,
    budget: int | None,
    redo_accelerators: bool = False,
    retranslate: bool = False,
    redo_machine: bool = False,
) -> int:
    missing = [m.name for m in modules if not (templates_dir / f"{m.name}.slt").is_file()]
    if missing:
        print(f"error: no template for {', '.join(missing)}", file=sys.stderr)
        print(
            "       run: src\\vcxproj\\build_langs.cmd --export-templates",
            file=sys.stderr,
        )
        return 1
    templates = {m.name: load(templates_dir / f"{m.name}.slt") for m in modules}

    # Contexts depend only on (module, template), never on the language, so
    # build them exactly once. Building them inside the per-language loops --
    # twice per (language, module) pair -- multiplied the dominant cost of a
    # multi-language run by 16 (feature 055).
    contexts_by_module = {m.name: build_contexts(m, templates[m.name]) for m in modules}

    # The client is created on first use, not up front: a run with nothing to
    # translate -- e.g. refreshing control geometry after a dialog template
    # changed -- then needs no key and no network at all, and is deterministic.
    client = None

    def translation_client():
        nonlocal client
        if client is None:
            client = Client(key=load_key(key_file))
            usage = client.usage()
            print(f"DeepL quota: {usage.remaining:,} of {usage.limit:,} characters remaining")
        return client

    reports: list[Coverage] = []
    spent = 0

    for language in languages:
        code = TARGET_CODES.get(language.folder)
        if code is None:
            print(f"skip {language.folder}: no DeepL target code", file=sys.stderr)
            continue

        gaps = collect_gaps(templates, language, modules, redo_accelerators,
                            retranslate, redo_machine, contexts_by_module)
        est = sum(len(text) for _, text in gaps)
        print(f"\n=== {language.folder} ({code}) -- {len(gaps)} unique gaps, ~{est:,} chars")

        translations: dict[tuple[str, str], str] = {}
        if gaps and not dry_run:
            if budget is not None and spent + est > budget:
                print(f"  budget cap reached ({budget:,}); stopping before {language.folder}")
                break

            def progress(done: int, total: int) -> None:
                print(f"\r  translating {done}/{total}", end="", flush=True)

            try:
                api = translation_client()
                translations = api.translate_all(gaps, code, progress)
            except DeepLError as e:
                print(f"\n  error: {e}", file=sys.stderr)
                return 1
            print(f"\r  translated {len(translations)}/{len(gaps)}   ")
            # sent_chars is cumulative on the client, so assign -- adding it
            # each round would count earlier languages again and again.
            spent = api.sent_chars

        for module in modules:
            legacy_path = language.directory / f"{module.name}.slt"
            legacy = load(legacy_path) if legacy_path.is_file() else None
            cov = Coverage(language=language.folder, module=module.name)
            merged, origin = build_slt(
                templates[module.name],
                None if retranslate else legacy,
                language,
                translations,
                cov,
                None if retranslate
                else load_origin(language, module, templates[module.name],
                                 redo_accelerators, redo_machine),
                contexts=contexts_by_module[module.name],
                overrides=load_overrides(language, module),
                module=module,
            )
            reports.append(cov)

            if not dry_run:
                out_path = language.directory / f"{module.name}.slt"
                merged.write(out_path)
                (language.directory / f"{module.name}.origin").write_text(
                    json.dumps(origin, indent=0, sort_keys=True, ensure_ascii=False),
                    encoding="utf-8",
                )

    print_report(reports, dry_run)
    if client is not None:
        print(f"\nDeepL characters sent this run: {client.sent_chars:,}")
        print(f"DeepL quota remaining: {client.usage().remaining:,}")
    return 0


def print_report(reports: list[Coverage], dry_run: bool) -> None:
    print("\n" + "=" * 78)
    print("coverage" + ("  (dry run -- nothing written)" if dry_run else ""))
    print("=" * 78)
    print(f"{'language':<20} {'total':>6} {'human':>6} {'machine':>8} {'fallbk':>7} {'skip':>6} {'human%':>7}")
    print("-" * 78)
    by_lang: dict[str, Coverage] = {}
    for r in reports:
        agg = by_lang.setdefault(r.language, Coverage(language=r.language, module="*"))
        agg.total += r.total
        agg.human += r.human
        agg.machine += r.machine
        agg.fallback += r.fallback
        agg.skip += r.skip
        agg.discarded += r.discarded
        agg.rebranded += r.rebranded
        agg.widened += r.widened
        agg.invalid += r.invalid
        agg.dup_accel += [f"{r.module} {d}" for d in r.dup_accel]
    for agg in by_lang.values():
        print(
            f"{agg.language:<20} {agg.total:>6} {agg.human:>6} {agg.machine:>8} "
            f"{agg.fallback:>7} {agg.skip:>6} {agg.pct_human():>6}%"
        )
    print("-" * 78)
    total_invalid = sum(len(a.invalid) for a in by_lang.values())
    total_dups = sum(len(a.dup_accel) for a in by_lang.values())
    total_rebrand = sum(a.rebranded for a in by_lang.values())
    total_widen = sum(a.widened for a in by_lang.values())
    total_discard = sum(a.discarded for a in by_lang.values())
    print(
        f"rebranded entries: {total_rebrand}   discarded legacy: {total_discard}   "
        f"validation failures: {total_invalid}   duplicate accelerators: {total_dups}"
    )
    total_reaccel = sum(a.reaccel for a in by_lang.values())
    print(
        f"controls widened to fit: {total_widen}   "
        f"accelerators reassigned: {total_reaccel}"
    )
    if total_invalid:
        print("\nvalidation failures (kept English):")
        shown = 0
        for agg in by_lang.values():
            for msg in agg.invalid:
                if shown >= 15:
                    break
                print(f"  [{agg.language}] {msg}")
                shown += 1


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="translate-merge",
        description="Build translations/<lang>/<module>.slt from template + legacy + DeepL.",
    )
    ap.add_argument(
        "--all", action="store_true",
        help="every ENABLED language and module (disabled languages are skipped)",
    )
    ap.add_argument(
        "--language",
        help="restrict to one language folder; naming a disabled language "
             "processes it anyway",
    )
    ap.add_argument("--module", help="restrict to one module")
    ap.add_argument("--dry-run", action="store_true", help="no API calls, no writes")
    ap.add_argument("--templates", type=Path, default=None)
    ap.add_argument("--key-file", type=Path, default=None)
    ap.add_argument("--budget", type=int, default=None, help="stop before exceeding N characters")
    ap.add_argument(
        "--retranslate",
        action="store_true",
        help="ignore existing translations and translate the selected modules "
             "from scratch (use after changing the context descriptions)",
    )
    ap.add_argument(
        "--redo-accelerators",
        action="store_true",
        help="re-translate machine entries whose English carries an accelerator",
    )
    ap.add_argument(
        "--redo-machine",
        action="store_true",
        help="re-translate every machine-provenance entry with the current "
             "context descriptions; human and skip entries are untouched "
             "(--retranslate dominates when both are given)",
    )
    ap.add_argument(
        "--exclude-module",
        action="append",
        default=[],
        metavar="NAME",
        help="drop NAME from the module set (repeatable), e.g. "
             "--exclude-module sftp; cross-module deduplication is kept for "
             "the remaining modules",
    )
    args = ap.parse_args(argv)

    if not (args.all or args.language or args.module or args.dry_run):
        ap.error("specify --all, --language, --module, or --dry-run")

    # Enabled languages only unless one is named explicitly. Translating a
    # language that will not ship spends DeepL characters for nothing
    # (feature 039, FR-012), so an unrestricted run never touches a disabled
    # one. Naming it IS the opt-in (FR-013) -- a maintainer who types the name
    # has said what they want, and a second flag on top of that is ceremony.
    languages = load_languages()
    modules = load_enabled_modules()
    if args.language:
        languages = [l for l in languages if l.folder == args.language]
        if not languages:
            # Not in the enabled set. Look again including the disabled ones so
            # "disabled" and "does not exist" get different answers.
            disabled = [
                l for l in load_languages(include_disabled=True)
                if l.folder == args.language
            ]
            if not disabled:
                print(f"error: unknown language {args.language!r}", file=sys.stderr)
                return 1
            print(
                f"note: {args.language!r} is disabled in languages.cfg -- "
                "processing it because you named it explicitly"
            )
            languages = disabled
    enabled_module_names = {m.name for m in modules}
    if args.module:
        modules = [m for m in modules if m.name == args.module]
        if not modules:
            print(f"error: unknown or disabled module {args.module!r}", file=sys.stderr)
            return 1
    for name in args.exclude_module:
        # Validated against the enabled registry, not the current selection, so
        # "--module zip --exclude-module sftp" is a harmless no-op rather than
        # an error -- but a typo like "--exclude-module sfpt" still fails loud.
        if name not in enabled_module_names:
            print(f"error: unknown or disabled module {name!r}", file=sys.stderr)
            return 1
    if args.exclude_module:
        excluded = set(args.exclude_module)
        modules = [m for m in modules if m.name not in excluded]
        if not modules:
            print("error: no modules left after --exclude-module", file=sys.stderr)
            return 1

    from . import REPO_ROOT

    templates_dir = args.templates or (REPO_ROOT / DEFAULT_TEMPLATES)
    return run(
        languages, modules, templates_dir, args.dry_run, args.key_file,
        args.budget, args.redo_accelerators, args.retranslate,
        args.redo_machine,
    )


if __name__ == "__main__":
    raise SystemExit(main())
