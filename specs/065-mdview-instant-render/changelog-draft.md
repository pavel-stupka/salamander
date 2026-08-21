# CHANGELOG draft — feature 065 (paste under the next release version)

Per the constitution, the entry below enters `CHANGELOG.md` in the same
change that bumps `VERSINFO_SALAMANDER_MINORB` + `VERSINFO_BUILDNUMBER`
(`src/plugins/shared/spl_vers.h`), `MyAppVersion`
(`setup/tandemcommander.iss`) and the version line in `CLAUDE.md`.

### Added

- **Markdown files open instantly after the first view in a session.** The
  Markdown viewer (F3) renders through the Windows WebView2 engine, and the
  engine used to shut down whenever the last viewer window closed — so
  almost every open paid the engine's start-up wait as a visibly empty
  window before content appeared. The engine is now kept ready in the
  background from the first Markdown view until the application exits: only
  the first view of a session (at most) shows the start-up wait; following
  links between documents and every later F3 render immediately. Sessions
  that never view a Markdown file are completely unaffected — nothing is
  started ahead of time. A new option in Plugins Manager → Markdown
  Viewer → Configure ("Keep the rendering engine ready for instant
  viewing", enabled by default) trades the instant opening back for the
  lower memory use of the old behavior when disabled.

### Changed

- The Markdown viewer's browser cache moved to a product-wide location
  (`%LOCALAPPDATA%\Tandem Commander\WebView2`); the old `mdview.WebView2`
  cache folder is removed automatically on first use. The cache holds no
  user data.
