# Delta Manifest

**Feature**: 068-encoding-regression-review · baseline tag `v0.1.4` (7b6137f), HEAD c577ff3 on 068-encoding-regression-review

## Unreleased delta `v0.1.4..HEAD` (features 065, 066, 067) — P6 line-level input

```
c577ff3 [Spec Kit] Implementation progress — [067] fix garbled numbers in drive/size dialogs
f57a851 [Spec Kit] Implementation progress — [066] fix operations on unpaired-surrogate names
3245809 [Spec Kit] Implementation progress — [065] instant markdown viewer display

 src/common/salfileio.cpp                   |   3 +
 src/common/salfileio.h                     |   9 +-
 src/common/salunicode.cpp                  | 246 ++++++++++++++-
 src/common/salunicode.h                    |  23 +-
 src/consts.h                               |  22 +-
 src/dialogs2.cpp                           |  10 +-
 src/dialogs3.cpp                           |  17 +-
 src/fileswn8.cpp                           |   9 +-
 src/gui.cpp                                |  10 +-
 src/plugins/mdview/IMPLEMENTATION_NOTES.md |  58 ++++
 src/plugins/mdview/lang/lang.rc2           |  11 +
 src/plugins/mdview/mdview.cpp              |  61 +++-
 src/plugins/mdview/mdview.h                |  48 +--
 src/plugins/mdview/mdview.rh2              |   4 +
 src/plugins/mdview/viewer.cpp              |  25 +-
 src/plugins/mdview/webview.cpp             | 478 +++++++++++++++++++++++++----
 src/plugins/mdview/webview.h               |  33 +-
 src/plugins/shared/spl_gen.h               |  11 +
 src/salamdr4.cpp                           |  40 ++-
 src/salamdr6.cpp                           |  89 ++++--
 src/saltests/saltests.cpp                  | 275 ++++++++++++++++-
 src/viewer3.cpp                            |  31 +-
 src/zip.cpp                                |   5 +-
 tools/check_encoding.py                    |  42 ++-
 24 files changed, 1357 insertions(+), 203 deletions(-)
```

## Release 0.1.1 → 0.1.2 delta `v0.1.1..v0.1.2` (features 052–055; ledger L87 — encoding perspective returned nothing structured)

```
 src/common/salunicode.cpp      |  46 +++++++
 src/common/salunicode.h        |  22 ++++
 src/dialogs5.cpp               |  39 +++---
 src/fileswn7.cpp               |  12 +-
 src/plugins.h                  |  11 ++
 src/plugins/sftp/dialogs.cpp   | 267 +++++++++++++++++++++++++++++++++++++----
 src/plugins/sftp/lang/lang.rc2 | 236 ++++++++++++++++++++----------------
 src/plugins/sftp/session.cpp   | 216 ++++++++++++++++++++++++---------
 src/plugins/sftp/sftp.cpp      | 139 ++++++++++++++++-----
 src/plugins/sftp/sftp.h        |  30 +++--
 src/plugins/shared/spl_vers.h  |   7 +-
 src/plugins1.cpp               |  70 +++++------
 src/plugins2.cpp               |  10 +-
 src/saltests/saltests.cpp      |  76 ++++++++++++
 14 files changed, 880 insertions(+), 301 deletions(-)
```
