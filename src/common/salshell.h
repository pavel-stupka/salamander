// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************
//
// salshell.h
//
// Command Shell presets (feature 071-configurable-command-shell).
//
// The Command Shell command (Num /, Ctrl+/, Commands > Command Shell, the
// toolbar button) opens a user-chosen program: one of the presets below,
// located at its standard installation places, or a custom program with
// arguments. This module holds the preset table and the locate algorithm; it
// talks to the machine only through CSalShellProbe, so saltests drives it
// with a fake probe. All strings are UTF-8 (feature 004 house rules).
//

// stable ids - stored in the configuration ("Command Shell Preset"); append only
enum CSalShellPreset
{
    sspCommandPrompt = 0, // %COMSPEC% - the default and the pre-071 behaviour
    sspWindowsPowerShell = 1,
    sspPowerShell7 = 2,
    sspWindowsTerminal = 3,
    sspGitBash = 4,
    sspCustom = 5, // user program + arguments from the configuration
    sspCount = 6
};

// size of the Custom arguments buffer (parity with USRMNUARGS_MAXLEN of the User Menu)
#define SAL_SHELL_ARGS_MAX 32772

// GetEnvironmentVariable with UTF-8 in/out (wide read, so a value outside the
// ANSI code page survives). Same return contract as the API: 0 = not set,
// >= bufSize = required size including the terminator (nothing stored),
// otherwise the length stored (without the terminator).
DWORD SalGetEnvVarU8(const char* name, char* u8Buf, DWORD bufSize);

// what the locate algorithm asks of the machine; saltests supplies a fake
class CSalShellProbe
{
public:
    virtual ~CSalShellProbe() {}
    // TRUE when 'u8Path' names an existing file (not a directory)
    virtual BOOL FileExists(const char* u8Path) const = 0;
    // value of the environment variable 'name'; FALSE when unset or too long
    virtual BOOL GetEnv(const char* name, char* u8Buf, int bufSize) const = 0;
    // REG_SZ/REG_EXPAND_SZ 'value' (NULL = the default value) under root\subKey,
    // environment references expanded; FALSE when the key or value is missing
    virtual BOOL RegReadString(HKEY root, const char* subKey, const char* value,
                               char* u8Buf, int bufSize) const = 0;
    // the same for the 'index'-th subkey of root\subKey; FALSE only when there
    // is no such subkey - a subkey without the value yields TRUE and ""
    virtual BOOL RegSubKeyString(HKEY root, const char* subKey, int index, const char* value,
                                 char* u8Buf, int bufSize) const = 0;
    // install folder of the first package of the MSIX package family 'family'
    virtual BOOL GetPackagePath(const char* family, char* u8Buf, int bufSize) const = 0;
};

// the real machine: W environment/registry APIs, SalGetFileAttributes
class CSalShellOsProbe : public CSalShellProbe
{
public:
    virtual BOOL FileExists(const char* u8Path) const;
    virtual BOOL GetEnv(const char* name, char* u8Buf, int bufSize) const;
    virtual BOOL RegReadString(HKEY root, const char* subKey, const char* value,
                               char* u8Buf, int bufSize) const;
    virtual BOOL RegSubKeyString(HKEY root, const char* subKey, int index, const char* value,
                                 char* u8Buf, int bufSize) const;
    virtual BOOL GetPackagePath(const char* family, char* u8Buf, int bufSize) const;
};

// Finds the program of 'preset' (sspCommandPrompt..sspGitBash) on this machine:
// walks the preset's candidate locations in order (contracts/shell-presets.md)
// and returns the first one that exists as a file. 'probe' NULL = the real
// machine. Returns FALSE for sspCustom, an unknown id, or when no candidate
// exists; 'u8Path' then holds "". On success 'u8Path' is an absolute UTF-8 path.
BOOL SalShellLocatePreset(int preset, const CSalShellProbe* probe, char* u8Path, int pathSize);

// arguments that make the preset open in the process's working directory
// ("-d ." for Windows Terminal, "" for the others); never contain placeholders
const char* SalShellPresetArguments(int preset);

// short ASCII key for traces and tests ("cmd", "powershell", "pwsh", "wt",
// "git-bash", "custom"); "" for an unknown id
const char* SalShellPresetKey(int preset);
