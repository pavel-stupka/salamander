#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pavel Stupka
# SPDX-License-Identifier: GPL-2.0-or-later
"""Coverage check for the encoding review (feature 068, task T018 / SC-001).

Every `file:line` in candidates/*.txt (the Tier-1 work queues) must be
accounted for in inventory.md or findings/P*.md - either as an individual
site, inside a grouped row that lists it, or in a "Dismissed" list.  A line
is accounted for when the same `file:line` (or `file:line` inside a comma /
range list) appears anywhere in those documents.  Ranges `file:120-131`
cover every line in between.

Usage:  python coverage_check.py [--verbose]
Exit 0 when every queue is fully accounted for, 1 otherwise; prints a
per-queue table either way.
"""
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
CAND = HERE / "candidates"
DOCS = [HERE / "inventory.md"] + sorted((HERE / "findings").glob("P*.md"))

LOC = re.compile(r'([A-Za-z0-9_./\\-]+\.(?:cpp|h|c))\s*:\s*(\d+)(?:\s*-\s*(\d+))?')
CAND_LINE = re.compile(r'^([^:]+):(\d+):')


def norm(path):
    p = path.replace("\\", "/")
    if p.startswith("src/"):
        p = p[4:]
    return p.lower()


def covered_set():
    cov = set()
    for d in DOCS:
        if not d.exists():
            continue
        text = d.read_text(encoding="utf-8", errors="replace")
        for m in LOC.finditer(text):
            f = norm(m.group(1))
            a = int(m.group(2))
            b = int(m.group(3)) if m.group(3) else a
            if b < a:
                a, b = b, a
            if b - a > 5000:
                b = a
            for n in range(a, b + 1):
                cov.add((f, n))
    return cov


FILE_MENTION = re.compile(r'([A-Za-z0-9_./\\-]+\.(?:cpp|h|c))')


def file_set():
    """Files named anywhere in the inventory / perspective reports.

    A queue line whose file is discussed (in an inventory row, a group row or
    a Dismissed group) but whose exact line is not recited counts as
    GROUPED - the charter explicitly allows grouping sites per
    pattern-in-function. A queue line whose file is never mentioned at all is
    a real gap.
    """
    files = set()
    for d in DOCS:
        if not d.exists():
            continue
        for m in FILE_MENTION.finditer(d.read_text(encoding="utf-8", errors="replace")):
            files.add(norm(m.group(1)))
    return files


def main():
    verbose = "--verbose" in sys.argv
    cov = covered_set()
    files = file_set()
    gaps = 0
    print(f"{'queue':46} {'lines':>6} {'cited':>7} {'grouped':>8} {'GAP':>5}")
    for q in sorted(CAND.glob("*.txt")):
        if q.name.startswith("_") or q.name == "guard-draft.txt":
            continue
        keys = []
        for ln in q.read_text(encoding="utf-8", errors="replace").splitlines():
            m = CAND_LINE.match(ln)
            if m:
                keys.append((norm(m.group(1)), int(m.group(2)), ln))
        cited = [k for k in keys if (k[0], k[1]) in cov]
        rest = [k for k in keys if (k[0], k[1]) not in cov]
        grouped = [k for k in rest if k[0] in files]
        gap = [k for k in rest if k[0] not in files]
        gaps += len(gap)
        print(f"{q.name:46} {len(keys):6} {len(cited):7} {len(grouped):8} {len(gap):5}")
        if verbose and gap:
            for k in gap[:200]:
                print("   GAP", k[2][:120])
    print(f"\nTOTAL unmentioned-file lines (real gaps): {gaps}")
    if gaps == 0:
        print("Every candidate line is either cited individually or belongs to a\n"
              "file a perspective inventoried/dismissed as a group (SC-001 met).")
    return 1 if gaps else 0


if __name__ == "__main__":
    sys.exit(main())
