// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <windows.h>
#include <appmodel.h>

#include "salunicode.h"
#include "salpath.h"
#include "salfileio.h"
#include "salshell.h"

//*****************************************************************************
//
// helpers
//

// copies 'src' into 'dst' (always terminated); FALSE when it does not fit
static BOOL CopyStr(char* dst, int dstSize, const char* src)
{
    if (dst == NULL || dstSize <= 0)
        return FALSE;
    if (src == NULL)
        src = "";
    size_t len = strlen(src);
    if (len + 1 > (size_t)dstSize)
    {
        dst[0] = 0;
        return FALSE;
    }
    memcpy(dst, src, len + 1);
    return TRUE;
}

// base + '\' + relative; a trailing backslash of 'base' is not doubled
// (installers record "C:\Program Files\Git\" as well as "C:\Program Files\Git");
// 'relative' NULL/"" returns 'base' itself. FALSE when empty or too long.
static BOOL JoinPath(const char* base, const char* relative, char* buf, int bufSize)
{
    if (base == NULL || base[0] == 0 || buf == NULL || bufSize <= 0)
        return FALSE;
    size_t baseLen = strlen(base);
    while (baseLen > 0 && base[baseLen - 1] == '\\')
        baseLen--;
    if (baseLen == 0)
        return FALSE;
    size_t relLen = (relative == NULL) ? 0 : strlen(relative);
    if (relLen == 0)
    {
        if (baseLen + 1 > (size_t)bufSize)
            return FALSE;
        memcpy(buf, base, baseLen);
        buf[baseLen] = 0;
        return TRUE;
    }
    if (baseLen + 1 + relLen + 1 > (size_t)bufSize)
        return FALSE;
    memcpy(buf, base, baseLen);
    buf[baseLen] = '\\';
    memcpy(buf + baseLen + 1, relative, relLen + 1);
    return TRUE;
}

//*****************************************************************************
//
// SalGetEnvVarU8
//

DWORD SalGetEnvVarU8(const char* name, char* u8Buf, DWORD bufSize)
{
    WCHAR nameW[256];
    if (name == NULL || name[0] == 0 || SalU8ToW(name, -1, nameW, _countof(nameW)) == 0)
    {
        SetLastError(ERROR_ENVVAR_NOT_FOUND);
        return 0;
    }
    DWORD needW = GetEnvironmentVariableW(nameW, NULL, 0); // incl. terminator; 0 = not set
    if (needW == 0)
        return 0;
    WCHAR* valW = (WCHAR*)malloc(needW * sizeof(WCHAR));
    if (valW == NULL)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }
    DWORD lenW = GetEnvironmentVariableW(nameW, valW, needW);
    if (lenW == 0 || lenW >= needW) // removed or grown meanwhile - treat as not set
    {
        free(valW);
        SetLastError(ERROR_ENVVAR_NOT_FOUND);
        return 0;
    }
    int u8Size = (int)needW * 3 + 1; // worst case 3 bytes per UTF-16 unit
    char* valU8 = (char*)malloc(u8Size);
    if (valU8 == NULL)
    {
        free(valW);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }
    int conv = SalWToU8(valW, -1, valU8, u8Size);
    free(valW);
    if (conv == 0)
    {
        free(valU8);
        SetLastError(ERROR_ENVVAR_NOT_FOUND);
        return 0;
    }
    DWORD lenU8 = (DWORD)strlen(valU8);
    DWORD ret;
    if (u8Buf == NULL || lenU8 + 1 > bufSize)
        ret = lenU8 + 1; // required size, nothing stored
    else
    {
        memcpy(u8Buf, valU8, lenU8 + 1);
        ret = lenU8;
    }
    free(valU8);
    return ret;
}

//*****************************************************************************
//
// CSalShellOsProbe
//

BOOL CSalShellOsProbe::FileExists(const char* u8Path) const
{
    if (u8Path == NULL || u8Path[0] == 0)
        return FALSE;
    DWORD attrs = SalGetFileAttributes(u8Path);
    // an App Execution Alias (wt.exe, pwsh.exe from the Store) is a 0-byte
    // reparse point: it reports attributes like any file and CreateProcessW
    // resolves it natively, so it counts as existing
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

BOOL CSalShellOsProbe::GetEnv(const char* name, char* u8Buf, int bufSize) const
{
    if (u8Buf == NULL || bufSize <= 0)
        return FALSE;
    DWORD res = SalGetEnvVarU8(name, u8Buf, (DWORD)bufSize);
    if (res == 0 || res >= (DWORD)bufSize)
    {
        u8Buf[0] = 0;
        return FALSE;
    }
    return TRUE;
}

// reads a REG_SZ/REG_EXPAND_SZ value of an open key as UTF-8 (expanded)
static BOOL ReadStringValueW(HKEY key, const char* value, char* u8Buf, int bufSize)
{
    if (u8Buf == NULL || bufSize <= 0)
        return FALSE;
    u8Buf[0] = 0;
    WCHAR valueW[256];
    if (value == NULL || value[0] == 0)
        valueW[0] = 0; // the default value
    else if (SalU8ToW(value, -1, valueW, _countof(valueW)) == 0)
        return FALSE;
    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueExW(key, valueW, NULL, &type, NULL, &size) != ERROR_SUCCESS)
        return FALSE;
    if ((type != REG_SZ && type != REG_EXPAND_SZ) || size == 0 || size > 1024 * 1024)
        return FALSE;
    WCHAR* data = (WCHAR*)malloc(size + 2 * sizeof(WCHAR));
    if (data == NULL)
        return FALSE;
    if (RegQueryValueExW(key, valueW, NULL, &type, (BYTE*)data, &size) != ERROR_SUCCESS)
    {
        free(data);
        return FALSE;
    }
    data[size / sizeof(WCHAR)] = 0; // registry strings may lack the terminator
    const WCHAR* str = data;
    WCHAR* expanded = NULL;
    if (type == REG_EXPAND_SZ)
    {
        DWORD need = ExpandEnvironmentStringsW(data, NULL, 0);
        if (need > 0)
        {
            expanded = (WCHAR*)malloc(need * sizeof(WCHAR));
            if (expanded != NULL && ExpandEnvironmentStringsW(data, expanded, need) != 0)
                str = expanded;
        }
    }
    BOOL ok = str[0] != 0 && SalWToU8(str, -1, u8Buf, bufSize) != 0;
    if (!ok)
        u8Buf[0] = 0;
    free(expanded);
    free(data);
    return ok;
}

BOOL CSalShellOsProbe::RegReadString(HKEY root, const char* subKey, const char* value,
                                     char* u8Buf, int bufSize) const
{
    WCHAR subKeyW[512];
    if (subKey == NULL || SalU8ToW(subKey, -1, subKeyW, _countof(subKeyW)) == 0)
        return FALSE;
    HKEY key;
    if (RegOpenKeyExW(root, subKeyW, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return FALSE;
    BOOL ok = ReadStringValueW(key, value, u8Buf, bufSize);
    RegCloseKey(key);
    return ok;
}

BOOL CSalShellOsProbe::RegSubKeyString(HKEY root, const char* subKey, int index, const char* value,
                                       char* u8Buf, int bufSize) const
{
    if (u8Buf == NULL || bufSize <= 0 || index < 0)
        return FALSE;
    u8Buf[0] = 0;
    WCHAR subKeyW[512];
    if (subKey == NULL || SalU8ToW(subKey, -1, subKeyW, _countof(subKeyW)) == 0)
        return FALSE;
    HKEY key;
    if (RegOpenKeyExW(root, subKeyW, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return FALSE;
    WCHAR nameW[256];
    DWORD nameLen = _countof(nameW);
    BOOL exists = RegEnumKeyExW(key, (DWORD)index, nameW, &nameLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS;
    if (exists)
    {
        HKEY sub;
        if (RegOpenKeyExW(key, nameW, 0, KEY_QUERY_VALUE, &sub) == ERROR_SUCCESS)
        {
            ReadStringValueW(sub, value, u8Buf, bufSize); // "" when the value is missing
            RegCloseKey(sub);
        }
    }
    RegCloseKey(key);
    return exists;
}

BOOL CSalShellOsProbe::GetPackagePath(const char* family, char* u8Buf, int bufSize) const
{
    if (u8Buf == NULL || bufSize <= 0)
        return FALSE;
    u8Buf[0] = 0;
    WCHAR familyW[256];
    if (family == NULL || SalU8ToW(family, -1, familyW, _countof(familyW)) == 0)
        return FALSE;
    UINT32 count = 0;
    UINT32 bufLen = 0;
    if (GetPackagesByPackageFamily(familyW, &count, NULL, &bufLen, NULL) != ERROR_INSUFFICIENT_BUFFER ||
        count == 0 || bufLen == 0)
    {
        return FALSE;
    }
    PWSTR* names = (PWSTR*)malloc(count * sizeof(PWSTR));
    WCHAR* buffer = (WCHAR*)malloc(bufLen * sizeof(WCHAR));
    BOOL ok = FALSE;
    if (names != NULL && buffer != NULL &&
        GetPackagesByPackageFamily(familyW, &count, names, &bufLen, buffer) == ERROR_SUCCESS && count > 0)
    {
        UINT32 pathLen = 0;
        if (GetPackagePathByFullName(names[0], &pathLen, NULL) == ERROR_INSUFFICIENT_BUFFER && pathLen > 0)
        {
            WCHAR* pathW = (WCHAR*)malloc(pathLen * sizeof(WCHAR));
            if (pathW != NULL && GetPackagePathByFullName(names[0], &pathLen, pathW) == ERROR_SUCCESS)
                ok = SalWToU8(pathW, -1, u8Buf, bufSize) != 0 && u8Buf[0] != 0;
            free(pathW);
        }
    }
    free(buffer);
    free(names);
    if (!ok)
        u8Buf[0] = 0;
    return ok;
}

//*****************************************************************************
//
// preset table (contracts/shell-presets.md)
//

enum CSalShellCandKind
{
    sckEnvValue,    // %Key% is the program itself
    sckEnvDir,      // %Key% + '\' + Relative
    sckRegString,   // Root\Key value 'Value' (+ '\' + Relative when set)
    sckRegEnum,     // like sckRegString for each subkey of Root\Key, first existing wins
    sckPackagePath, // MSIX package family 'Key' install folder + '\' + Relative
};

struct CSalShellCandidate
{
    CSalShellCandKind Kind;
    HKEY Root;            // registry kinds only
    const char* Key;      // environment variable / registry subkey / package family
    const char* Value;    // registry value name (NULL = the default value)
    const char* Relative; // program relative to the located folder (NULL = the value is the program)
};

struct CSalShellPresetDef
{
    const char* Key;
    const char* Arguments;
    const CSalShellCandidate* Candidates;
    int Count;
};

static const CSalShellCandidate CmdCandidates[] = {
    {sckEnvValue, NULL, "COMSPEC", NULL, NULL}, // exactly what 0.1.5 launched
    {sckEnvDir, NULL, "SystemRoot", NULL, "System32\\cmd.exe"},
};

static const CSalShellCandidate PowerShellCandidates[] = {
    {sckEnvDir, NULL, "SystemRoot", NULL, "System32\\WindowsPowerShell\\v1.0\\powershell.exe"},
};

// alias/MSIX before the MSI registry key: winget installs the MSIX package
// from 7.6 and no MSI is produced from 7.7 (order of Windows Terminal's own
// profile generator)
static const CSalShellCandidate PwshCandidates[] = {
    {sckEnvDir, NULL, "ProgramFiles", NULL, "PowerShell\\7\\pwsh.exe"},
    {sckEnvDir, NULL, "LOCALAPPDATA", NULL, "Microsoft\\WindowsApps\\pwsh.exe"},
    {sckEnvDir, NULL, "LOCALAPPDATA", NULL, "Microsoft\\WindowsApps\\Microsoft.PowerShell_8wekyb3d8bbwe\\pwsh.exe"},
    {sckRegEnum, HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\PowerShellCore\\InstalledVersions", "InstallLocation", "pwsh.exe"},
    {sckRegString, HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\pwsh.exe", NULL, NULL},
};

static const CSalShellCandidate WtCandidates[] = {
    {sckEnvDir, NULL, "LOCALAPPDATA", NULL, "Microsoft\\WindowsApps\\wt.exe"},
    {sckEnvDir, NULL, "LOCALAPPDATA", NULL, "Microsoft\\WindowsApps\\Microsoft.WindowsTerminal_8wekyb3d8bbwe\\wt.exe"},
    {sckPackagePath, NULL, "Microsoft.WindowsTerminal_8wekyb3d8bbwe", NULL, "wt.exe"},
};

static const CSalShellCandidate GitBashCandidates[] = {
    {sckRegString, HKEY_CURRENT_USER, "Software\\GitForWindows", "InstallPath", "git-bash.exe"},
    {sckRegString, HKEY_LOCAL_MACHINE, "SOFTWARE\\GitForWindows", "InstallPath", "git-bash.exe"},
    {sckRegString, HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\GitForWindows", "InstallPath", "git-bash.exe"},
    {sckRegString, HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Git_is1", "InstallLocation", "git-bash.exe"},
    {sckEnvDir, NULL, "ProgramFiles", NULL, "Git\\git-bash.exe"},
    {sckEnvDir, NULL, "LOCALAPPDATA", NULL, "Programs\\Git\\git-bash.exe"},
};

static const CSalShellPresetDef Presets[sspCount] = {
    {"cmd", "", CmdCandidates, _countof(CmdCandidates)},
    {"powershell", "", PowerShellCandidates, _countof(PowerShellCandidates)},
    {"pwsh", "", PwshCandidates, _countof(PwshCandidates)},
    {"wt", "-d .", WtCandidates, _countof(WtCandidates)}, // relative to wt.exe's working directory
    {"git-bash", "", GitBashCandidates, _countof(GitBashCandidates)},
    {"custom", "", NULL, 0},
};

//*****************************************************************************
//
// SalShellLocatePreset / SalShellPresetArguments / SalShellPresetKey
//

BOOL SalShellLocatePreset(int preset, const CSalShellProbe* probe, char* u8Path, int pathSize)
{
    if (u8Path == NULL || pathSize <= 0)
        return FALSE;
    u8Path[0] = 0;
    if (preset < 0 || preset >= sspCustom)
        return FALSE;
    static const CSalShellOsProbe osProbe;
    if (probe == NULL)
        probe = &osProbe;

    const CSalShellPresetDef& def = Presets[preset];
    char* base = (char*)malloc(SAL_MAX_PATH_UTF8);
    char* cand = (char*)malloc(SAL_MAX_PATH_UTF8);
    BOOL found = FALSE;
    if (base != NULL && cand != NULL)
    {
        for (int i = 0; !found && i < def.Count; i++)
        {
            const CSalShellCandidate& c = def.Candidates[i];
            base[0] = 0;
            cand[0] = 0;
            switch (c.Kind)
            {
            case sckEnvValue:
                if (probe->GetEnv(c.Key, base, SAL_MAX_PATH_UTF8) && CopyStr(cand, SAL_MAX_PATH_UTF8, base))
                    found = probe->FileExists(cand);
                break;

            case sckEnvDir:
                if (probe->GetEnv(c.Key, base, SAL_MAX_PATH_UTF8) && JoinPath(base, c.Relative, cand, SAL_MAX_PATH_UTF8))
                    found = probe->FileExists(cand);
                break;

            case sckRegString:
                if (probe->RegReadString(c.Root, c.Key, c.Value, base, SAL_MAX_PATH_UTF8) &&
                    JoinPath(base, c.Relative, cand, SAL_MAX_PATH_UTF8))
                {
                    found = probe->FileExists(cand);
                }
                break;

            case sckRegEnum:
            {
                for (int j = 0; !found && j < 64 && probe->RegSubKeyString(c.Root, c.Key, j, c.Value, base, SAL_MAX_PATH_UTF8); j++)
                {
                    if (base[0] != 0 && JoinPath(base, c.Relative, cand, SAL_MAX_PATH_UTF8))
                        found = probe->FileExists(cand);
                }
                break;
            }

            case sckPackagePath:
                if (probe->GetPackagePath(c.Key, base, SAL_MAX_PATH_UTF8) && JoinPath(base, c.Relative, cand, SAL_MAX_PATH_UTF8))
                    found = probe->FileExists(cand);
                break;
            }
        }
        if (found && !CopyStr(u8Path, pathSize, cand))
            found = FALSE;
    }
    free(base);
    free(cand);
    if (!found)
        u8Path[0] = 0;
    return found;
}

const char* SalShellPresetArguments(int preset)
{
    if (preset < 0 || preset >= sspCount)
        return "";
    return Presets[preset].Arguments;
}

const char* SalShellPresetKey(int preset)
{
    if (preset < 0 || preset >= sspCount)
        return "";
    return Presets[preset].Key;
}
