// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <windows.h>
#include <aclapi.h>

#include "handles.h"
#include "salunicode.h"
#include "salpath.h"
#include "salfileio.h"

// frees the converted path while keeping the API call's last error
static void SalFreeKeepLastError(WCHAR* w)
{
    DWORD err = GetLastError();
    free(w);
    SetLastError(err);
}

//*****************************************************************************
//
// SalFindFirstFile / SalFindNextFile
//

HANDLE SalFindFirstFile(const char* u8pattern, WIN32_FIND_DATAW* data)
{
    WCHAR* w = SalPathToWExtAlloc(u8pattern);
    if (w == NULL)
    {
        SetLastError(ERROR_INVALID_NAME);
        return INVALID_HANDLE_VALUE;
    }
    HANDLE h = HANDLES_Q(FindFirstFileW(w, data));
    SalFreeKeepLastError(w);
    return h;
}

BOOL SalFindNextFile(HANDLE find, WIN32_FIND_DATAW* data)
{
    return FindNextFileW(find, data);
}

void SalConvertFindDataW(const WIN32_FIND_DATAW* w, WIN32_FIND_DATA* a,
                         char* nameU8, int nameU8Size, char* dosNameU8, int dosNameU8Size)
{
    if (a != NULL)
    {
        a->dwFileAttributes = w->dwFileAttributes;
        a->ftCreationTime = w->ftCreationTime;
        a->ftLastAccessTime = w->ftLastAccessTime;
        a->ftLastWriteTime = w->ftLastWriteTime;
        a->nFileSizeHigh = w->nFileSizeHigh;
        a->nFileSizeLow = w->nFileSizeLow;
        a->dwReserved0 = w->dwReserved0;
        a->dwReserved1 = w->dwReserved1;
        a->cFileName[0] = 0;          // names are NOT kept in the legacy view,
        a->cAlternateFileName[0] = 0; // they live in nameU8/dosNameU8
    }
    // SalWToU8 is total since feature 066 (unpaired surrogates travel as
    // WTF-8), so it fails only for a too-small buffer; the lenient WinAPI
    // call remains as a last-resort fail-safe only
    if (nameU8 != NULL &&
        SalWToU8(w->cFileName, -1, nameU8, nameU8Size) == 0 &&
        // encoding-check: allow lossy-lenient-at-intake - last-resort fail-safe
        // after the total SalWToU8; reachable only on a too-small buffer (066)
        WideCharToMultiByte(CP_UTF8, 0, w->cFileName, -1, nameU8, nameU8Size, NULL, NULL) == 0)
        nameU8[0] = 0;
    if (dosNameU8 != NULL &&
        SalWToU8(w->cAlternateFileName, -1, dosNameU8, dosNameU8Size) == 0 &&
        // encoding-check: allow lossy-lenient-at-intake - last-resort fail-safe
        // after the total SalWToU8; reachable only on a too-small buffer (066)
        WideCharToMultiByte(CP_UTF8, 0, w->cAlternateFileName, -1, dosNameU8, dosNameU8Size, NULL, NULL) == 0)
        dosNameU8[0] = 0;
}

//*****************************************************************************
//
// SalCreateFile
//

HANDLE SalCreateFile(const char* u8path, DWORD desiredAccess, DWORD shareMode,
                     LPSECURITY_ATTRIBUTES securityAttributes, DWORD creationDisposition,
                     DWORD flagsAndAttributes, HANDLE templateFile)
{
    WCHAR* w = SalPathToWExtAlloc(u8path);
    if (w == NULL)
    {
        SetLastError(ERROR_INVALID_NAME);
        return INVALID_HANDLE_VALUE;
    }
    HANDLE h = HANDLES_Q(CreateFileW(w, desiredAccess, shareMode, securityAttributes,
                                     creationDisposition, flagsAndAttributes, templateFile));
    SalFreeKeepLastError(w);
    return h;
}

HANDLE SalCreateFileNH(const char* u8path, DWORD desiredAccess, DWORD shareMode,
                       LPSECURITY_ATTRIBUTES securityAttributes, DWORD creationDisposition,
                       DWORD flagsAndAttributes, HANDLE templateFile)
{
    WCHAR* w = SalPathToWExtAlloc(u8path);
    if (w == NULL)
    {
        SetLastError(ERROR_INVALID_NAME);
        return INVALID_HANDLE_VALUE;
    }
    HANDLE h = NOHANDLES(CreateFileW(w, desiredAccess, shareMode, securityAttributes,
                                     creationDisposition, flagsAndAttributes, templateFile));
    SalFreeKeepLastError(w);
    return h;
}

//*****************************************************************************
//
// SalCreateProcess
//

BOOL SalCreateProcess(const char* u8AppName, const char* u8CmdLine,
                      LPSECURITY_ATTRIBUTES processAttrs, LPSECURITY_ATTRIBUTES threadAttrs,
                      BOOL inheritHandles, DWORD creationFlags, LPVOID environment,
                      const char* u8CurrentDir, STARTUPINFOA* siA, PROCESS_INFORMATION* pi)
{
    WCHAR* appW = u8AppName == NULL ? NULL : SalU8ToWAlloc(u8AppName, -1);
    WCHAR* cmdW = u8CmdLine == NULL ? NULL : SalU8ToWAlloc(u8CmdLine, -1); // CreateProcessW may modify it
    WCHAR* dirW = u8CurrentDir == NULL ? NULL : SalU8ToWAlloc(u8CurrentDir, -1);
    if ((u8AppName != NULL && appW == NULL) || (u8CmdLine != NULL && cmdW == NULL) ||
        (u8CurrentDir != NULL && dirW == NULL))
    {
        free(appW);
        free(cmdW);
        free(dirW);
        SetLastError(ERROR_INVALID_NAME);
        return FALSE;
    }

    STARTUPINFOW siW;
    memset(&siW, 0, sizeof(siW));
    siW.cb = sizeof(siW);
    if (siA != NULL)
    { // copy the non-string fields (lpDesktop/lpTitle are not used by our call sites)
        siW.dwX = siA->dwX;
        siW.dwY = siA->dwY;
        siW.dwXSize = siA->dwXSize;
        siW.dwYSize = siA->dwYSize;
        siW.dwXCountChars = siA->dwXCountChars;
        siW.dwYCountChars = siA->dwYCountChars;
        siW.dwFillAttribute = siA->dwFillAttribute;
        siW.dwFlags = siA->dwFlags;
        siW.wShowWindow = siA->wShowWindow;
        siW.hStdInput = siA->hStdInput;
        siW.hStdOutput = siA->hStdOutput;
        siW.hStdError = siA->hStdError;
    }

    BOOL ret = CreateProcessW(appW, cmdW, processAttrs, threadAttrs, inheritHandles,
                              creationFlags, environment, dirW, &siW, pi);
    DWORD err = GetLastError();
    free(appW);
    free(cmdW);
    free(dirW);
    if (ret && pi != NULL)
    {
        if (pi->hProcess != NULL)
            HANDLES_ADD(__htProcess, __hoCreateProcess, pi->hProcess);
        if (pi->hThread != NULL)
            HANDLES_ADD(__htThread, __hoCreateProcess, pi->hThread);
    }
    SetLastError(err);
    return ret;
}

//*****************************************************************************
//
// SalShellExecuteEx
//

BOOL SalShellExecuteEx(SHELLEXECUTEINFOA* seiA)
{
    if (seiA == NULL)
        return FALSE;

    WCHAR* verbW = seiA->lpVerb == NULL ? NULL : SalU8ToWAlloc(seiA->lpVerb, -1);
    WCHAR* fileW = seiA->lpFile == NULL ? NULL : SalU8ToWAlloc(seiA->lpFile, -1);
    WCHAR* paramsW = seiA->lpParameters == NULL ? NULL : SalU8ToWAlloc(seiA->lpParameters, -1);
    WCHAR* dirW = seiA->lpDirectory == NULL ? NULL : SalU8ToWAlloc(seiA->lpDirectory, -1);
    WCHAR* classW = (seiA->fMask & SEE_MASK_CLASSNAME) && seiA->lpClass != NULL ? SalU8ToWAlloc(seiA->lpClass, -1) : NULL;

    BOOL convOK = (seiA->lpVerb == NULL || verbW != NULL) &&
                  (seiA->lpFile == NULL || fileW != NULL) &&
                  (seiA->lpParameters == NULL || paramsW != NULL) &&
                  (seiA->lpDirectory == NULL || dirW != NULL);

    BOOL ret = FALSE;
    if (convOK)
    {
        SHELLEXECUTEINFOW seiW;
        memset(&seiW, 0, sizeof(seiW));
        seiW.cbSize = sizeof(seiW);
        seiW.fMask = seiA->fMask;
        seiW.hwnd = seiA->hwnd;
        seiW.lpVerb = verbW;
        seiW.lpFile = fileW;
        seiW.lpParameters = paramsW;
        seiW.lpDirectory = dirW;
        seiW.nShow = seiA->nShow;
        seiW.lpIDList = seiA->lpIDList;
        seiW.lpClass = classW;
        seiW.hkeyClass = seiA->hkeyClass;
        seiW.dwHotKey = seiA->dwHotKey;
        seiW.hIcon = seiA->hIcon;
        seiW.hProcess = seiA->hProcess;

        ret = ShellExecuteExW(&seiW);

        seiA->hInstApp = seiW.hInstApp; // copy the output fields back
        seiA->hProcess = seiW.hProcess;
        seiA->hIcon = seiW.hIcon;
    }
    else
        SetLastError(ERROR_INVALID_NAME);

    DWORD err = GetLastError();
    free(verbW);
    free(fileW);
    free(paramsW);
    free(dirW);
    free(classW);
    SetLastError(err);
    return ret;
}

//*****************************************************************************
//
// single-path operations
//

typedef BOOL(WINAPI* FSalPathOpW)(LPCWSTR);

static BOOL SalPathOp(const char* u8path, FSalPathOpW op)
{
    WCHAR* w = SalPathToWExtAlloc(u8path);
    if (w == NULL)
    {
        SetLastError(ERROR_INVALID_NAME);
        return FALSE;
    }
    BOOL ret = op(w);
    SalFreeKeepLastError(w);
    return ret;
}

BOOL SalDeleteFile(const char* u8path)
{
    return SalPathOp(u8path, DeleteFileW);
}

BOOL SalRemoveDirectory(const char* u8path)
{
    return SalPathOp(u8path, RemoveDirectoryW);
}

BOOL SalCreateDirectory(const char* u8path, LPSECURITY_ATTRIBUTES securityAttributes)
{
    WCHAR* w = SalPathToWExtAlloc(u8path);
    if (w == NULL)
    {
        SetLastError(ERROR_INVALID_NAME);
        return FALSE;
    }
    BOOL ret = CreateDirectoryW(w, securityAttributes);
    SalFreeKeepLastError(w);
    return ret;
}

//*****************************************************************************
//
// two-path operations
//

BOOL SalMoveFile(const char* u8from, const char* u8to)
{
    if (!SalMoveFileEx(u8from, u8to, 0))
    {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED)
        { // Novell issue: MoveFile fails for files with the read-only attribute
            DWORD attr = SalGetFileAttributes(u8from);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY))
            {
                SalSetFileAttributes(u8from, FILE_ATTRIBUTE_ARCHIVE);
                if (SalMoveFileEx(u8from, u8to, 0))
                {
                    SalSetFileAttributes(u8to, attr);
                    return TRUE;
                }
                err = GetLastError();
                SalSetFileAttributes(u8from, attr);
            }
            SetLastError(err);
        }
        return FALSE;
    }
    return TRUE;
}

BOOL SalMoveFileEx(const char* u8from, const char* u8to, DWORD flags)
{
    WCHAR* wf = SalPathToWExtAlloc(u8from);
    WCHAR* wt = SalPathToWExtAlloc(u8to);
    BOOL ret = FALSE;
    if (wf == NULL || wt == NULL)
        SetLastError(ERROR_INVALID_NAME);
    else
        ret = flags == 0 ? MoveFileW(wf, wt) : MoveFileExW(wf, wt, flags);
    DWORD err = GetLastError();
    free(wf);
    free(wt);
    SetLastError(err);
    return ret;
}

BOOL SalCopyFile(const char* u8from, const char* u8to, BOOL failIfExists)
{
    WCHAR* wf = SalPathToWExtAlloc(u8from);
    WCHAR* wt = SalPathToWExtAlloc(u8to);
    BOOL ret = FALSE;
    if (wf == NULL || wt == NULL)
        SetLastError(ERROR_INVALID_NAME);
    else
        ret = CopyFileW(wf, wt, failIfExists);
    DWORD err = GetLastError();
    free(wf);
    free(wt);
    SetLastError(err);
    return ret;
}

//*****************************************************************************
//
// SalGetShortPathName
//

BOOL SalGetShortPathName(const char* u8path, char* buf, int bufSize)
{
    if (buf == NULL || bufSize <= 0)
        return FALSE;
    buf[0] = 0;
    WCHAR* w = SalPathToWExtAlloc(u8path);
    if (w == NULL)
    {
        SetLastError(ERROR_INVALID_NAME);
        return FALSE;
    }
    WCHAR shortW[2 * MAX_PATH]; // 8.3 components are short; deep paths may still not fit - fail cleanly
    DWORD res = GetShortPathNameW(w, shortW, _countof(shortW));
    SalFreeKeepLastError(w);
    if (res == 0 || res >= _countof(shortW))
        return FALSE;
    char* u8 = SalPathFromWAlloc(shortW); // strips the \\?\ prefix
    if (u8 == NULL)
        return FALSE;
    BOOL ret = (int)strlen(u8) < bufSize;
    if (ret)
        strcpy(buf, u8);
    free(u8);
    return ret;
}

//*****************************************************************************
//
// attributes
//

DWORD SalGetFileAttributes(const char* u8path)
{
    WCHAR* w = SalPathToWExtAlloc(u8path);
    if (w == NULL)
    {
        SetLastError(ERROR_INVALID_NAME);
        return INVALID_FILE_ATTRIBUTES;
    }
    DWORD ret = GetFileAttributesW(w);
    SalFreeKeepLastError(w);
    return ret;
}

BOOL SalSetFileAttributes(const char* u8path, DWORD attributes)
{
    WCHAR* w = SalPathToWExtAlloc(u8path);
    if (w == NULL)
    {
        SetLastError(ERROR_INVALID_NAME);
        return FALSE;
    }
    BOOL ret = SetFileAttributesW(w, attributes);
    SalFreeKeepLastError(w);
    return ret;
}

BOOL SalGetFileAttributesEx(const char* u8path, WIN32_FILE_ATTRIBUTE_DATA* data)
{
    WCHAR* w = SalPathToWExtAlloc(u8path);
    if (w == NULL)
    {
        SetLastError(ERROR_INVALID_NAME);
        return FALSE;
    }
    BOOL ret = GetFileAttributesExW(w, GetFileExInfoStandard, data);
    SalFreeKeepLastError(w);
    return ret;
}

DWORD SalGetNamedSecurityInfo(const char* u8path, SECURITY_INFORMATION si,
                              PSID* owner, PSID* group, PACL* dacl, PACL* sacl,
                              PSECURITY_DESCRIPTOR* sd)
{
    WCHAR* w = SalPathToWExtAlloc(u8path);
    if (w == NULL)
        return ERROR_INVALID_NAME;
    DWORD ret = GetNamedSecurityInfoW(w, SE_FILE_OBJECT, si, owner, group, dacl, sacl, sd);
    free(w);
    return ret;
}

DWORD SalSetNamedSecurityInfo(const char* u8path, SECURITY_INFORMATION si,
                              PSID owner, PSID group, PACL dacl, PACL sacl)
{
    WCHAR* w = SalPathToWExtAlloc(u8path);
    if (w == NULL)
        return ERROR_INVALID_NAME;
    DWORD ret = SetNamedSecurityInfoW(w, SE_FILE_OBJECT, si, owner, group, dacl, sacl);
    free(w);
    return ret;
}

BOOL SalEncryptFile(const char* u8path)
{
    WCHAR* w = SalPathToWExtAlloc(u8path);
    if (w == NULL)
    {
        SetLastError(ERROR_INVALID_NAME);
        return FALSE;
    }
    BOOL ret = EncryptFileW(w);
    SalFreeKeepLastError(w);
    return ret;
}

BOOL SalDecryptFile(const char* u8path)
{
    WCHAR* w = SalPathToWExtAlloc(u8path);
    if (w == NULL)
    {
        SetLastError(ERROR_INVALID_NAME);
        return FALSE;
    }
    BOOL ret = DecryptFileW(w, 0);
    SalFreeKeepLastError(w);
    return ret;
}
