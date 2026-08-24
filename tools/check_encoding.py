#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pavel Stupka
# SPDX-License-Identifier: GPL-2.0-or-later
"""Guard against the file-name display-encoding defect class (feature 042).

A file name is stored as UTF-8 everywhere in this application.  It is destroyed
only at the last step before it is drawn, and it goes wrong in exactly two ways:

  * LOSSY        - the name is converted down to the machine's legacy codepage,
                   so every character that codepage cannot hold becomes '?'.
                   One '?' per UTF-16 code unit, so one emoji costs two.
  * UNINTERPRETED- the name's UTF-8 bytes are drawn as if they were legacy text,
                   so 'c' with caron becomes two mojibake characters.

Both were reported by users, twice, on different surfaces.  This script exists so
the third occurrence fails the build instead of reaching someone's screen.

Rules
-----
cp-acp-display   A name is converted with WideCharToMultiByte(CP_ACP, ...) on a
                 path that ends in a display.  Lossy by construction: forbidden.

mixed-composition
                 A printf-family call whose FORMAT comes from LoadStr() (legacy
                 codepage) and whose arguments include a file name (UTF-8), with
                 the result handed to a message box.  The composed string is then
                 not valid UTF-8, so CMessageBox refuses its own wide drawing
                 path and falls back to the legacy one - which renders the
                 template correctly and the name as mojibake.
                 NOTE: an English build cannot reproduce this.  English resources
                 are pure ASCII, ASCII is valid UTF-8, so the composed string
                 converts cleanly and looks correct.  Only localized builds show
                 it.  That is why this rule is static rather than a runtime test.

ansi-template-number
                 A grouped number composed with an ANSI LoadStr() template
                 (feature 067).  NumberToStr()/PrintDiskSize() output carries
                 the UTF-8 locale thousands separator (feature 041), so an
                 ANSI template makes the composed string invalid UTF-8 and a
                 Sal*U8 sink draws it through the legacy path - the Czech
                 no-break-space separator renders as "A-circumflex + space".
                 Two shapes: a printf-family call whose format is LoadStr()
                 and whose arguments call NumberToStr()/PrintDiskSize()
                 directly, and ExpandPluralString() fed a LoadStr() bytes-
                 plural template (those templates receive a grouped number by
                 construction).  Use LoadStrU8(), or PrintDiskSize(..., TRUE).

dead-dispinfow   A dialog handles LVN_GETDISPINFOW but never sends NF_REQUERY.
                 Such a handler can never run: a list view asks its parent for a
                 notification format during its own creation, which is before
                 WM_INITDIALOG, and CDialog::CDialogProc only attaches the dialog
                 object at WM_INITDIALOG.  The query therefore goes unanswered,
                 DefDlgProc replies from IsWindowUnicode(parent) - FALSE for every
                 dialog here - and the control settles on ANSI permanently.
                 This rule would have caught the reported Find defect on the day
                 the handler was written.

Draft rules (feature 068, report-only, `--draft`)
-------------------------------------------------
The product-wide encoding review (specs/068-encoding-regression-review)
found fourteen defect classes with no guard at all.  The rules below describe
the ones a regex can see.  They run ONLY with --draft, never in the strict
build gate, until the review has classified their hits and annotated the
legitimate ones; a rule is then promoted into RULES (see the review report).

ansi-api-on-utf8-path
                 A UTF-8 name or path (an identifier that holds one by
                 convention, or the result of a known UTF-8 producer) passed
                 to an un-suffixed - i.e. ANSI - Win32 file/shell/process/
                 registry call.  The core is built without UNICODE, so
                 CreateFile( is CreateFileA(.  Features 058, 062 and 063 each
                 fixed instances of this shape; nothing enforced it.  Use the
                 Sal* facade (src/common/salfileio.h) or SalU8ToW + the W call.

cp-acp-utf8-source
                 MultiByteToWideChar(CP_ACP, ...) applied to a UTF-8 value -
                 the narrow-to-wide mirror of cp-acp-display (features 058 and
                 062).  The else-branch of a SalU8ToW attempt is not flagged.

ansi-tooltip-handler
                 An ANSI TTN_NEEDTEXT / TTN_GETDISPINFO handler that composes
                 UTF-8 text into its ANSI szText (feature 067, viewer offset
                 tooltip).  Register the tool with TOOLINFOW/TTM_ADDTOOLW and
                 handle the W notification.

strict-probe-rejects-wtf8
                 A raw MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ...)
                 probe outside salunicode.cpp.  Since feature 066 internal names
                 are WTF-8; a raw strict probe misroutes a surrogate-bearing
                 name into the ANSI branch.  Probe with SalU8ToW instead, or
                 mark a non-name site as reviewed.

lossy-lenient-at-intake
                 A lenient WideCharToMultiByte(CP_UTF8, 0, ...) conversion, or
                 a SalU8ToWDisplay result handed to an operational call (file,
                 registry, process).  U+FFFD baked into a name destroys its
                 identity (feature 066).

acp-byte-table-on-name
                 A 256-entry ACP byte table (LowerCase[], IsAlpha[], ...)
                 indexed by a byte of a UTF-8 name: every byte >= 0x80 - all
                 of UTF-8 - is folded through the active code page, so two
                 spellings of one accented name compare unequal while
                 unrelated accented letters compare equal (cluster B-2).

                 Feature 069 narrowed this rule: it used to also flag a name
                 byte compared as a control character through a *signed* char,
                 a premise that is void - the product compiles with /J, so
                 plain char is unsigned (feature 068, ledger L07).

missed-twin      LoadStr(IDS_X) where IDS_X is already loaded with LoadStrU8
                 somewhere else.  The remaining ANSI twin is either composed
                 with UTF-8 (the feature 067 zip.cpp defect) or genuinely
                 ANSI-only - either way it must be decided, not overlooked.

Suppressing a finding
---------------------
Put a marker on the offending line or the line above it:

    // encoding-check: allow <rule-id> - <reason>

The reason is mandatory and is what a future reader will be judged on.  Blanket
suppression of a whole file is deliberately not supported.

Usage
-----
    python tools/check_encoding.py              # report, always exit 0
    python tools/check_encoding.py --strict     # exit 1 if anything is found
    python tools/check_encoding.py --rule mixed-composition
    python tools/check_encoding.py --format list   # machine-readable
    python tools/check_encoding.py --draft         # report-only draft rules (068)
    python tools/check_encoding.py --draft --rule missed-twin
"""

import argparse
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
SRC = REPO / "src"

# Subtrees that are not the main application binary.  Plugins are excluded by
# policy (feature 042 must not change plugin behaviour); the rest are separate
# executables with their own text handling.
EXCLUDED = ("plugins/", "saltests/", "tserver/", "shellext/", "setup/",
            "salmon/", "salopen/", "translator/", "reglib/", "common/dep/")

SUPPRESS = re.compile(r'//\s*encoding-check:\s*allow\s+([a-z0-9-]+)\s*-\s*(\S.*)')

PRINTF = re.compile(r'\b(?:sprintf|sprintf_s|_snprintf|_snprintf_s|wsprintf)\s*\(')
FMT_IS_LOADSTR = re.compile(r'\b(?:sprintf|sprintf_s|_snprintf|_snprintf_s|wsprintf)\s*\('
                            r'[^;]*?\bLoadStr\s*\(')
# An argument that carries a file or directory name.  Deliberately broad: the
# strict version of this pattern missed fileswnb.cpp:815 - the very defect that
# prompted this feature - because the name arrived from an accessor called
# GetEquivalentPairNoticeName().  Recorded in research.md R5.
NAME_ARG = re.compile(r'[A-Za-z_]*(?:[Nn]ame|[Pp]ath|[Ff]ile|[Dd]ir|[Mm]ask|[Aa]rchive)[A-Za-z_]*')
MSGBOX = re.compile(r'\b(?:SalMessageBox|SalMessageBoxEx|ShowMessageBox)\b')

CP_ACP_W2A = re.compile(r'\bWideCharToMultiByte\s*\(\s*CP_ACP\b')
# A CP_ACP conversion counts as a display path only when its result is handed
# straight to something that draws.  This keeps the many legitimate OLE, shell
# and clipboard boundary conversions out of the rule.
DISPLAY_SINK = re.compile(r'\bpszText\b|\bDrawText\s*\(|\bTextOut\s*\(|\bExtTextOut\s*\(|'
                          r'\bSetWindowText\s*\(|\bSetDlgItemText\s*\(|SB_SETTEXT')

DISPINFOW = re.compile(r'\bLVN_GETDISPINFOW\b')
REQUERY = re.compile(r'\bNF_REQUERY\b')

# --- feature 067 -------------------------------------------------------------
# Number formatters whose output carries the UTF-8 locale separator; composing
# it with an ANSI LoadStr() template is the Drive Information defect shape.
NUMBER_ARG = re.compile(r'\b(?:NumberToStr2?|PrintDiskSize)\s*\(')
EXPAND_PLURAL = re.compile(r'\bExpandPluralString\s*\(')
# the bytes-plural templates receive a NumberToStr() result by construction
ANSI_BYTES_TEMPLATE = re.compile(r'\bLoadStr\s*\(\s*IDS_\w*BYTES')

# --- feature 043 -------------------------------------------------------------
# The rules above describe the two defects feature 042 had in hand. They passed
# cleanly while three further defects of the same class were present, because
# they were written around those two SHAPES rather than around the defect: a
# UTF-8 value reaching a call that reads bytes as legacy single-byte text.
#
# The rules below describe the defect. Sources known to produce UTF-8:
UTF8_SOURCE = re.compile(
    r'\b('
    r'LoadStrU8|GetErrorText|NumberToStr|PrintDiskSize|PointToLocalDecimalSeparator|'
    r'SalGetLocaleInfoU8|SalGetDateFormatU8|SalGetTimeFormatU8|GetLanguageName|'
    r'ExpandPluralBytesFilesDirs|AlterFileName|GetZIPArchive|GetPath'
    r')\s*\(')
# ... and identifiers that hold one by convention in this codebase.
UTF8_IDENT = re.compile(
    r'\b('
    r'\w*[Ff]ile[Nn]ame\w*|\w*[Ff]ullName\w*|formatedFileName|editName|'
    r'\w*[Pp]ath\b|\w*[Pp]ath[A-Z]\w*|f->Name|item->Name|oneFile->Name|'
    r'\w*[Ll]inkName\w*|\w*[Aa]rchive[A-Za-z]*|subject|Subject|'
    # plugin metadata holds UTF-8 by CONTRACT, not convention (feature 052):
    # specs/052-fix-plugin-name-encoding/contracts/plugin-metadata-encoding.md
    r'plugin->Name|Plugin->Name|p->Name|pluginData->Name|pluginName|'
    r'\w+->Description|\w+->Copyright|\w+->Extensions|\w+->ChDrvMenuFSItemName|'
    # tooltip text is UTF-8 by CONTRACT - normalized at SetToolTipText intake
    # (feature 063, contracts/filelist-text-encoding.md C3)
    r'ToolTipText'
    r')\b')

# Legacy sinks: the byte-oriented A-variants. The W and Sal*U8 forms are safe.
SINK_LISTVIEW = re.compile(r'\bListView_SetItemText\s*\(')
SINK_WNDTEXT = re.compile(r'(?<!Sal)\b(?:SetWindowText|SetDlgItemText)\s*\(')
SINK_STATUS = re.compile(r'\bSB_SETTEXT\b(?!W)')
SINK_COMBO = re.compile(r'\bCB_ADDSTRING\b(?!W)')
# the ANSI clipboard entry point keeps CP_ACP semantics for the plugin ABI;
# core code holding UTF-8 must use CopyTextToClipboardU8/W (feature 063,
# specs/063-fix-filelist-encoding/contracts/filelist-text-encoding.md C2)
SINK_CLIPBOARD = re.compile(r'\bCopyTextToClipboard\s*\(')

RULES = ("cp-acp-display", "mixed-composition", "dead-dispinfow",
         "utf8-to-legacy-sink", "ansi-template-caption", "ansi-template-number",
         # promoted by feature 068 after the review classified every hit:
         "strict-probe-rejects-wtf8",  # 0 hits - pure forward guard for the 066 invariant
         "lossy-lenient-at-intake",    # 2 hits, both annotated fail-safes
         "ansi-tooltip-handler")       # 1 hit, annotated (deferred ledger L06)

# Rules that live in scan_draft() but are now enforced: main() must merge them
# into the strict run, otherwise they would be listed and never executed - the
# dead-dispinfow shape, in the guard itself.
PROMOTED_FROM_DRAFT = ("strict-probe-rejects-wtf8", "lossy-lenient-at-intake",
                       "ansi-tooltip-handler")

# --- feature 068 draft rules (report-only until promoted) ------------------
# Un-suffixed Win32 calls that TAKE a name or path.  Output-side calls
# (GetModuleFileName, GetTempPath, GetCurrentDirectory, ...) are producers of
# ANSI text and are reviewed by hand, not by this rule.
ANSI_NAME_API = re.compile(
    r'(?<![A-Za-z_])(?:'
    r'CreateFile|FindFirstFile|FindFirstFileEx|GetFileAttributes|GetFileAttributesEx|'
    r'SetFileAttributes|DeleteFile|MoveFile|MoveFileEx|CopyFile|CopyFileEx|'
    r'CreateDirectory|RemoveDirectory|SetCurrentDirectory|GetFullPathName|'
    r'GetLongPathName|GetShortPathName|GetTempFileName|GetVolumeInformation|'
    r'GetDiskFreeSpaceEx|GetDriveType|ExpandEnvironmentStrings|SearchPath|'
    r'FindFirstChangeNotification|CreateHardLink|GetFileVersionInfo|'
    r'GetFileVersionInfoSize|LoadLibrary|LoadLibraryEx|SHFileOperation|'
    r'SHGetFileInfo|ShellExecute|ShellExecuteEx|SHBrowseForFolder|'
    r'GetOpenFileName|GetSaveFileName|ExtractIconEx|CreateProcess|'
    r'SetEnvironmentVariable|QueryDosDevice|WNetGetConnection|WNetAddConnection2|'
    r'RegOpenKeyEx|RegCreateKeyEx|RegQueryValueEx|RegSetValueEx|RegDeleteValue|'
    r'RegDeleteKey|RegEnumKeyEx|RegEnumValue|_access|_stat|fopen|_mkdir|_rmdir|'
    r'_unlink|remove|rename'
    r')A?\s*\(')
# the debug handle-tracking shims wrap every API once; they are one boundary,
# reviewed by hand, not a finding per wrapper
SHIM_FILES = ("common/handles.cpp",)
CP_ACP_A2W = re.compile(r'\bMultiByteToWideChar\s*\(\s*CP_ACP\b')
TOOLTIP_ANSI = re.compile(r'\b(?:TTN_NEEDTEXT|TTN_GETDISPINFO)\b(?!W)')
MB2WC_CALL = re.compile(r'\bMultiByteToWideChar\s*\(')
LENIENT_W2U8 = re.compile(r'\bWideCharToMultiByte\s*\(\s*CP_UTF8\s*,\s*0\b')
DISPLAY_CONV = re.compile(r'\bSalU8ToWDisplay\w*\s*\(')
OPERATIONAL_SINK = re.compile(
    r'\b(?:Sal(?:CreateFile|CreateFileNH|DeleteFile|MoveFile|MoveFileEx|CopyFile|'
    r'GetFileAttributes|SetFileAttributes|FindFirstFile|CreateDirectory|'
    r'RemoveDirectory|RegSetValueExW8|CreateProcess|ShellExecuteEx)|'
    r'CreateFileW|DeleteFileW|MoveFileW|MoveFileExW|CopyFileW|SHFileOperationW|'
    r'CreateProcessW|ShellExecuteW|ShellExecuteExW|SetCurrentDirectoryW|'
    r'RegSetValueExW)\s*\(')
BYTE_TABLE = re.compile(r'\b(?:IsNotAlphaNorNum|IsAlpha|LowerCase|UpperCase)\s*\[')
LOADSTRU8_ID = re.compile(r'\bLoadStrU8\s*\(\s*(IDS_\w+)')
LOADSTR_ID = re.compile(r'(?<![A-Za-z_])LoadStr\s*\(\s*(IDS_\w+)')

# Still report-only: each is blocked on a fix deferred to its own feature (the
# 068 review report; specs/069-finish-encoding-fixes/research.md R8).
# acp-byte-table-on-name is the feature-069 successor of signed-char-name-byte:
# the signed-char half of that rule rested on a void premise (/J makes plain
# char unsigned, feature 068 ledger L07); the byte-table half is the real defect
# and waits for the group B-2 work.
DRAFT_RULES = ("ansi-api-on-utf8-path", "cp-acp-utf8-source",
               "acp-byte-table-on-name", "missed-twin")


class Finding:
    def __init__(self, rule, path, line, text):
        self.rule, self.path, self.line, self.text = rule, path, line, text

    def __str__(self):
        return f"{self.path}:{self.line}: [{self.rule}] {self.text.strip()[:110]}"


def sources():
    for p in sorted(SRC.rglob("*.cpp")):
        rel = p.relative_to(SRC).as_posix()
        if rel.startswith(EXCLUDED):
            continue
        yield p, rel


def call_text(lines, i, call=PRINTF):
    """Return exactly the call (printf-family by default) starting on line i.

    A fixed line window is not good enough: it swallows the next statement and
    reports calls whose format is a plain literal as if it came from LoadStr().
    Match parentheses instead, so the text is the call and nothing else.
    """
    m = call.search(lines[i])
    if not m:
        return None
    # text from the call NAME onwards (the name is needed for FMT_IS_LOADSTR to
    # match), across at most 12 lines, cut at the call's closing paren
    blob = " ".join([lines[i][m.start():]] + lines[i + 1:i + 12])
    open_at = blob.index("(")
    depth = 0
    for pos in range(open_at, len(blob)):
        if blob[pos] == '(':
            depth += 1
        elif blob[pos] == ')':
            depth -= 1
            if depth == 0:
                return blob[:pos + 1]
    return blob


WIDE_ATTEMPT = re.compile(
    r'\bSalU8ToW\w*\s*\(|\bSetWindowTextW\s*\(|\bSetDlgItemTextW\s*\(|'
    r'\bLVM_SETITEMTEXTW\b|\bSB_SETTEXTW\b|\bDrawTextW\s*\(|\bCB_ADDSTRING\b.*W\b')


def wide_fallback(lines, i):
    """True when this legacy call is the ELSE branch of a wide attempt.

    The codebase's established shape is "convert to UTF-16 and use the W call;
    if the value is not valid UTF-8, fall back to the legacy call". The legacy
    call in that shape is deliberate and correct - flagging it would mean
    flagging roughly sixty correct sites and training everyone to ignore the
    guard, which is worse than not having one.
    """
    if lines[i].lstrip().startswith("//"):
        return True                      # a comment mentioning the call
    if re.search(r'\(LPARAM\)\s*""|,\s*""\s*\)', lines[i]):
        return True                      # clearing a field, nothing to mangle
    window = "".join(lines[max(0, i - 10):i + 1])
    if not WIDE_ATTEMPT.search(window):
        return False
    # the wide attempt must be paired with an else / fallback for THIS call
    tail = "".join(lines[max(0, i - 3):i + 1])
    return bool(re.search(r'\belse\b', tail) or re.search(r'legacy|fallback|transitional', tail, re.I))


def suppressed(lines, i, rule):
    """A marker on the offending line, or anywhere in the comment block above it.

    The block is walked rather than just the previous line so a reason long
    enough to be worth reading can wrap onto continuation lines without the
    marker silently ceasing to apply.
    """
    if 0 <= i < len(lines):
        m = SUPPRESS.search(lines[i])
        if m and m.group(1) == rule:
            return True
    j = i - 1
    while j >= 0 and lines[j].lstrip().startswith("//"):
        m = SUPPRESS.search(lines[j])
        if m and m.group(1) == rule:
            return True
        j -= 1
    return False


def scan(only=None):
    findings = []
    for path, rel in sources():
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        joined = "\n".join(lines)

        # --- dead-dispinfow: a whole-file property -------------------------
        if only in (None, "dead-dispinfow"):
            if DISPINFOW.search(joined) and not REQUERY.search(joined):
                i = next(i for i, l in enumerate(lines) if DISPINFOW.search(l))
                if not suppressed(lines, i, "dead-dispinfow"):
                    findings.append(Finding(
                        "dead-dispinfow", rel, i + 1,
                        "LVN_GETDISPINFOW handler in a file that never sends NF_REQUERY "
                        "- the handler cannot run"))

        for i, ln in enumerate(lines):
            # --- mixed-composition ----------------------------------------
            if only in (None, "mixed-composition") and PRINTF.search(ln):
                stmt = call_text(lines, i) or ""
                # the FORMAT argument must be the LoadStr() one: it is the first
                # argument after the destination buffer
                if FMT_IS_LOADSTR.search(stmt):
                    after = stmt[stmt.find("LoadStr("):]
                    if NAME_ARG.search(after) and MSGBOX.search(" ".join(lines[i:i + 14])):
                        if not suppressed(lines, i, "mixed-composition"):
                            findings.append(Finding("mixed-composition", rel, i + 1, ln))

            # --- ansi-template-number (feature 067) -----------------------
            if only in (None, "ansi-template-number"):
                if PRINTF.search(ln):
                    stmt = call_text(lines, i) or ""
                    if FMT_IS_LOADSTR.search(stmt) and NUMBER_ARG.search(stmt):
                        if not suppressed(lines, i, "ansi-template-number"):
                            findings.append(Finding("ansi-template-number", rel, i + 1, ln))
                if EXPAND_PLURAL.search(ln):
                    stmt = call_text(lines, i, EXPAND_PLURAL) or ""
                    if ANSI_BYTES_TEMPLATE.search(stmt):
                        if not suppressed(lines, i, "ansi-template-number"):
                            findings.append(Finding("ansi-template-number", rel, i + 1, ln))

            # --- cp-acp-display -------------------------------------------
            if only in (None, "cp-acp-display") and CP_ACP_W2A.search(ln):
                window = " ".join(lines[max(0, i - 6):i + 8])
                if DISPLAY_SINK.search(window):
                    if not suppressed(lines, i, "cp-acp-display"):
                        findings.append(Finding("cp-acp-display", rel, i + 1, ln))

            # --- utf8-to-legacy-sink (feature 043) -------------------------
            # A value produced by a known UTF-8 source, or held in an
            # identifier that carries one, handed to a byte-oriented sink.
            if only in (None, "utf8-to-legacy-sink"):
                sink = (SINK_LISTVIEW.search(ln) or SINK_WNDTEXT.search(ln) or
                        SINK_STATUS.search(ln) or SINK_COMBO.search(ln) or
                        SINK_CLIPBOARD.search(ln))
                if sink:
                    # the value is either on this line, or assigned to the
                    # variable this line passes, a few lines above
                    back = " ".join(lines[max(0, i - 8):i + 1])
                    if (UTF8_SOURCE.search(ln) or UTF8_IDENT.search(ln) or
                            UTF8_SOURCE.search(back)):
                        if (not wide_fallback(lines, i) and
                                not suppressed(lines, i, "utf8-to-legacy-sink")):
                            findings.append(Finding("utf8-to-legacy-sink", rel, i + 1, ln))

            # --- ansi-template-caption (feature 043) -----------------------
            # A composed caption (CTruncatedString::Set) whose template came
            # from the ANSI LoadStr and whose substituted value is a name.
            # The consumers have a wide path; an ANSI template is what stops
            # them taking it, so the whole caption falls back and the NAME
            # becomes mojibake while the localized words survive.
            if only in (None, "ansi-template-caption") and re.search(r'\.Set\s*\(', ln):
                back = " ".join(lines[max(0, i - 6):i + 2])
                if ("LoadStr(" in back and "LoadStrU8(" not in back and
                        UTF8_IDENT.search(ln)):
                    if not suppressed(lines, i, "ansi-template-caption"):
                        findings.append(Finding("ansi-template-caption", rel, i + 1, ln))

    return findings


def scan_draft(only=None):
    """Feature 068 draft rules.  Separate from scan() on purpose: the strict
    build gate must not change while these are being tuned on the inventory.
    Same suppression marker, same Finding shape."""
    findings = []

    # missed-twin needs a whole-tree pre-pass: which ids are loaded as UTF-8?
    u8_ids = set()
    if only in (None, "missed-twin"):
        for path, rel in sources():
            u8_ids.update(LOADSTRU8_ID.findall(
                path.read_text(encoding="utf-8", errors="replace")))

    for path, rel in sources():
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        shim = rel in SHIM_FILES
        for i, ln in enumerate(lines):
            if ln.lstrip().startswith(("//", "*", "/*")):
                continue

            # --- ansi-api-on-utf8-path ---------------------------------
            if only in (None, "ansi-api-on-utf8-path") and not shim and ANSI_NAME_API.search(ln):
                stmt = call_text(lines, i, ANSI_NAME_API) or ln
                if UTF8_IDENT.search(stmt) or UTF8_SOURCE.search(stmt):
                    if not suppressed(lines, i, "ansi-api-on-utf8-path"):
                        findings.append(Finding("ansi-api-on-utf8-path", rel, i + 1, ln))

            # --- cp-acp-utf8-source ------------------------------------
            if only in (None, "cp-acp-utf8-source") and CP_ACP_A2W.search(ln):
                stmt = call_text(lines, i, CP_ACP_A2W) or ln
                if ((UTF8_IDENT.search(stmt) or UTF8_SOURCE.search(stmt)) and
                        not wide_fallback(lines, i)):
                    if not suppressed(lines, i, "cp-acp-utf8-source"):
                        findings.append(Finding("cp-acp-utf8-source", rel, i + 1, ln))

            # --- ansi-tooltip-handler ----------------------------------
            if only in (None, "ansi-tooltip-handler") and TOOLTIP_ANSI.search(ln):
                window = " ".join(lines[i:i + 40])
                if UTF8_SOURCE.search(window) or UTF8_IDENT.search(window):
                    if not suppressed(lines, i, "ansi-tooltip-handler"):
                        findings.append(Finding("ansi-tooltip-handler", rel, i + 1, ln))

            # --- strict-probe-rejects-wtf8 -----------------------------
            if (only in (None, "strict-probe-rejects-wtf8") and
                    rel != "common/salunicode.cpp" and MB2WC_CALL.search(ln)):
                stmt = call_text(lines, i, MB2WC_CALL) or ln
                if "MB_ERR_INVALID_CHARS" in stmt:
                    if not suppressed(lines, i, "strict-probe-rejects-wtf8"):
                        findings.append(Finding("strict-probe-rejects-wtf8", rel, i + 1, ln))

            # --- lossy-lenient-at-intake -------------------------------
            if only in (None, "lossy-lenient-at-intake") and rel != "common/salunicode.cpp":
                if LENIENT_W2U8.search(ln):
                    if not suppressed(lines, i, "lossy-lenient-at-intake"):
                        findings.append(Finding("lossy-lenient-at-intake", rel, i + 1, ln))
                elif DISPLAY_CONV.search(ln) and OPERATIONAL_SINK.search(" ".join(lines[i:i + 6])):
                    if not suppressed(lines, i, "lossy-lenient-at-intake"):
                        findings.append(Finding("lossy-lenient-at-intake", rel, i + 1, ln))

            # --- acp-byte-table-on-name --------------------------------
            if only in (None, "acp-byte-table-on-name"):
                if BYTE_TABLE.search(ln) and UTF8_IDENT.search(ln):
                    if not suppressed(lines, i, "acp-byte-table-on-name"):
                        findings.append(Finding("acp-byte-table-on-name", rel, i + 1, ln))

            # --- missed-twin -------------------------------------------
            if only in (None, "missed-twin"):
                m = LOADSTR_ID.search(ln)
                if m and m.group(1) in u8_ids:
                    if not suppressed(lines, i, "missed-twin"):
                        findings.append(Finding("missed-twin", rel, i + 1, ln))

    return findings


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--strict", action="store_true",
                    help="exit 1 when anything is found (used by build.cmd)")
    ap.add_argument("--rule", choices=RULES + DRAFT_RULES, help="scan a single rule")
    ap.add_argument("--format", choices=("report", "list"), default="report")
    ap.add_argument("--draft", action="store_true",
                    help="run the report-only draft rules (feature 068); "
                         "never exits 1, never used by build.cmd")
    args = ap.parse_args()
    if args.rule in DRAFT_RULES:
        args.draft = True

    if args.draft:
        findings = scan_draft(args.rule)
        rules = DRAFT_RULES
    else:
        findings = scan(args.rule)
        # the promoted rules are implemented in scan_draft(); run them too
        if args.rule is None or args.rule in PROMOTED_FROM_DRAFT:
            findings += [f for f in scan_draft(args.rule)
                         if f.rule in PROMOTED_FROM_DRAFT]
        rules = RULES

    if args.format == "list":
        for f in findings:
            print(f)
    else:
        by_rule = {}
        for f in findings:
            by_rule.setdefault(f.rule, []).append(f)
        print("=" * 72)
        print(" check_encoding.py - file-name display-encoding guard (feature 042)"
              + (" - DRAFT rules (feature 068)" if args.draft else ""))
        print("=" * 72)
        for rule in rules:
            if args.rule and rule != args.rule:
                continue
            hits = by_rule.get(rule, [])
            print(f"\n[{rule}] {len(hits)} finding(s)")
            for f in hits:
                print(f"  {f}")
        print(f"\nTOTAL: {len(findings)} finding(s)")
        if findings and args.strict and not args.draft:
            print("\nA file name would be destroyed on its way to the screen.")
            print("Fix the site, or suppress it with a reason:")
            print("    // encoding-check: allow <rule-id> - <reason>")

    return 1 if (findings and args.strict and not args.draft) else 0


if __name__ == "__main__":
    sys.exit(main())
