#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pavel Stupka
# SPDX-License-Identifier: GPL-2.0-or-later
"""Generate the codeview language map and viewer masks (feature 070, task T009).

Developer-side only -- the product build consumes the committed output
(src/plugins/codeview/langmap.cpp / .h). See tools/codeview/README.md.

Inputs (all pinned or committed):
  * linguist-languages @ pins.json          -- extensions, filenames, interpreters, tmScope
  * src/plugins/codeview/web/shipped-languages.json  -- the grammars that survived
                                               the licence audit (build_web.py)
  * overlay-editor.json / overlay-windows.json       -- committed corrections
  * ambiguity.json                          -- the FR-006 disambiguation table

Outputs:
  * src/plugins/codeview/langmap.cpp, langmap.h   -- the tables the plugin uses
  * src/plugins/codeview/test/langmap-manifest.json -- data for the harness

Contract: contracts/claimed-types.md. Regeneration from the same pinned inputs
is byte-identical (spec FR-008).
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
PLUGIN = os.path.join(REPO, 'src', 'plugins', 'codeview')
WEB = os.path.join(PLUGIN, 'web')

# Masks that belong to other shipped viewers, or that this plugin deliberately
# leaves to the built-in viewer (contracts/claimed-types.md S2, spec FR-010).
NEVER_CLAIM = {
    # mdview
    '.md', '.markdown',
    # dbviewer
    '.csv', '.dbf', '.tsv',
    # pictview's legacy masks that collide with obscure text formats (v1 concedes)
    '.st', '.pyx', '.dtx', '.icn', '.web', '.sam', '.mac', '.pic', '.img',
    # binary look-alikes that share an extension with a text format
    '.res', '.pt', '.pkl', '.msg', '.app', '.cdf', '.ts.gz',
    # verified collision with a shipped viewer (harness test T033)
    '.pat',
}

# Family grouping for the Viewers list. Order here is *display* order; the
# registration order is reversed by the plugin so the first family ends up on
# top (rows insert at index 0). Every extension lands in the first family whose
# predicate matches; the catch-all family collects the rest.
FAMILIES = [
    ('CODE_CORE', 'Source code'),
    ('SCRIPTS', 'Scripts and shells'),
    ('WEB', 'Web and markup'),
    ('DATA_CONFIG', 'Data and configuration'),
    ('XML_BASED', 'XML-based formats'),
    ('BUILD_CI', 'Build and tooling'),
    ('DOCS_ADJACENT', 'Documentation and data languages'),
    ('PLAINTEXT', 'Plain text and logs'),
]

FAMILY_OF_LANG = {
    'CODE_CORE': {'c', 'cpp', 'objective-c', 'objective-cpp', 'csharp', 'fsharp', 'vb', 'java',
                  'kotlin', 'scala', 'groovy', 'clojure', 'rust', 'go', 'zig', 'd', 'nim', 'swift',
                  'dart', 'haskell', 'ocaml', 'elm', 'purescript', 'gleam', 'crystal', 'odin', 'v',
                  'mojo', 'moonbit', 'c3', 'cairo', 'move', 'vyper', 'solidity', 'pascal', 'ada',
                  'cobol', 'fortran-free-form', 'fortran-fixed-form', 'common-lisp', 'emacs-lisp',
                  'scheme', 'racket', 'fennel', 'hy', 'prolog', 'smalltalk', 'apl', 'abap', 'sas',
                  'stata', 'matlab', 'wolfram', 'apex', 'ballerina', 'chapel', 'clarity', 'codeql',
                  'lean', 'coq', 'asm', 'llvm', 'wasm', 'cuda', 'verilog', 'system-verilog', 'vhdl',
                  'glsl', 'hlsl', 'wgsl', 'shaderlab', 'gdshader', 'gdscript', 'haxe', 'vala',
                  'genie', 'riscv', 'smali', 'erlang', 'elixir', 'julia', 'raku', 'typespec'},
    'SCRIPTS': {'python', 'cython', 'ruby', 'perl', 'lua', 'luau', 'tcl', 'r', 'shellscript', 'fish',
                'nushell', 'awk', 'applescript', 'ahk', 'autoit', 'viml', 'powershell', 'bat',
                'shellsession', 'gnuplot', 'narrat', 'talonscript', 'logo', 'wenyan'},
    'WEB': {'javascript', 'typescript', 'jsx', 'tsx', 'html', 'html-derivative', 'css', 'scss',
            'sass', 'less', 'stylus', 'postcss', 'vue', 'vue-html', 'vue-vine', 'svelte', 'astro',
            'marko', 'imba', 'edge', 'templ', 'handlebars', 'pug', 'haml', 'twig', 'liquid',
            'jinja', 'erb', 'blade', 'soy', 'glimmer-js', 'glimmer-ts', 'php', 'hack', 'graphql',
            'http', 'hurl', 'razor', 'angular-html', 'angular-ts', 'qml', 'qss', 'mdc', 'mdx'},
    'DATA_CONFIG': {'json', 'jsonc', 'json5', 'jsonl', 'hjson', 'yaml', 'toml', 'ini', 'properties',
                    'dotenv', 'editorconfig', 'gitignore', 'git-config', 'git-commit', 'git-rebase',
                    'reg', 'desktop', 'systemd', 'ssh-config', 'nginx', 'apache', 'hcl', 'terraform',
                    'docker', 'proto', 'prisma', 'kdl', 'ron', 'pkl', 'cue', 'jsonnet', 'nix',
                    'bicep', 'kusto', 'powerquery', 'dax', 'codeowners', 'bsl', 'sdbl', 'fluent',
                    'tasl', 'polar', 'smithy', 'rosmsg', 'bird2'},
    'XML_BASED': {'xml', 'xsl'},
    'BUILD_CI': {'cmake', 'make', 'just', 'gn', 'hxml', 'jison', 'regexp', 'gherkin', 'nextflow',
                 'nextflow-groovy', 'jssm', 'zenscript', 'wit', 'openscad', 'beancount'},
    'DOCS_ADJACENT': {'latex', 'tex', 'bibtex', 'rst', 'asciidoc', 'org', 'wikitext', 'mermaid',
                      'typst', 'po', 'diff', 'sql', 'plsql', 'sparql', 'cypher', 'surrealql',
                      'splunk', 'log', 'markdown', 'coffee', 'stylus2', 'wolfram2'},
}

MAX_ROW_BYTES = 200          # hard product cap is 259 (LoadViewers MAX_PATH buffer)


def node_json(work: str, script: str):
    p = os.path.join(work, '_gen.mjs')
    open(p, 'w', encoding='utf-8').write(script)
    out = subprocess.run(['node', p], cwd=work, check=True, capture_output=True)
    if out.returncode != 0:
        raise SystemExit(out.stderr.decode('utf-8', 'replace'))
    return json.loads(out.stdout)


def c_str(s: str) -> str:
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--work', required=True, help='the build_web.py work dir (has node_modules)')
    args = ap.parse_args()

    shipped = json.load(open(os.path.join(WEB, 'shipped-languages.json'), encoding='utf-8'))
    overlay_editor = json.load(open(os.path.join(HERE, 'overlay-editor.json'), encoding='utf-8'))
    overlay_win = json.load(open(os.path.join(HERE, 'overlay-windows.json'), encoding='utf-8'))
    ambiguity = json.load(open(os.path.join(HERE, 'ambiguity.json'), encoding='utf-8'))

    print('[1/5] reading linguist data')
    ling = node_json(args.work,
                     "import * as L from 'linguist-languages';"
                     "const out = {};"
                     "for (const [k, v] of Object.entries(L)) if (v && v.name) out[v.name] = v;"
                     "process.stdout.write(JSON.stringify(out));")
    print(f'      {len(ling)} linguist languages, {len(shipped)} shipped grammars')

    # ---- bridge linguist -> grammar -------------------------------------
    by_scope, by_name = {}, {}
    for gname, g in shipped.items():
        if g['scopeName']:
            by_scope[g['scopeName']] = gname
        by_name[gname.lower()] = gname
        for a in g.get('aliases') or []:
            by_name.setdefault(a.lower(), gname)

    langs = {}          # id -> {display, grammar, family}
    ext_map = {}        # ".ext" -> lang id
    name_map = {}       # exact lower-case file name -> lang id
    multidot = {}       # ".d.ts" -> lang id
    shebang = {}        # interpreter -> lang id
    bridged = 0

    def want(l) -> bool:
        return l.get('type') in ('programming', 'markup', 'data')

    for lname, l in sorted(ling.items()):
        if not want(l):
            continue
        gname = None
        if l.get('tmScope') and l['tmScope'] in by_scope:
            gname = by_scope[l['tmScope']]
        if not gname:
            for cand in [lname.lower()] + [a.lower() for a in (l.get('aliases') or [])]:
                if cand in by_name:
                    gname = by_name[cand]
                    break
        lang_id = gname or ('x-' + lname.lower().replace(' ', '-').replace('/', '-')
                            .replace('+', 'p').replace('#', 'sharp').replace("'", ''))
        if gname:
            bridged += 1
        langs.setdefault(lang_id, {'display': l.get('name', lname), 'grammar': gname})
        # CLAIM POLICY (contracts/claimed-types.md S2): a file type is claimed
        # only when this plugin can actually add something -- i.e. a grammar for
        # it survived the licence audit. Languages Linguist knows but we cannot
        # highlight would open as unstyled text, which the built-in viewer
        # already does well, so they stay with it. The overlays below add the
        # Windows/dev formats that are claimed deliberately.
        if not gname:
            continue
        for e in (l.get('extensions') or []):
            e = e.lower()
            if e in NEVER_CLAIM:
                continue
            if e.count('.') > 1:
                multidot.setdefault(e, lang_id)
            else:
                ext_map.setdefault(e, lang_id)
        for f in (l.get('filenames') or []):
            name_map.setdefault(f.lower(), lang_id)
        for i in (l.get('interpreters') or []):
            shebang.setdefault(i.lower(), lang_id)

    # ---- overlays --------------------------------------------------------
    for src in (overlay_editor, overlay_win):
        for lang_id, spec in src.get('languages', {}).items():
            langs.setdefault(lang_id, {'display': spec.get('display', lang_id),
                                       'grammar': spec.get('grammar')})
            if 'display' in spec:
                langs[lang_id]['display'] = spec['display']
            if 'grammar' in spec:
                langs[lang_id]['grammar'] = spec['grammar'] or None
        for e, lang_id in src.get('extensions', {}).items():
            e = e.lower()
            if e in NEVER_CLAIM:
                continue
            (multidot if e.count('.') > 1 else ext_map)[e] = lang_id
        for f, lang_id in src.get('filenames', {}).items():
            name_map[f.lower()] = lang_id
        for i, lang_id in src.get('interpreters', {}).items():
            shebang[i.lower()] = lang_id
        for e in src.get('drop_extensions', []):
            ext_map.pop(e.lower(), None)
            multidot.pop(e.lower(), None)

    # An overlay may name a language that Linguist does not know but that ships
    # as a grammar of its own (EditorConfig, CODEOWNERS, dotenv, ...): adopt the
    # grammar's own metadata rather than making every overlay repeat it.
    for table in (ext_map, multidot, name_map, shebang):
        for v in set(table.values()):
            if v not in langs and v in shipped:
                langs[v] = {'display': shipped[v].get('displayName', v), 'grammar': v}

    # every referenced language must exist and, if it names a grammar, that
    # grammar must ship (spec FR-008)
    for table in (ext_map, multidot, name_map, shebang):
        for k, v in table.items():
            if v not in langs:
                raise SystemExit(f'FATAL: {k!r} maps to unknown language {v!r}')
    for lang_id, spec in langs.items():
        g = spec.get('grammar')
        if g and g not in shipped:
            raise SystemExit(f'FATAL: language {lang_id!r} references grammar {g!r}, '
                             f'which did not survive the licence audit')

    # ---- ambiguity rules -------------------------------------------------
    rules = ambiguity['rules']
    for ext, spec in ambiguity['extensions'].items():
        e = ext.lower()
        if spec.get('default') not in langs:
            raise SystemExit(f'FATAL: ambiguity default {spec.get("default")!r} for {e} is unknown')
        if e in NEVER_CLAIM:
            continue
        ext_map[e] = spec['default']

    # ---- families and mask rows -----------------------------------------
    fam_of = {}
    for fam, members in FAMILY_OF_LANG.items():
        for lang_id in members:
            fam_of[lang_id] = fam
    rows = {f[0]: [] for f in FAMILIES}
    # PLAINTEXT owns these outright (spec clarification #5). They must not also
    # appear in another family's rows: a mask claimed twice means deleting the
    # plain-text family would NOT stop the plugin opening .txt/.log, which is
    # exactly what FR-009 promises removing a family does.
    PLAINTEXT_OWNED = {'.txt', '.log'}
    for e in sorted(ext_map):
        if e in PLAINTEXT_OWNED:
            continue
        lang_id = ext_map[e]
        fam = fam_of.get(lang_id, 'DATA_CONFIG')
        rows[fam].append('*' + e)
    for e in sorted(multidot):
        fam = fam_of.get(multidot[e], 'DATA_CONFIG')
        rows[fam].append('*' + e)
    for f in sorted(name_map):
        fam = fam_of.get(name_map[f], 'BUILD_CI')
        rows[fam].append(f)
    rows['PLAINTEXT'] = ['*.txt', '*.log']          # spec clarification #5

    # split each family into as many rows as the byte cap needs
    mask_rows = []
    for fam, _label in FAMILIES:
        cur = ''
        for mask in rows[fam]:
            add = (';' if cur else '') + mask
            if len(cur) + len(add) > MAX_ROW_BYTES:
                mask_rows.append((fam, cur))
                cur = mask
            else:
                cur += add
        if cur:
            mask_rows.append((fam, cur))
    over = [r for _f, r in mask_rows if len(r) > MAX_ROW_BYTES]
    if over:
        raise SystemExit(f'FATAL: {len(over)} mask rows exceed {MAX_ROW_BYTES} bytes')

    print(f'[2/5] {len(ext_map)} extensions, {len(multidot)} compound suffixes, '
          f'{len(name_map)} exact names, {len(shebang)} interpreters')
    print(f'[3/5] {len(langs)} languages ({bridged} with a grammar), '
          f'{len(mask_rows)} mask rows across {len(FAMILIES)} families')

    # ---- emit -------------------------------------------------------------
    lang_ids = sorted(langs)
    idx = {l: i for i, l in enumerate(lang_ids)}
    rule_ids = sorted(rules)
    ridx = {r: i for i, r in enumerate(rule_ids)}

    h = []
    h.append('// GENERATED by tools/codeview/gen_langmap.py -- do not edit.\n')
    h.append('// SPDX-FileCopyrightText: 2026 Pavel Stupka\n')
    h.append('// SPDX-License-Identifier: GPL-2.0-or-later\n//\n')
    h.append('// Language identification tables (spec FR-005/FR-008,\n'
             '// contracts/claimed-types.md). Regenerating from the pinned inputs\n'
             '// reproduces this file byte for byte.\n\n')
    h.append('#pragma once\n\n')
    h.append('struct CvLanguage\n{\n    const char* Id;       // stable ascii id (= grammar name when one ships)\n'
             '    const char* Display;  // English display name (not translated: proper names)\n'
             '    const char* Grammar;  // grammar module name, or NULL = plain text (FR-003)\n};\n\n')
    h.append('struct CvNameRule\n{\n    const char* Key;      // lower-case exact name / suffix / extension\n'
             '    short Lang;           // index into CvLanguages\n'
             '    short Rule;           // index into CvAmbiguityRules, -1 = none\n};\n\n')
    h.append('extern const CvLanguage CvLanguages[];\nextern const int CvLanguageCount;\n\n')
    h.append('extern const CvNameRule CvExactNames[];\nextern const int CvExactNameCount;\n\n')
    h.append('extern const CvNameRule CvCompoundSuffixes[];\nextern const int CvCompoundSuffixCount;\n\n')
    h.append('extern const CvNameRule CvExtensions[];\nextern const int CvExtensionCount;\n\n')
    h.append('extern const CvNameRule CvInterpreters[];\nextern const int CvInterpreterCount;\n\n')
    h.append('// Ambiguity rule ids, in the order of CvAmbiguityRules; intake.cpp\n'
             '// implements the probe for each (spec FR-006).\n')
    h.append('enum CvAmbiguityRuleId\n{\n')
    for r in rule_ids:
        h.append('    CV_RULE_%s,\n' % r.upper().replace('-', '_'))
    h.append('    CV_RULE_COUNT\n};\n\n')
    h.append('struct CvAmbiguityRule\n{\n    const char* Id;\n    short Fallback;   // language when no probe matches\n};\n\n')
    h.append('extern const CvAmbiguityRule CvAmbiguityRules[];\n\n')
    h.append('// The Viewers-list rows registered by Connect(), in DISPLAY order.\n'
             '// codeview.cpp registers them in reverse (rows insert at index 0).\n')
    h.append('extern const char* const CvMaskRows[];\nextern const int CvMaskRowCount;\n')
    open(os.path.join(PLUGIN, 'langmap.h'), 'w', encoding='utf-8', newline='\r\n').write(''.join(h))

    c = []
    c.append('// GENERATED by tools/codeview/gen_langmap.py -- do not edit.\n')
    c.append('// SPDX-FileCopyrightText: 2026 Pavel Stupka\n')
    c.append('// SPDX-License-Identifier: GPL-2.0-or-later\n\n')
    c.append('#include "precomp.h"\n#include "langmap.h"\n\n')
    c.append('const CvLanguage CvLanguages[] = {\n')
    for l in lang_ids:
        spec = langs[l]
        g = c_str(spec['grammar']) if spec.get('grammar') else 'NULL'
        c.append('    { %s, %s, %s },\n' % (c_str(l), c_str(spec['display']), g))
    c.append('};\nconst int CvLanguageCount = %d;\n\n' % len(lang_ids))

    def emit_table(name, table, count_name, rule_for=None):
        c.append('const CvNameRule %s[] = {\n' % name)
        for k in sorted(table):
            r = -1
            if rule_for and k in rule_for:
                r = ridx[rule_for[k]]
            c.append('    { %s, %d, %d },\n' % (c_str(k), idx[table[k]], r))
        c.append('};\nconst int %s = %d;\n\n' % (count_name, len(table)))

    ext_rule = {e.lower(): spec['rule'] for e, spec in ambiguity['extensions'].items()
                if spec.get('rule') and e.lower() in ext_map}
    emit_table('CvExactNames', name_map, 'CvExactNameCount')
    emit_table('CvCompoundSuffixes', multidot, 'CvCompoundSuffixCount')
    emit_table('CvExtensions', ext_map, 'CvExtensionCount', ext_rule)
    emit_table('CvInterpreters', shebang, 'CvInterpreterCount')

    c.append('const CvAmbiguityRule CvAmbiguityRules[] = {\n')
    for r in rule_ids:
        fb = rules[r]['fallback']
        if fb not in idx:
            raise SystemExit(f'FATAL: ambiguity rule {r} falls back to unknown language {fb!r}')
        c.append('    { %s, %d },\n' % (c_str(r), idx[fb]))
    c.append('};\n\n')

    c.append('const char* const CvMaskRows[] = {\n')
    for fam, row in mask_rows:
        c.append('    %s, // %s\n' % (c_str(row), fam))
    c.append('};\nconst int CvMaskRowCount = %d;\n' % len(mask_rows))
    open(os.path.join(PLUGIN, 'langmap.cpp'), 'w', encoding='utf-8', newline='\r\n').write(''.join(c))

    print('[4/5] wrote langmap.h / langmap.cpp')

    manifest = {
        'generated_by': 'tools/codeview/gen_langmap.py',
        'counts': {'languages': len(lang_ids), 'with_grammar': bridged,
                   'extensions': len(ext_map), 'compound_suffixes': len(multidot),
                   'exact_names': len(name_map), 'interpreters': len(shebang),
                   'mask_rows': len(mask_rows),
                   'masks': sum(len(r.split(';')) for _f, r in mask_rows)},
        'never_claim': sorted(NEVER_CLAIM),
        'mask_rows': [{'family': f, 'row': r, 'bytes': len(r)} for f, r in mask_rows],
        'languages': {l: langs[l] for l in lang_ids},
        'extensions': ext_map,
        'compound_suffixes': multidot,
        'exact_names': name_map,
        'interpreters': shebang,
        'ambiguity': ambiguity,
    }
    os.makedirs(os.path.join(PLUGIN, 'test'), exist_ok=True)
    with open(os.path.join(PLUGIN, 'test', 'langmap-manifest.json'), 'w',
              encoding='utf-8', newline='\n') as f:
        json.dump(manifest, f, indent=1, sort_keys=True)
        f.write('\n')
    print(f'[5/5] manifest written: {manifest["counts"]["masks"]} masks, '
          f'{manifest["counts"]["languages"]} languages')
    return 0


if __name__ == '__main__':
    sys.exit(main())
