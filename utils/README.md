# Tandem Commander utilities

## migrate-altap-settings.cmd

A one-time helper that copies your settings from **Altap Salamander** (or the
older Servant Salamander) into **Tandem Commander**. It is a single file —
there is nothing to install.

### How to get and run it

1. Download the single file `migrate-altap-settings.cmd` from this directory
   (open the file in the repository and use *Raw* / *Download raw file*).
2. Close **both** Tandem Commander and Altap Salamander (the utility refuses
   to run while either is open — Tandem Commander saves its settings on exit
   and would overwrite the transferred data).
3. Double-click the file and follow the prompts: it finds your Altap
   Salamander configuration(s), shows what can be transferred with item
   counts, and you tick the groups you want. **Nothing is written until you
   explicitly confirm.**

Requires stock Windows 11 (uses the built-in Windows PowerShell 5.1) — no
installation, no other downloads. Supported sources: Altap Salamander
2.5–4.0; older Servant Salamander configurations are detected too and
transfer best-effort (some groups are not offered for them).

### What it transfers

| Setting group | Notes |
|---------------|-------|
| Directory hot paths | including names and visibility |
| FTP connection bookmarks | plus proxy servers, custom server types and stored passwords (see below) |
| User menu commands | commands pointing into the Altap installation folder are flagged in the summary |
| Viewer & editor associations | entries bound to a specific Altap plugin are skipped and named in the summary |
| Confirmation prompts | |
| Colors & panel highlighting | color scheme, custom colors, highlight rules |
| Panel view templates | column layouts |
| Internal viewer settings | |
| Per-drive default directories | |
| General configuration | options from the Configuration dialog |
| Other plugin settings | ZIP, 7-Zip, PictView, File Comparator, Renamer, Checksum, Database Viewer, Disk Map, PE Viewer, Registry Editor, UnCAB, Undelete, UnISO |

### What it never transfers (and why)

- **Archiver settings** (custom packers/unpackers, archive associations) —
  Tandem Commander deliberately rebuilds these from defaults.
- **Window and panel session state** (positions, open paths, sort order) and
  **history lists** — transient state, not worth carrying over.
- **Settings of plugins Tandem Commander does not ship** (e.g. UnRAR,
  Automation, Windows Mobile).
- Anything installation-specific: language choice, plugin registrations,
  toolbar button layouts, the old product's internal version markers.

Everything skipped is listed in the closing summary with a reason — nothing
is dropped silently.

### Stored FTP passwords

- Passwords saved **without** a master password transfer as-is and just work.
- If your Altap Salamander used a **master password**, the encrypted
  passwords and the master-password settings transfer together — unlock them
  in Tandem Commander with the **same** master password.
- If Tandem Commander already has its **own** master password, the encrypted
  passwords cannot be carried over (the utility never asks for master
  passwords); those bookmarks transfer without the password and the summary
  lists each one so you can re-enter them.

### Undo / restore

Before writing anything, the utility saves a complete backup of your current
Tandem Commander settings next to the script (or in `Documents`):

- `tc-settings-backup-<timestamp>.reg` — the backup itself
- `tc-settings-restore-<timestamp>.cmd` — **double-click to undo the whole
  migration** (restores the exact pre-migration state, or removes the
  settings if none existed before)
- `tc-migration-summary-<timestamp>.txt` — what was transferred and what was
  skipped, with reasons

Your Altap Salamander configuration is opened read-only and is never
modified, so the old program keeps working unchanged no matter what.

### Re-running

Running the utility again is safe: a selected group is replaced wholesale by
the source's current content (never duplicated), and groups you don't select
are left untouched.

---

Developer notes: the test harness lives in `test/` (`run_migration_tests.cmd`
drives the wizard end-to-end against fixture hives under
`HKCU\Software\TCMigTest`). Design documents:
`specs/057-altap-settings-migration/` in the repository.
