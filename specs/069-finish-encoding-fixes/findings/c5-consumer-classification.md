# T033 — consumer classification for group C5 (application locations)

**Why this exists**: both of the fixes feature 068 had rejected were DC-09 —
a strict facade meeting a legacy producer, because the fixer converted one link
and left an adjacent one on the code page. C5 is the group where that trap
lives, so the classification is written down *before* any code changes
(`contracts/fix-protocol.md` A2).

## The producers

`grep -rn "GetModuleFileName(" src/*.cpp` finds **37** call sites. Only **three**
buffers feed a strict-UTF-8 consumer; the rest are consistent legacy chains that
work today and **must not change**:

| Buffer | Producer | Consumers | Class |
|---|---|---|---|
| `CurrentHelpDir` | `mainwnd3.cpp:171` | `DirExists` (**strict facade**), `helpPath` → `HANDLES_Q(FindFirstFile(...))` (**ANSI**), `HtmlHelp(NULL, helpPath, …)` (**ANSI**, = `HtmlHelpA`) | **mixed — must be converted as a whole** |
| `ConfigurationName` | `salamdr1.cpp:3566`, `:3669` (and the `-C` command line at `:3605`, which is **already UTF-8**) | `FileExists` (**strict**), `ImportConfiguration` → `HANDLES_Q(CreateFile(fileName,…))` (**ANSI**), the Save Configuration prompt (`mainwnd3.cpp:2844`, F-P2-13), `strrchr`/`memcpy` (byte ops, encoding-agnostic) | **mixed — and already inconsistent today**: the `-C` path is UTF-8 and meets that ANSI `CreateFile`, so an accented `-C` path fails *now* |
| `data->Buffer` (`$(SalDir)`, `$(SalPath)`) | `execute.cpp:828`, `:919` | the composed command line → `SalCreateProcess` (**strict UTF-8**) | **strict — convert the producer** |

### The legacy chains that must stay ANSI

Verified by reading each consumer; every one of these is a *consistent* ANSI
pair and works today, including on a non-ASCII install path:

- plugin loading: `plugins1.cpp:1728`, `:2166`, `plugins2.cpp:1282`, `:1934`,
  `:2961`, `:3253`, `:3396`, `:3493`, `:3512` — the path ends in an ANSI
  `LoadLibrary` for the `.spl`, and `plugins.h`'s own metadata contract records
  `DLLName` as **not normalized** for exactly that reason.
- language modules: `salamdr2.cpp:3190`, `:3196`, `:3217`, `:3219`, `:3229`,
  `:3238`, `:3254` — `lang\*.slg` + `LoadLibrary`.
- conversion tables: `codetbl.cpp:319`, `:455` — `convert\*.cfg` read with the
  narrow CRT.
- the rest: `bugreprt.cpp:1720` (diagnostic text), `dialogs2.cpp:832/959`,
  `dialogs5.cpp:741/889`, `dialogs6.cpp:207`, `salamdr1.cpp:4052/4352`,
  `salamdr4.cpp:1935`, `salmoncl.cpp:163/172`, `shiconov.cpp:54`,
  `svg.cpp:108`, `mainwnd3.cpp:2903` — each stays with its ANSI consumer.

## What this means for the fix

1. **F-P1-10 help**: converting `CurrentHelpDir` alone is *not* enough and would
   be a regression risk — the same function then hands the UTF-8 path to an ANSI
   `FindFirstFile` and to `HtmlHelpA`. All three hops move together:
   `GetModuleFileNameW` + `SalWToU8`, the enumeration through the facade, and
   `HtmlHelpW` with a converted path (legacy `HtmlHelp` as the fallback).
2. **F-P1-10 config + F-P2-13**: `ConfigurationName` becomes UTF-8 from every
   producer, `ImportConfiguration`'s `CreateFile` becomes `SalCreateFile`, and
   only *then* may the Save Configuration prompt become `LoadStrU8`. This also
   repairs the `-C <accented path>` case, which is broken today.
3. **F-P1-10 `$(SalDir)`**: producer only — its single consumer is already
   strict.
4. **F-P1-08** (`SHGetFolderPath`) is independent of the above: read and write
   side convert together (`GetOurPathInRoamingAPPDATA` +
   `CreateOurPathInRoamingAPPDATA`), because the ANSI pair can currently write a
   directory it cannot then read back.
5. **F-P1-24** (browse dialogs) keeps its ANSI edit-line round trip: the
   verifier refuted the display claim, so `WM_GETTEXT`/`WM_SETTEXT` stay as they
   are and only the value stored into the configuration changes. Any sub-case
   that cannot be made correct without converting the dialog itself is deferred
   to cluster B-1 with that reason.

## Status

Analysis complete; the code changes are the last group of this feature and are
scheduled after the batch-4 review lands (they touch `mainwnd3.cpp` and
`mainwnd4.cpp`, which that review is reading).
