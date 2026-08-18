# Contract: Path Encoding in the Icon / Overlay / Change-Monitoring Pipeline

**Feature**: 058-fix-cloud-status-icons · **Status**: implemented (2026-08-18) —
all governed call sites below verified against the final diff
**Scope**: core (`salamand.vcxproj`) — icon reader, shell icon overlays,
snooper; plugin-facing `CSalamanderGeneral::GetFileIcon`.

## Rules

1. **Panel paths and item names are UTF-8** (`CFilesWindow::Path`,
   `CFileData::Name` — feature 004/027). Any code that hands them to a
   Windows API MUST convert to UTF-16 and call the W variant.
2. **Conversion helper**: `SalU8ToW` (fixed buffer) or `SalU8ToWAlloc`
   (heap), from `src/common/salunicode.h`.
3. **Fallback**: if the input is not valid UTF-8 (helper fails/returns 0),
   fall back to the legacy `CP_ACP` conversion or ANSI API — never fail the
   operation outright. This keeps legacy plugin callers (ACP strings, see
   `src/pluglegacy.h`) working through shared entry points.
4. **No ANSI shell/file APIs on panel-derived paths**: specifically
   `FindFirstChangeNotificationA`, `MultiByteToWideChar(CP_ACP, …)` on
   UTF-8 input, and `SHGetFileInfoA` are non-compliant for these strings.
5. **Offsets are per-encoding**: a byte offset into a UTF-8 string MUST NOT
   be used as a WCHAR offset into its converted form (RC1's second bug).
   Wide name-append positions are derived from the converted wide prefix.

## Governed call sites (after this feature)

| Site | API reached | Compliance mechanism |
|------|-------------|----------------------|
| `src/fileswn1.cpp` icon-reader prefix build | `IsMemberOf` (via `GetIconOverlayIndex`) | `SalU8ToW` prefix + wide-derived `wName` |
| `src/shiconov.cpp` `GetIconOverlayIndex` name part | `IsMemberOf` | `SalU8ToW` + ACP fallback (pre-existing) |
| `src/snooper.cpp` `AddDirectory` / `ChangeDirectory` (3 sites) | `FindFirstChangeNotificationW` | convert final pointer (after `MakeCopyWithBackslashIfNeeded`, which swaps by reference) |
| `src/common/handles.cpp` | HANDLES tracking | W overload registered under `__hoFindFirstChangeNotification` |
| `src/geticon.cpp` `SHILCreateFromPath` | `IShellFolder::ParseDisplayName` | `SalU8ToWAlloc`; NULL → legacy `CP_ACP` branch |
| `src/geticon.cpp` `SalSHGetFileInfoIcons` | `SHGetFileInfoW` | pre-existing compliant wrapper (reference) |

## Plugin-facing guarantee

`CSalamanderGeneral::GetFileIcon(const char* path, …)`
(`spl_gen.h:2972`): signature and observable behavior for existing callers
are unchanged. Interpretation order for `path`: valid UTF-8 → UTF-16;
otherwise legacy CP_ACP (matches the modern-plugin UTF-8 contract of
interface 104+ while tolerating legacy ACP callers). No
`LAST_VERSION_OF_SALAMANDER` change.

## Non-goals

- Explorer's storage-provider *Status column* properties (different
  mechanism; out of scope by spec assumption).
- Google Drive handler gating / name-list refresh (`shiconov.cpp`) — see
  research.md R6; modern DriveFS handlers intentionally run un-gated.
- Long-path (`> MAX_PATH`) icon retrieval — feature 027 routes such paths
  away from icon reading before these sites are reached.
