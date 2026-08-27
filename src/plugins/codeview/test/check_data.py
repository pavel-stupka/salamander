#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pavel Stupka
# SPDX-License-Identifier: GPL-2.0-or-later
"""codeview data-level harness (feature 070, tasks T033/T035).

Checks the generated language map, the viewer masks and the licence manifest
against the rules the specification and the contracts state. Everything here is
deterministic and needs no GUI, so it can run on every build:

    python src/plugins/codeview/test/check_data.py

Exit code 0 = all checks pass. Any failure prints the rule it broke.
"""

from __future__ import annotations

import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.dirname(HERE)
REPO = os.path.abspath(os.path.join(PLUGIN, '..', '..', '..'))

MANIFEST = os.path.join(HERE, 'langmap-manifest.json')
LICENCES = os.path.join(PLUGIN, 'web', 'licence-manifest.json')

# contracts/claimed-types.md S3: a stored Viewers entry is read into
# char[MAX_PATH] and a failure drops that row AND every row below it
# (src/salamdr2.cpp:2708-2745), so 259 is the hard ceiling.
HARD_ROW_CAP = 259
SOFT_ROW_CAP = 200

# spec FR-010
NEVER = {'*.md', '*.markdown', '*.csv', '*.dbf', '*.tsv', '*.*'}

# spec SC-001
MIN_LANGUAGES_WITH_GRAMMAR = 200
MIN_MASKS = 700

failures: list[str] = []


def check(ok: bool, rule: str, detail: str = '') -> None:
    if ok:
        print(f'  PASS  {rule}')
    else:
        print(f'  FAIL  {rule}' + (f'\n        {detail}' if detail else ''))
        failures.append(rule)


def other_plugin_masks() -> dict[str, set[str]]:
    """Every mask registered by another shipped viewer plugin."""
    out: dict[str, set[str]] = {}
    for path in glob.glob(os.path.join(REPO, 'src', 'plugins', '*', '*.cpp')):
        plug = os.path.basename(os.path.dirname(path))
        if plug == 'codeview':
            continue
        try:
            text = open(path, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        for m in re.finditer(r'AddViewer\s*\(\s*"([^"]*)"', text):
            for mask in m.group(1).split(';'):
                mask = mask.strip().lower()
                if mask:
                    out.setdefault(mask, set()).add(plug)
    return out


def main() -> int:
    man = json.load(open(MANIFEST, encoding='utf-8'))
    lic = json.load(open(LICENCES, encoding='utf-8'))
    rows = man['mask_rows']
    masks = [m.strip().lower() for r in rows for m in r['row'].split(';') if m.strip()]

    print('codeview data harness')
    print(f'  {len(masks)} masks in {len(rows)} rows, '
          f'{man["counts"]["languages"]} languages '
          f'({man["counts"]["with_grammar"]} with a grammar)')

    print('\n[claimed types]')
    over_hard = [r for r in rows if r['bytes'] > HARD_ROW_CAP]
    check(not over_hard, 'every Viewers row is within the hard 259-byte cap',
          f'{len(over_hard)} rows over cap')
    over_soft = [r for r in rows if r['bytes'] > SOFT_ROW_CAP]
    check(not over_soft, f'every Viewers row is within the {SOFT_ROW_CAP}-byte working cap',
          f'{len(over_soft)} rows over the working cap')

    banned = sorted(set(masks) & NEVER)
    check(not banned, 'no mask this plugin must never claim (FR-010)', str(banned))

    others = other_plugin_masks()
    clash = sorted(m for m in masks if m in others)
    check(not clash, 'zero mask intersection with other shipped viewers (FR-010)',
          '; '.join(f'{m} <- {sorted(others[m])}' for m in clash[:10]))

    dupes = sorted({m for m in masks if masks.count(m) > 1})
    check(not dupes, 'no mask is registered twice', str(dupes[:10]))

    # A family's rows must be contiguous, so removing a family in Options is one
    # block of entries (spec FR-009).
    order = [r['family'] for r in rows]
    seen, contiguous = set(), True
    prev = None
    for fam in order:
        if fam != prev:
            if fam in seen:
                contiguous = False
            seen.add(fam)
            prev = fam
    check(contiguous, "each family's rows are contiguous (FR-009)", str(order))

    print('\n[coverage]')
    check(man['counts']['with_grammar'] >= MIN_LANGUAGES_WITH_GRAMMAR,
          f'at least {MIN_LANGUAGES_WITH_GRAMMAR} languages have a grammar (SC-001)',
          f'have {man["counts"]["with_grammar"]}')
    check(len(masks) >= MIN_MASKS, f'at least {MIN_MASKS} file-name patterns claimed (SC-001)',
          f'have {len(masks)}')
    check(lic['counts']['themes'] >= 6, 'at least 3 light + 3 dark schemes ship (FR-013)',
          f'have {lic["counts"]["themes"]}')

    print('\n[language map]')
    langs = man['languages']
    shipped_grammars = {n for n, v in lic['shipped'].items() if v['kind'] == 'grammar'}
    missing = sorted({v['grammar'] for v in langs.values()
                      if v.get('grammar') and v['grammar'] not in shipped_grammars})
    check(not missing, 'every referenced grammar survived the licence audit (FR-008)', str(missing))

    for table in ('extensions', 'compound_suffixes', 'exact_names', 'interpreters'):
        unknown = sorted({v for v in man[table].values() if v not in langs})
        check(not unknown, f'every {table} entry names a known language', str(unknown[:10]))

    # The generated C table is binary-searched at run time, so it must be sorted.
    cpp = open(os.path.join(PLUGIN, 'langmap.cpp'), encoding='utf-8').read()
    for name in ('CvExactNames', 'CvCompoundSuffixes', 'CvExtensions', 'CvInterpreters'):
        block = re.search(r'const CvNameRule %s\[\] = \{(.*?)\n\};' % name, cpp, re.S)
        keys = re.findall(r'\{ "((?:[^"\\]|\\.)*)"', block.group(1)) if block else []
        check(keys == sorted(keys), f'{name} is sorted (intake.cpp binary-searches it)',
              'first out-of-order key: ' +
              next((k for a, k in zip(sorted(keys), keys) if a != k), '?'))

    print('\n[licences]')
    bad = {n: v for n, v in lic['shipped'].items()
           if v['licence'] in ('GPL-3.0', 'GPL-2.0', 'AGPL-3.0', 'GNU', 'NOASSERTION')}
    check(not bad, 'no copyleft-incompatible asset ships (clarification 2026-08-26)',
          str(sorted(bad)))
    unlicensed = {n for n, v in lic['shipped'].items() if not v.get('licence')}
    check(not unlicensed, 'every shipped asset has a resolved licence', str(sorted(unlicensed)))
    print(f'        ({len(lic["or_later_notices"])} assets ship under the "or later" clause; '
          f'{lic["counts"]["grammars_excluded"]} excluded)')

    print('\n[assets]')
    table = open(os.path.join(PLUGIN, 'web', 'assets_table.inc'), encoding='utf-8').read()
    urls = re.findall(r'\{ "([^"]+)", IDR_WEB_FIRST\+(\d+)', table)
    check(len(urls) == lic['counts']['assets'],
          'the resource table lists every generated asset',
          f'{len(urls)} in table vs {lic["counts"]["assets"]} in manifest')
    ids = [int(i) for _u, i in urls]
    check(ids == list(range(len(ids))), 'resource ids are dense and start at IDR_WEB_FIRST')
    missing_files = [u for u, _i in urls
                     if not os.path.exists(os.path.join(PLUGIN, 'web', u.replace('/', os.sep)))]
    check(not missing_files, 'every asset named in the table exists on disk', str(missing_files[:5]))

    # rc.exe does not evaluate arithmetic in the resource-id position: a
    # symbolic id like "IDR_WEB_FIRST+0" becomes a resource NAMED "5000+0"
    # and FindResource(MAKEINTRESOURCE(...)) returns 403 for every page asset.
    rc2 = open(os.path.join(PLUGIN, 'web', 'assets.rc2'), encoding='utf-8').read()
    rc_ids = [int(i) for i in re.findall(r'^(\d+) RCDATA "', rc2, re.M)]
    symbolic = re.findall(r'^([^/\s]\S*\+\S*)\s+RCDATA', rc2, re.M)
    check(not symbolic, 'assets.rc2 ids are numeric literals (rc.exe would make "5000+0" a NAME)',
          str(symbolic[:3]))
    check(rc_ids == [5000 + i for i in range(len(urls))],
          'assets.rc2 ids are IDR_WEB_FIRST (5000) + index, in table order',
          f'{len(rc_ids)} rc entries vs {len(urls)} table entries, first={rc_ids[:1]}')

    print()
    if failures:
        print(f'FAILED: {len(failures)} rule(s) broken')
        return 1
    print('All data checks passed.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
