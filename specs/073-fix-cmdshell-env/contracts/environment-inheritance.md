# Contract: Environment inheritance of programs started by Tandem Commander

**Feature**: 073-fix-cmdshell-env · **Date**: 2026-08-31 · Shelf design — this
contract describes the guarantee the feature would establish; today's code
satisfies it on the reporting machine by measurement (spec V6), not by
construction.

## Guarantee

**G1 — One environment for every launch.** A program started by Tandem
Commander — the Command Shell command (all presets, Custom), `Enter`/double-
click on a file, the User Menu, the command line box, external viewers and
editors — receives the environment block of the Tandem Commander process,
unchanged, plus only the per-drive current-directory bookkeeping
(`=A:`…`=Z:`) that `CMainWindow::SetDefaultDirectories` has always set. No
launch path constructs, filters or edits an environment block.

**G2 — Startup parity.** Immediately after startup, the Tandem Commander
process's block (bookkeeping excluded) is identical — same names, same values
code unit by code unit — to the block it inherited from its parent, whether
*Keep environment variables updated to system values* is on or off.

**G3 — Change propagation (option on).** After Windows broadcasts
`WM_SETTINGCHANGE` with `"Environment"`, the block equals the system's
regenerated environment, with every variable of the inherited-only set
(names present at startup that the regeneration does not produce) present
with its startup value. Regenerated values win for all other names;
nothing is deleted.

**G4 — Exact values.** Names and values are handled as UTF-16 throughout;
a value outside the Windows code page (or containing a lone surrogate)
reaches the started program unchanged.

**G5 — Degradation.** If the shell32 regeneration export is unavailable, G2
holds (block = inherited) and G3 is not attempted (behaviour identical to
today's failure path).

**Not covered (by design, documented in the manual):** the parent's own
environment (a Tandem Commander started from the installer's *Launch*
checkbox or a terminal carries that parent's environment); elevation; the
shell host's session identity (`WT_SESSION`, `WT_PROFILE_ID`); a Windows
Terminal version that does not forward the launcher's environment.

## Module surface (`src/common/salenv.h`) — interface only

```cpp
// wide environment block as sorted entries; '='-prefixed names excluded
class CSalEnvBlock;                       // Load(const WCHAR* doubleNulBlock); Count(); Name(i); Value(i); Find(name)

// what the module asks of the machine; saltests supplies a fake
class CSalEnvOs
{
public:
    virtual ~CSalEnvOs() {}
    virtual BOOL GetBlock(CSalEnvBlock& out) const = 0;              // GetEnvironmentStringsW
    virtual BOOL Set(const WCHAR* name, const WCHAR* valueOrNull) = 0; // SetEnvironmentVariableW
    virtual BOOL Regenerate() = 0;                                    // shell32 RegenerateUserEnvironment(&prev, TRUE); FALSE if unavailable
};

// startup: snapshot A, regenerate, snapshot B, compute inheritedOnly, restore A (G2)
// returns FALSE when regeneration is unavailable (inheritedOnly left empty, block untouched — G5)
BOOL SalEnvInitAndRestore(CSalEnvOs& os, CSalEnvBlock& inheritedOnlyOut);

// on WM_SETTINGCHANGE "Environment": regenerate, then re-add inheritedOnly with startup values (G3)
BOOL SalEnvRegenerateKeepingInherited(CSalEnvOs& os, const CSalEnvBlock& inheritedOnly);

// pure helpers (unit-tested with fakes)
void SalEnvInheritedOnly(const CSalEnvBlock& a, const CSalEnvBlock& b, CSalEnvBlock& out);   // names(A) \ names(B), A values
BOOL SalEnvRestore(const CSalEnvBlock& a, const CSalEnvBlock& b, CSalEnvOs& os);            // make the process block == A
```

`src/salamdr7.cpp` keeps `InitEnvironmentVariablesDifferences()` and
`RegenEnvironmentVariables()` with their current names and call sites,
delegating to the two entry points above with `CSalEnvOsReal`.

## Observable invariants (what tests assert)

| Id | Invariant | Where checked |
|----|-----------|---------------|
| I1 | `SalEnvRestore(A, B)` leaves the block equal to A (names + values), including a U+010D value, a lone-surrogate value, and a name differing from B only in case | saltests, fake OS |
| I2 | A name only in B is removed by restore; a name only in A is kept | saltests, fake OS |
| I3 | `SalEnvRegenerateKeepingInherited` after a changed B' yields B' ∪ inheritedOnly(A values), B' values winning on overlap | saltests, fake OS |
| I4 | `=`-prefixed entries are never read into a block nor set by the module | saltests, fake OS |
| I5 | A child started through `SalCreateProcess(NULL environment)` sees the parent's block exactly, non-ACP marker included | saltests, real OS, self-spawn `--dump-env` |
| I6 | The running product's block equals Explorer's right after startup | manual — `evidence/penv.ps1` |
| I7 | Each preset / Custom / `.bat`-via-Enter: `set` in the started shell equals the plainly started one up to the allow-list | manual — quickstart matrix |
| I8 | After `setx TC_TEST …` while running, a newly opened shell shows the value; inherited-only variables survive | manual — quickstart |
