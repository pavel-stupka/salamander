#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pavel Stupka
# SPDX-License-Identifier: GPL-2.0-or-later
"""Consolidate the perspective reports into review-report.md (feature 068, T017).

Reads findings/P*.md and rewrites the generated sections of review-report.md
between the markers

    <!-- BEGIN GENERATED: <name> -->  ...  <!-- END GENERATED: <name> -->

Sections: findings (one row per raised Finding), ledger (every L-row
disposition proposed by any perspective, merged), contracts (every contract
obligation verdict, merged). Hand-written prose outside the markers is never
touched, so the file can be edited freely and regenerated.

Usage: python consolidate.py
"""
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
FIND = HERE / "findings"
REPORT = HERE / "review-report.md"

PERSP_ORDER = ["P1", "P2", "P3", "P4", "P5", "P6", "P7"]


def read(p):
    return p.read_text(encoding="utf-8", errors="replace") if p.exists() else ""


def section(txt, title):
    """Body of a '## <title>' section (all occurrences concatenated)."""
    out = []
    for m in re.finditer(r'^##\s+%s\s*.*?$(.*?)(?=^## |\Z)' % re.escape(title),
                         txt, re.M | re.S):
        out.append(m.group(1))
    return "\n".join(out)


def field(body, *labels):
    for lab in labels:
        m = re.search(r'^[-*]?\s*\*\*%s[^:*]*\*\*\s*:?\s*(.+?)$' % lab, body, re.M | re.I)
        if m:
            return m.group(1).strip()
    return ""


def cell(s, limit=240):
    s = re.sub(r'\s+', ' ', s or "").strip()
    s = s.replace("|", "\\|")
    return (s[:limit] + "…") if len(s) > limit else s


def collect_findings():
    rows = []
    for p in PERSP_ORDER:
        txt = read(FIND / f"{p}.md")
        if not txt:
            continue
        for m in re.finditer(r'^###\s+(F-%s-\d+)\s*[—–-]\s*(.+?)\s*$' % p, txt, re.M):
            fid, claim = m.group(1), m.group(2)
            start = m.end()
            nxt = re.search(r'^(?:###\s|##\s)', txt[start:], re.M)
            body = txt[start:start + (nxt.start() if nxt else 6000)]
            withdrawn = bool(re.match(r'\s*WITHDRAWN|\s*\*\*WITHDRAWN', claim, re.I))
            rows.append(dict(
                id=fid, persp=p, claim=claim, withdrawn=withdrawn,
                dc=field(body, "Defect class", "Class"),
                sites=field(body, "Sites?", "Location", "Site"),
                scen=field(body, "Failure scenario", "Scenario"),
            ))
    return rows


def collect_tables(title, key_re):
    """Merge markdown tables from a named section across perspectives."""
    seen, rows = {}, []
    for p in PERSP_ORDER:
        body = section(read(FIND / f"{p}.md"), title)
        for line in body.splitlines():
            line = line.strip()
            if not line.startswith("|") or re.match(r'^\|[\s:|-]+\|$', line):
                continue
            if re.search(r'^\|\s*(Obligation|ID|Row|Ledger)\b', line, re.I):
                continue
            m = re.search(key_re, line)
            if not m:
                continue
            key = m.group(0)
            cells = [c.strip() for c in line.strip("|").split("|")]
            rows.append((key, p, cells))
            seen.setdefault(key, []).append(p)
    return rows, seen


VERDICT_HDR = re.compile(
    r'^##\s+(F-[A-Za-z0-9]+-\d+)\s*[·:|—–-]+\s*\**\s*'
    r'(CONFIRMED|REFUTED|LATENT|BY-DESIGN)([^\n]*)',
    re.M | re.I)


def collect_verdicts():
    """Verdicts written by the independent verifier agents (T019)."""
    out = {}
    for f in sorted(FIND.glob("verdicts-*.md")):
        batch = f.stem.split("-")[-1]
        txt = read(f)
        for m in VERDICT_HDR.finditer(txt):
            fid, verdict, tail = m.group(1), m.group(2).upper(), m.group(3)
            qual = ""
            t = tail.lower()
            if "part" in t:
                qual = " (in part)"
            elif "edge" in t:
                qual = " (edge)"
            start = m.end()
            nxt = re.search(r'^##\s', txt[start:], re.M)
            body = txt[start:start + (nxt.start() if nxt else 4000)]
            out[fid] = dict(verdict=verdict + qual, batch=batch,
                            scen=field(body, "Scenario", "Failure scenario"),
                            scope=field(body, "Scope"))
    return out


def disposition(v):
    if v is None:
        return "pending"
    base = v["verdict"].split(" ")[0]
    return {"CONFIRMED": "fix candidate (scope test T020)",
            "REFUTED": "no change",
            "LATENT": "deferred — latent",
            "BY-DESIGN": "no change — by design"}.get(base, "pending")


def gen_findings(rows):
    verdicts = collect_verdicts()
    out = ["| ID | Persp. | Claim | Verdict | Batch | Disposition |",
           "|---|---|---|---|---|---|"]
    counts = {}
    for r in rows:
        if r["withdrawn"]:
            v, batch, disp = "WITHDRAWN (by author)", "—", "no change"
        else:
            rec = verdicts.get(r["id"])
            v = rec["verdict"] if rec else "pending"
            batch = rec["batch"] if rec else "—"
            disp = disposition(rec)
        counts[v.split(" ")[0]] = counts.get(v.split(" ")[0], 0) + 1
        out.append("| {id} | {p} | {claim} | **{v}** | {b} | {d} |".format(
            id=r["id"], p=r["persp"], claim=cell(r["claim"], 150),
            v=v, b=batch, d=disp))
    live = [r for r in rows if not r["withdrawn"]]
    out.append("")
    tally = ", ".join(f"{k} {v}" for k, v in sorted(counts.items()))
    out.append(f"**{len(live)} live findings** ({len(rows)} raised, "
               f"{len(rows) - len(live)} withdrawn by their author). Verdicts: {tally}. "
               "Every verdict is written by an independent refute-first verifier that did "
               "not raise the finding (research R5); only CONFIRMED findings may drive a "
               "code change (FR-006/FR-007).")
    return "\n".join(out)


def gen_ledger(rows, seen):
    out = ["| L-row | Perspectives | Proposed disposition (verbatim from the perspective) |",
           "|---|---|---|"]
    order = sorted(seen, key=lambda k: int(re.sub(r'\D', '', k) or 0))
    for key in order:
        for k, p, cells in rows:
            if k != key:
                continue
            disp = " · ".join(cell(c, 200) for c in cells[1:] if c)
            out.append(f"| {key} | {p} | {disp} |")
    out.append("")
    out.append(f"**{len(order)} of 89 ledger rows re-examined** so far "
               f"({sum(len(v) for v in seen.values())} dispositions from "
               f"{len({p for v in seen.values() for p in v})} perspectives).")
    return "\n".join(out)


def gen_contracts(rows, seen):
    out = ["| Obligation | Persp. | Verdict + evidence |", "|---|---|---|"]
    for key in sorted(seen):
        for k, p, cells in rows:
            if k != key:
                continue
            rest = " · ".join(cell(c, 260) for c in cells[1:] if c)
            out.append(f"| {key} | {p} | {rest} |")
    out.append("")
    out.append(f"**{len(seen)} contract obligations carry a verdict.**")
    return "\n".join(out)


def splice(report, name, content):
    b, e = f"<!-- BEGIN GENERATED: {name} -->", f"<!-- END GENERATED: {name} -->"
    block = f"{b}\n{content}\n{e}"
    if b in report and e in report:
        return re.sub(re.escape(b) + r".*?" + re.escape(e), lambda _: block, report, flags=re.S)
    return None


def main():
    findings = collect_findings()
    led_rows, led_seen = collect_tables("Ledger rows", r'\bL\d{2}\b')
    con_rows, con_seen = collect_tables("Contract verdicts", r'\bB\d+\.\d+\b')

    report = read(REPORT)
    for name, content in (("findings", gen_findings(findings)),
                          ("ledger", gen_ledger(led_rows, led_seen)),
                          ("contracts", gen_contracts(con_rows, con_seen))):
        new = splice(report, name, content)
        if new is None:
            print(f"!! markers for '{name}' not found in review-report.md", file=sys.stderr)
            return 1
        report = new
    REPORT.write_text(report, encoding="utf-8")

    live = [f for f in findings if not f["withdrawn"]]
    per = {}
    for f in live:
        per[f["persp"]] = per.get(f["persp"], 0) + 1
    print(f"findings: {len(findings)} raised, {len(live)} live " +
          "(" + ", ".join(f"{k}:{v}" for k, v in sorted(per.items())) + ")")
    print(f"ledger rows re-examined: {len(led_seen)}/89")
    print(f"contract obligations with a verdict: {len(con_seen)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
