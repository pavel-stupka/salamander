"""Context for the machine translator: *where* a UI string is used.

Sending a bare label like ``Host:`` or ``&New`` to a translation engine loses
the only thing that decides the right word. DeepL rendered ``Host:`` as the
Czech "Moderátor" (a talk-show host), ``Key file`` as "Klíčový" (the adjective
"key", cut off), and ``&New`` as "Novinka" (a news item) -- each defensible for
the isolated word and wrong for a dialog field in an SFTP client.

Everything needed to fix that is already in the repository: the ``.slt``
carries the dialog/menu/string-table each row belongs to, and the module's
``.rh`` files name every control. ``IDT_HOSTADDRESS`` says "static label for a
host address", ``IDB_NEWBOOKMARK`` says "button that creates a new bookmark".
This module turns that into an English sentence per section, which is passed to
the engine as translation context (context is guidance only -- it is never
itself translated).

Context is built per section rather than per row so a dialog needs one request
instead of one per control, and so the engine also sees the *neighbouring*
labels, which is what disambiguates a one-word button.
"""

from __future__ import annotations

import re
from pathlib import Path

from . import REPO_ROOT
from .config import Module
from .slt import Row, Section, SltFile

__all__ = ["load_symbols", "section_context", "humanize_symbol"]

_DEFINE = re.compile(r"^\s*#define\s+([A-Za-z_][A-Za-z0-9_]*)\s+(\d+)\s*(?://.*)?$")

# Control-id prefixes used across this code base, and what they say about the
# role of the string. Longest prefix wins, so IDS_MENU_ beats IDS_.
_ROLES: list[tuple[str, str]] = [
    ("IDT_", "static text labelling the neighbouring input field"),
    ("IDE_", "editable text field"),
    ("IDB_", "push button"),
    ("IDC_", "check box or radio button"),
    ("IDL_", "list box"),
    ("IDI_", "icon or image"),
    ("IDD_", "dialog"),
    ("IDS_MENU_", "menu command"),
    ("IDS_ERR_", "error message shown to the user"),
    ("IDS_CONFIRM_", "confirmation question"),
    ("IDS_COL_", "file-list column heading"),
    ("IDS_HOSTKEY_", "text about the server's SSH host key"),
    ("IDS_", "message or label"),
    ("IDM_", "menu command"),
    ("IDOK", "OK button"),
    ("IDCANCEL", "Cancel button"),
]

# What each module is, in one clause -- the domain the words live in.
_DOMAINS: dict[str, str] = {
    "sftp": "an SFTP/SSH file-transfer client (connecting to remote servers, "
            "browsing and transferring files)",
    "ftp": "an FTP client (connecting to remote servers, browsing and "
           "transferring files)",
    "zip": "an archive plugin that opens and creates ZIP archives",
    "checksum": "a plugin that computes and verifies file checksums",
    "mdview": "a Markdown file viewer",
    "codeview": "a source-code viewer with syntax highlighting (colour "
                "schemes, language selection, text encodings)",
    "pictview": "an image viewer",
    "salamand": "a two-panel file manager for Windows",
    "7zip": "an archive plugin that creates, browses and extracts "
            "7-Zip archives",
    "tar": "an archive plugin that browses and extracts tar, gzip and "
           "bzip2 archives",
    "uncab": "an archive plugin that browses and extracts Windows CAB "
             "archives",
    "uniso": "a plugin that browses and extracts CD/DVD disk images (ISO)",
    "dbviewer": "a viewer for dBase, FoxPro and CSV database files "
                "(tabular data)",
    "peviewer": "a viewer showing headers, sections, imports and exports "
                "of Windows executable (PE) files",
    "diskmap": "a tool that shows disk-space usage of folders as a treemap",
    "filecomp": "a tool that visually compares two text or binary files "
                "and shows their differences",
    "renamer": "a batch file renamer with pattern and regular-expression "
               "rules",
    "portables": "a plugin that browses portable devices such as phones, "
                 "cameras and media players",
    "regedt": "a plugin that browses and edits the Windows Registry",
    "undelete": "a tool that recovers deleted files from FAT and NTFS "
                "volumes",
    "folders": "a plugin that browses Windows shell folders such as "
               "Desktop, Control Panel and the Recycle Bin",
}


def load_symbols(module: Module) -> dict[int, str]:
    """Map resource id -> symbol name for one module.

    Reads the module's ``lang.rh`` plus the sibling ``.rh``/``.rh2`` headers
    (control ids for translatable dialogs live in ``<module>.rh2`` and in the
    shared ``statics.rh2``). A duplicate id keeps the first name seen, which is
    the module's own file -- the shared header only adds generic statics.
    """
    out: dict[int, str] = {}
    candidates: list[Path] = [module.rh_path]
    base = module.rh_path.parent.parent  # src/plugins/<name> or src
    candidates += sorted(base.glob("*.rh2")) + sorted(base.glob("*.rh"))
    candidates.append(REPO_ROOT / "src" / "plugins" / "shared" / "statics.rh2")

    for path in candidates:
        if not path.is_file():
            continue
        for line in path.read_text(encoding="utf-8-sig", errors="replace").splitlines():
            m = _DEFINE.match(line)
            if m is None:
                continue
            name, value = m.group(1), int(m.group(2))
            out.setdefault(value, name)
    return out


# Resource symbols run words together (IDB_NEWBOOKMARK), which reads as
# "newbookmark" and tells the engine nothing. Split on these known UI words,
# longest match first, so the context says "new bookmark".
_WORDS = sorted(
    """
    accept access address append attributes bookmark bookmarks browse cancel
    changed check chmod colview column compression confirm connect connection
    copy create current default delete descr description dir directory disk
    download duplicate edit empty enter error existing file files filter
    fingerprint folder generic group host hostkey icon incomplete initial
    interval invalid item items keepalive key label link list local log logs
    login menu missing mtime name new number octal once open operation
    organize overwrite owner packet passphrase password path pause permission
    permissions plugin port prompt recurse remote remove rename resume
    retries retry rights save saved server session settings show size skip
    stop symlink text time timeout title too trust type unlock upload user
    username value view warn window write
    advanced archive batch battery buffer capacity case chart checksum
    checksums cluster codepage compare comparison compress compressed counter
    damaged data database decrypt deleted device devices difference
    differences digits drive drives encoding encrypt expand export
    expression extension extensions extract field fields found free header
    headers hive horizontal ignore image import level lost lower manual mask
    masks media method mixed panel panels partition pattern patterns preview
    progress properties record records recycle registry replace results
    scan scanning search section sections sector select selected signature
    solid space split storage subdirs subkey table target track tree
    treemap undo unpack upper used verify version vertical viewer volume
    volumes whitespace wipe
    shell preset cmd found args arguments custom intro hint program
    """.split(),
    key=len,
    reverse=True,
)


def _split_words(body: str) -> str:
    """``newbookmark`` -> ``new bookmark`` (greedy longest known word first)."""
    out: list[str] = []
    i = 0
    while i < len(body):
        for word in _WORDS:
            if body.startswith(word, i):
                out.append(word)
                i += len(word)
                break
        else:
            # Unknown run: keep it whole, up to the next known word boundary.
            j = i + 1
            while j < len(body) and not any(body.startswith(w, j) for w in _WORDS):
                j += 1
            out.append(body[i:j])
            i = j
    return " ".join(out)


def humanize_symbol(symbol: str) -> str:
    """``IDB_NEWBOOKMARK`` -> ``new bookmark``; ``IDS_ERR_KEYUNLOCK`` -> ``key unlock``."""
    body = symbol
    for prefix in ("IDS_MENU_", "IDS_ERR_", "IDS_CONFIRM_", "IDS_COL_", "IDS_HOSTKEY_",
                   "IDT_", "IDE_", "IDB_", "IDC_", "IDL_", "IDI_", "IDD_", "IDS_", "IDM_"):
        if body.startswith(prefix):
            body = body[len(prefix):]
            break
    parts = [_split_words(p.lower()) for p in body.split("_") if p]
    return " ".join(p for p in parts if p).strip()


def _role(symbol: str) -> str:
    for prefix, role in _ROLES:
        if symbol.startswith(prefix) or symbol == prefix:
            return role
    return "user-interface string"


def _domain(module_name: str) -> str:
    return _DOMAINS.get(module_name, f"the {module_name} plugin of a Windows file manager")


def _plain(text: str) -> str:
    """Strip accelerator markers and XML metacharacters.

    The context travels in the same request as the texts, and that request is
    parsed with ``tag_handling=xml`` -- an unescaped ``&`` from a label like
    ``&Host:`` makes DeepL reject the whole batch. Accelerators carry no meaning
    for a human reader either, so they go.
    """
    out = text.replace("&&", "\x00").replace("&", "").replace("\x00", "&")
    return out.replace("&", "and").replace("<", "(").replace(">", ")")


def _dialog_title(section: Section) -> str | None:
    """The caption row's text, i.e. the dialog's own title."""
    for row in section.rows:
        if row.entry_id is None:
            return row.text or None
    return None


def section_context(
    module_name: str,
    section: Section,
    symbols: dict[int, str],
    max_items: int = 40,
) -> str:
    """One English context sentence for every row of ``section``.

    The result names the product, the containing dialog or menu, and lists the
    section's entries as ``role: text`` so a one-word label is read together
    with its neighbours.
    """
    domain = _domain(module_name)

    if section.kind == "DIALOG":
        title = _dialog_title(section)
        where = f'the "{_plain(title)}" dialog' if title else f"dialog {section.number}"
        head = (f"User-interface strings from {where} of {domain}. "
                f"Translate each as a short UI label, not as prose. Items:")
    elif section.kind == "MENU":
        head = (f"Menu commands of {domain}. Translate each as a short menu "
                f"command. Items:")
    else:
        head = (f"Messages and labels of {domain}. Translate each as short UI "
                f"text. Items:")

    items: list[str] = []
    for row in section.rows:
        if not row.text.strip():
            continue
        symbol = symbols.get(row.entry_id) if row.entry_id is not None else None
        if row.entry_id is None:
            desc = "dialog title"
        elif symbol:
            desc = f"{_role(symbol)} for {humanize_symbol(symbol)}"
        else:
            desc = "user-interface string"
        items.append(f'"{_plain(row.text)}" = {desc}')
        if len(items) >= max_items:
            break

    return head + " " + "; ".join(items)


def build_contexts(module: Module, template: SltFile) -> dict[tuple[str, int], str]:
    """``{(section kind, number): context}`` for a whole module."""
    symbols = load_symbols(module)
    return {
        s.key: section_context(module.name, s, symbols)
        for s in template.sections
    }
