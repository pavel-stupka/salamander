#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pavel Stupka
# SPDX-License-Identifier: GPL-2.0-or-later
"""Generate the codeview plugin's embedded web assets (feature 070, task T008).

Developer-side only -- the product build NEVER runs this script; it consumes the
committed output under src/plugins/codeview/web/. See tools/codeview/README.md.

What it does, in order:

1. installs the pinned npm packages from pins.json into a work directory;
2. runs the LICENCE AUDIT over tm-grammars/tm-themes metadata plus the
   committed manual resolutions (resolved-licences.json) and refuses to ship
   anything that is not GPLv2-compatible (research D3, spec clarification #2);
3. bundles the Shiki core + Oniguruma engine into one ESM file with esbuild;
4. copies the language modules reachable from the shipped languages, writing a
   licence STUB (a module exporting []) in place of every excluded grammar that
   a shipped grammar imports (spike-results.md S7);
5. copies the 12 shipped themes;
6. writes web/assets.rc2 (RCDATA block), web/assets_table.inc (URL -> resource
   id table for webglue.cpp) and web/licence-manifest.json.

Usage:  python tools/codeview/build_web.py [--work DIR] [--keep]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
WEB = os.path.join(REPO, 'src', 'plugins', 'codeview', 'web')

# Licences accepted for a shipped asset. GPLv2-or-later is the product licence,
# so GPL-3.0-only assets are refused (spec clarification 2026-08-26); Apache-2.0
# is accepted only through the "or later" clause and is flagged in the manifest.
COMPATIBLE = {
    'MIT': 'ok',
    'BSD-2-Clause': 'ok',
    'BSD-3-Clause': 'ok',
    'ISC': 'ok',
    'TextMate-permissive': 'ok',
    'Unlicense/PD': 'ok',
    'MPL-2.0': 'ok',
    'Apache-2.0': 'or-later',       # compatible with GPLv3, i.e. via "or later"
    'Apache-2.0-LLVM': 'or-later',
}

# The 12 shipped colour schemes (research D16; all MIT in tm-themes metadata).
THEMES = [
    'github-light', 'light-plus', 'one-light', 'solarized-light', 'catppuccin-latte',
    'github-dark', 'dark-plus', 'one-dark-pro', 'solarized-dark', 'catppuccin-mocha',
    'gruvbox-dark-medium', 'nord',
]


def run(cmd, cwd, **kw):
    print('  $', ' '.join(cmd))
    return subprocess.run(cmd, cwd=cwd, check=True, **kw)


def npm_install(work: str, pins: dict) -> None:
    pkg = {
        'name': 'codeview-webbuild', 'private': True, 'version': '1.0.0', 'type': 'module',
        'dependencies': pins['npm'],
    }
    with open(os.path.join(work, 'package.json'), 'w', encoding='utf-8') as f:
        json.dump(pkg, f, indent=2)
    npm = 'npm.cmd' if os.name == 'nt' else 'npm'
    run([npm, 'install', '--no-audit', '--no-fund'], work)
    # esbuild needs its postinstall to place the platform binary
    try:
        run([npm, 'approve-scripts', 'esbuild'], work)
    except subprocess.CalledProcessError:
        pass  # older npm without the gate


def read_metadata(work: str) -> tuple[list, list]:
    """Return (grammars, themes) metadata from the installed packages."""
    script = (
        "import {grammars} from 'tm-grammars';"
        "import {themes} from 'tm-themes';"
        "process.stdout.write(JSON.stringify({grammars, themes}));"
    )
    p = os.path.join(work, '_meta.mjs')
    open(p, 'w', encoding='utf-8').write(script)
    out = subprocess.run(['node', p], cwd=work, check=True, capture_output=True).stdout
    data = json.loads(out)
    return data['grammars'], data['themes']


def audit(grammars, themes, resolved) -> tuple[dict, dict, list]:
    """Classify every grammar/theme. Returns (shipped, excluded, notices)."""
    shipped, excluded, notices = {}, {}, []
    for g in grammars:
        lic = g.get('license') or None
        src = g.get('source') or ''
        if lic in (None, 'NOASSERTION'):
            r = resolved.get(g['name'])
            if r and r.get('spdx'):
                lic, src = r['spdx'], r.get('url', src)
            else:
                excluded[g['name']] = {'kind': 'grammar', 'reason': 'licence unresolved',
                                       'metadata': g.get('license'), 'source': g.get('source', '')}
                continue
        status = COMPATIBLE.get(lic)
        if not status:
            excluded[g['name']] = {'kind': 'grammar', 'reason': 'incompatible licence',
                                   'licence': lic, 'source': g.get('source', '')}
            continue
        shipped[g['name']] = {'kind': 'grammar', 'licence': lic, 'source': src,
                              'scopeName': g.get('scopeName', ''),
                              'displayName': g.get('displayName', g['name']),
                              'aliases': g.get('aliases', []) or []}
        if status == 'or-later':
            notices.append(f"{g['name']}: {lic} (GPLv2-or-later compatible only via the 'or later' clause)")
    tmeta = {t['name']: t for t in themes}
    for name in THEMES:
        t = tmeta.get(name)
        if t is None:
            raise SystemExit(f'FATAL: theme {name} not present in tm-themes')
        lic = t.get('license')
        status = COMPATIBLE.get(lic)
        if not status:
            raise SystemExit(f'FATAL: shipped theme {name} has licence {lic!r} '
                             f'-- remove it from THEMES or resolve the licence')
        shipped['theme:' + name] = {'kind': 'theme', 'licence': lic,
                                    'source': t.get('source', ''), 'type': t.get('type', '')}
        if status == 'or-later':
            notices.append(f"theme {name}: {lic} (via the 'or later' clause)")
    return shipped, excluded, notices


IMPORT_RE = re.compile(r"^import\s+\w+\s+from\s+'\./([A-Za-z0-9_.\-]+)\.mjs'", re.M)


def collect_langs(langs_dir: str, shipped: dict, excluded: dict) -> tuple[set, set]:
    """Transitive closure of module files needed by the shipped languages.

    Returns (files, stubs): 'files' are copied verbatim, 'stubs' are excluded
    grammars that a shipped grammar imports -- they are replaced by a module
    exporting [] so the importer keeps working without shipping the content.
    """
    entry = [n for n, v in shipped.items() if v['kind'] == 'grammar']
    files, stubs, queue = set(), set(), list(entry)
    seen = set()
    while queue:
        name = queue.pop()
        if name in seen:
            continue
        seen.add(name)
        path = os.path.join(langs_dir, name + '.mjs')
        if not os.path.exists(path):
            continue
        if name in excluded:
            stubs.add(name)
            continue          # do not read its imports: nothing of it ships
        files.add(name)
        for dep in IMPORT_RE.findall(open(path, encoding='utf-8').read()):
            if dep not in seen:
                queue.append(dep)
    return files, stubs


def write_engine(work: str) -> None:
    entry = os.path.join(work, 'engine-entry.mjs')
    open(entry, 'w', encoding='utf-8').write(
        "export { createHighlighterCore, getLastGrammarState } from '@shikijs/core'\n"
        "export { createOnigurumaEngine } from '@shikijs/engine-oniguruma'\n"
        "export { default as wasmBinary } from 'shiki/wasm'\n")
    npx = 'npx.cmd' if os.name == 'nt' else 'npx'
    out = os.path.join(WEB, 'shiki', 'engine.js')
    run([npx, '--no-install', 'esbuild', 'engine-entry.mjs', '--bundle', '--format=esm',
         '--minify', '--target=es2022', '--outfile=' + out], work)


def rc_escape(path: str) -> str:
    return path.replace('\\', '\\\\')


# Must match IDR_WEB_FIRST in src/plugins/codeview/codeview.rh2. The .rc2 needs
# the value resolved: rc.exe does not evaluate arithmetic in the resource-id
# position, so an id written as "IDR_WEB_FIRST+0" compiles into a resource
# NAMED "5000+0" and FindResource(MAKEINTRESOURCE(5000)) never finds it.
IDR_WEB_FIRST = 5000


def emit_resource_tables(assets: list[tuple[str, str, str]]) -> None:
    """assets: (url path, file path relative to web/, MIME type)."""
    rc, tab = [], []
    rc.append('// GENERATED by tools/codeview/build_web.py -- do not edit.\n'
              '// Paths are relative to the .rc that includes this file (the plugin root),\n'
              '// not to this file: the resource compiler resolves them from the .rc.\n'
              '// Ids are IDR_WEB_FIRST (%d) + index, written as numbers because rc.exe\n'
              '// treats an id containing "+" as a resource NAME, not an integer.\n'
              % IDR_WEB_FIRST)
    tab.append('// GENERATED by tools/codeview/build_web.py -- do not edit.\n'
               '// URL path -> embedded resource id + MIME type (contracts/host-page-interface.md).\n')
    tab.append('static const CodeViewAsset g_assets[] = {\n')
    for i, (url, rel, mime) in enumerate(assets):
        rc.append('%d RCDATA "%s"\n' % (IDR_WEB_FIRST + i, rc_escape(os.path.join('web', rel))))
        tab.append('    { "%s", IDR_WEB_FIRST+%d, "%s" },\n' % (url, i, mime))
    tab.append('};\n')
    open(os.path.join(WEB, 'assets.rc2'), 'w', encoding='utf-8', newline='\r\n').write(''.join(rc))
    open(os.path.join(WEB, 'assets_table.inc'), 'w', encoding='utf-8', newline='\r\n').write(''.join(tab))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--work', default=None, help='work directory (default: a temp dir)')
    ap.add_argument('--keep', action='store_true', help='keep the work directory')
    ap.add_argument('--skip-install', action='store_true', help='reuse an existing work dir')
    args = ap.parse_args()

    pins = json.load(open(os.path.join(HERE, 'pins.json'), encoding='utf-8'))
    resolved = json.load(open(os.path.join(HERE, 'resolved-licences.json'), encoding='utf-8'))['resolved']

    work = args.work or tempfile.mkdtemp(prefix='codeview-web-')
    os.makedirs(work, exist_ok=True)
    print('work dir:', work)

    if not args.skip_install:
        print('[1/6] installing pinned packages')
        npm_install(work, pins)

    print('[2/6] licence audit')
    grammars, themes = read_metadata(work)
    shipped, excluded, notices = audit(grammars, themes, resolved)
    n_gram = sum(1 for v in shipped.values() if v['kind'] == 'grammar')
    print(f'      {n_gram} grammars ship, {len(excluded)} excluded, {len(THEMES)} themes')
    if n_gram < pins['minimum_grammars']:
        raise SystemExit(f'FATAL: only {n_gram} grammars pass the audit, '
                         f'minimum is {pins["minimum_grammars"]} (SC-001)')

    langs_dir = os.path.join(work, 'node_modules', '@shikijs', 'langs', 'dist')
    themes_dir = os.path.join(work, 'node_modules', '@shikijs', 'themes', 'dist')
    files, stubs = collect_langs(langs_dir, shipped, excluded)
    print(f'      {len(files)} language modules reachable, {len(stubs)} licence stubs '
          f'({", ".join(sorted(stubs)) or "none"})')

    print('[3/6] clearing generated output')
    for sub in ('shiki',):
        p = os.path.join(WEB, sub)
        if os.path.isdir(p):
            shutil.rmtree(p)
    for sub in ('shiki', os.path.join('shiki', 'langs'), os.path.join('shiki', 'themes')):
        os.makedirs(os.path.join(WEB, sub), exist_ok=True)

    print('[4/6] bundling the engine')
    write_engine(work)

    print('[5/6] copying grammars and themes')
    for name in sorted(files):
        shutil.copyfile(os.path.join(langs_dir, name + '.mjs'),
                        os.path.join(WEB, 'shiki', 'langs', name + '.mjs'))
    stub_body = ('// Licence stub (feature 070): the upstream grammar for this language could\n'
                 '// not be shipped -- see web/licence-manifest.json. A shipped grammar imports\n'
                 '// it, so an empty grammar list keeps that grammar working.\n'
                 'export default []\n')
    for name in sorted(stubs):
        open(os.path.join(WEB, 'shiki', 'langs', name + '.mjs'), 'w',
             encoding='utf-8', newline='\n').write(stub_body)
    for name in THEMES:
        shutil.copyfile(os.path.join(themes_dir, name + '.mjs'),
                        os.path.join(WEB, 'shiki', 'themes', name + '.mjs'))

    print('[6/6] writing resource tables and manifest')
    assets: list[tuple[str, str, str]] = []
    JS = 'text/javascript; charset=utf-8'
    for base, mime in (('viewer.html', 'text/html; charset=utf-8'),
                       ('viewer.css', 'text/css; charset=utf-8'),
                       ('viewer.js', JS),
                       ('worker.js', JS)):
        if not os.path.exists(os.path.join(WEB, base)):
            print(f'      WARNING: {base} missing (hand-written asset, not generated)')
        assets.append((base, base, mime))
    assets.append(('shiki/engine.js', os.path.join('shiki', 'engine.js'), JS))
    for name in sorted(files | stubs):
        assets.append(('shiki/langs/%s.mjs' % name, os.path.join('shiki', 'langs', name + '.mjs'), JS))
    for name in THEMES:
        assets.append(('shiki/themes/%s.mjs' % name, os.path.join('shiki', 'themes', name + '.mjs'), JS))
    emit_resource_tables(assets)

    manifest = {
        'generated_by': 'tools/codeview/build_web.py',
        'pins': pins['npm'],
        'policy': 'GPLv2-or-later compatible only; GPL-3.0-only refused (spec clarification 2026-08-26)',
        'counts': {'grammars_shipped': n_gram, 'grammars_excluded': len(excluded),
                   'language_modules': len(files), 'licence_stubs': len(stubs),
                   'themes': len(THEMES), 'assets': len(assets)},
        'or_later_notices': sorted(notices),
        'shipped': shipped,
        'excluded': excluded,
        'stubs': sorted(stubs),
    }
    with open(os.path.join(WEB, 'licence-manifest.json'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(manifest, f, indent=1, sort_keys=True)
        f.write('\n')

    # language list for gen_langmap.py: grammar name -> scope/display/aliases
    langs_json = {n: {'displayName': v['displayName'], 'scopeName': v['scopeName'],
                      'aliases': v['aliases']}
                  for n, v in shipped.items() if v['kind'] == 'grammar'}
    with open(os.path.join(WEB, 'shipped-languages.json'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(langs_json, f, indent=1, sort_keys=True)
        f.write('\n')

    total = sum(os.path.getsize(os.path.join(WEB, rel)) for _, rel, _ in assets
                if os.path.exists(os.path.join(WEB, rel)))
    print(f'done: {len(assets)} assets, {total / 1024 / 1024:.1f} MB')
    if not args.keep and not args.work:
        shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
