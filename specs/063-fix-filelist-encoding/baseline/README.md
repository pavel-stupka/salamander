# Pre-fix baseline (T001)

Captured 2026-08-19 on branch `063-fix-filelist-encoding` at commit `15f70c6`
(pre-fix state), for the SC-004/FR-008 regression guard.

## ASCII-output baseline — analytical

For an ASCII-only selection with the default line `$(FileName)$(CRLF)`, the
pre-fix pipeline emits, per item, exactly the name bytes + `0D 0A`:

- expansion: `DoExpandVarString` copies value bytes verbatim (`salamdr2.cpp:1038`);
  for ASCII there is no padding (`maxSizes` only used with `:max`/`:N`);
- file leg: `WriteFile` of those bytes (`fileswn6.cpp:201`) — no BOM, no transform;
- clipboard leg: `CF_TEXT` = the raw bytes; `CF_UNICODETEXT` =
  `MultiByteToWideChar(CP_ACP)` of them — for bytes < 0x80 this equals the
  identity mapping on every Windows ANSI code page.

`ascii-expected.txt` in this directory holds the expected byte-exact output for
the fixture's ASCII files (sorted as the panel lists them, name+CRLF each).
The fixed build must produce identical bytes for the same selection (the fix
adds `SalIsASCII` fast paths precisely to guarantee this).

A runtime capture from the pre-fix binary was not automated (the flow is
GUI-interactive); the analytical derivation above is the baseline of record,
and T033 verifies the fixed build against `ascii-expected.txt` at runtime.

## Pre-fix defect record (Czech UI, reproduced by the user, confirmed in code)

1. Ctrl+M → clipboard: Czech names paste as mojibake
   (`AddUnicodeToClipboard` decodes UTF-8 as CP_ACP, `salamdr4.cpp:1027`).
2. Ctrl+M dialog → "nápověda k řádku" hint: garbled diacritics
   (CP1250 `LoadStr` text fails the tooltip's strict UTF-8 probe → `DrawTextA`
   with font-charset-dependent mapping, `tooltip.cpp:298-333`).
3. Ctrl+M dialog → "Soubor:" radio clipped (cx=27 in all languages,
   `translations/czech/salamand.slt:1153` / master `lang.rc:1193`).
