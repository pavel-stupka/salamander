// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later

// feature 071-configurable-command-shell: the Command Shell command
// (CM_DOSSHELL - Num /, Ctrl+/, Commands > Command Shell, the toolbar button)
// opens the program chosen on the Configuration > Command Shell page: a preset
// located by src/common/salshell.cpp, or the user's Custom program + arguments
// (placeholders expanded by execute.cpp). The launch itself - the working
// directory rule, STARTUPINFO, the Group Policy check - is the pre-071
// handler's, moved here from mainwnd3.cpp; with the default preset it starts
// %COMSPEC% exactly as before.

#include "precomp.h"

#include "stswnd.h"
#include "editwnd.h"
#include "usermenu.h"
#include "execute.h"
#include "plugins.h"
#include "fileswnd.h"
#include "toolbar.h"
#include "mainwnd.h"
#include "cfgdlg.h"
#include "dialogs.h"

int GetCommandShellPresetNameResID(int preset)
{
    switch (preset)
    {
    case sspCommandPrompt:
        return IDS_CMDSHELL_PRESET_CMD;
    case sspWindowsPowerShell:
        return IDS_CMDSHELL_PRESET_POWERSHELL;
    case sspPowerShell7:
        return IDS_CMDSHELL_PRESET_PWSH;
    case sspWindowsTerminal:
        return IDS_CMDSHELL_PRESET_WT;
    case sspGitBash:
        return IDS_CMDSHELL_PRESET_GITBASH;
    default:
        return IDS_CMDSHELL_PRESET_CUSTOM;
    }
}

// E1/E2: the message names the program and carries a Help button that opens
// the Command Shell configuration topic (IDD_CFGPAGE_CMDSHELL is its help alias)
static void ShowCommandShellError(HWND parent, const char* u8Text)
{
    MSGBOXEX_PARAMS params;
    memset(&params, 0, sizeof(params));
    params.HParent = parent;
    params.Flags = MSGBOXEX_OK | MSGBOXEX_HELP | MSGBOXEX_ICONEXCLAMATION;
    params.Caption = LoadStrU8(IDS_CMDSHELL_ERRTITLE);
    params.Text = u8Text;
    params.ContextHelpId = IDD_CFGPAGE_CMDSHELL;
    params.HelpCallback = MessageBoxHelpCallback;
    SalMessageBoxEx(&params);
}

// UTF-16 length of a UTF-8 string without the terminator (0 when invalid)
static int WideLengthU8(const char* u8)
{
    int n = SalU8ToW(u8, -1, NULL, 0); // required size including the terminator
    return n > 0 ? n - 1 : 0;
}

void CMainWindow::OpenCommandShell(CFilesWindow* activePanel)
{
    CALL_STACK_MESSAGE1("CMainWindow::OpenCommandShell()");
    if (activePanel == NULL)
        return;
    activePanel->UserWorkedOnThisPath = TRUE;

    // working directory: the pre-071 rule - a disk directory, for an archive
    // panel the folder containing the archive (that is what GetPath() holds),
    // none for a plugin file system
    const char* curDir = (activePanel->Is(ptDisk) || activePanel->Is(ptZIPArchive)) ? activePanel->GetPath() : NULL;
    if (curDir != NULL && curDir[0] == 0)
        curDir = NULL;

    int preset = Configuration.CommandShellPreset;
    if (preset < 0 || preset >= sspCount)
        preset = sspCommandPrompt;

    char* program = (char*)malloc(SAL_MAX_PATH_UTF8);
    char* args = (char*)malloc(SAL_SHELL_ARGS_MAX);
    char* cmdLine = NULL;
    if (program == NULL || args == NULL)
    {
        TRACE_E(LOW_MEMORY);
        free(program);
        free(args);
        return;
    }
    program[0] = 0;
    args[0] = 0;

    BOOL ok = TRUE;
    if (preset == sspCustom)
    {
        // the expansion engine reports syntax and environment errors itself
        // (the User Menu behaviour); a FALSE means it did and we stop quietly
        ok = ExpandCommand(HWindow, Configuration.CommandShellProgram, program, SAL_MAX_PATH_UTF8, FALSE) &&
             ExpandCommandShellArguments(HWindow, curDir, Configuration.CommandShellArguments,
                                         args, SAL_SHELL_ARGS_MAX, FALSE);
        if (ok)
        {
            // the page refuses a blank program, but a hand-edited registry value may be one
            char* s = program;
            while (*s == ' ')
                s++;
            if (s != program)
                memmove(program, s, strlen(s) + 1);
            size_t len = strlen(program);
            while (len > 0 && program[len - 1] == ' ')
                program[--len] = 0;
            if (program[0] == 0)
            {
                char text[1000];
                _snprintf_s(text, _TRUNCATE, LoadStrU8(IDS_CMDSHELL_ERRNOTFOUND),
                            LoadStrU8(IDS_CMDSHELL_PRESET_CUSTOM));
                ShowCommandShellError(HWindow, text);
                ok = FALSE;
            }
        }
    }
    else
    {
        if (!SalShellLocatePreset(preset, NULL, program, SAL_MAX_PATH_UTF8))
        {
            // E1: the preset is not installed (any more)
            char text[1000];
            _snprintf_s(text, _TRUNCATE, LoadStrU8(IDS_CMDSHELL_ERRNOTFOUND),
                        LoadStrU8(GetCommandShellPresetNameResID(preset)));
            ShowCommandShellError(HWindow, text);
            ok = FALSE;
        }
        else
            lstrcpyn(args, SalShellPresetArguments(preset), SAL_SHELL_ARGS_MAX);
    }

    if (ok)
    {
        // Group Policy: the check the pre-071 handler ran on %COMSPEC%, now on
        // the resolved program (GetMyCanRun compares the leaf name)
        if (SystemPolicies.GetNoRun() ||
            (SystemPolicies.GetMyRunRestricted() && !SystemPolicies.GetMyCanRun(program)))
        {
            MSGBOXEX_PARAMS params;
            memset(&params, 0, sizeof(params));
            params.HParent = HWindow;
            params.Flags = MSGBOXEX_OK | MSGBOXEX_HELP | MSGBOXEX_ICONEXCLAMATION;
            params.Caption = LoadStr(IDS_POLICIESRESTRICTION_TITLE);
            params.Text = LoadStr(IDS_POLICIESRESTRICTION);
            params.ContextHelpId = IDH_GROUPPOLICY;
            params.HelpCallback = MessageBoxHelpCallback;
            SalMessageBoxEx(&params);
            ok = FALSE;
        }
    }

    if (ok)
    {
        // "program" args - CreateProcess wants a name with spaces quoted
        size_t cmdSize = strlen(program) + 2 + 1 + strlen(args) + 1;
        cmdLine = (char*)malloc(cmdSize);
        if (cmdLine == NULL)
        {
            TRACE_E(LOW_MEMORY);
            ok = FALSE;
        }
        else
        {
            strcpy(cmdLine, program);
            AddDoubleQuotesIfNeeded(cmdLine, (int)cmdSize);
            if (args[0] != 0)
            {
                strcat(cmdLine, " ");
                strcat(cmdLine, args);
            }
        }
    }

    if (ok)
    {
        SetDefaultDirectories();

        STARTUPINFO si;
        memset(&si, 0, sizeof(STARTUPINFO));
        si.cb = sizeof(STARTUPINFO);
        si.lpTitle = LoadStr(IDS_COMMANDSHELL); // parity with the pre-071 handler (SalCreateProcess does not forward it)
        si.dwFlags = STARTF_USESHOWWINDOW;
        POINT p;
        if (MultiMonGetDefaultWindowPos(HWindow, &p))
        {
            // the main window sits on another monitor: open the new window there
            // too, at that monitor's default position
            si.dwFlags |= STARTF_USEPOSITION;
            si.dwX = p.x;
            si.dwY = p.y;
        }
        si.wShowWindow = SW_SHOWNORMAL;

        PROCESS_INFORMATION pi;
        BOOL started = SalCreateProcess(NULL, cmdLine, NULL, NULL, FALSE,
                                        CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS, NULL,
                                        curDir, &si, &pi);
        DWORD err = started ? NO_ERROR : GetLastError();
        if (!started && curDir != NULL && WideLengthU8(curDir) >= MAX_PATH - 1)
        {
            // Windows refuses a starting directory of MAX_PATH-1 characters or
            // more for every program, long-path awareness or not (research
            // R19): retry once with the directory's 8.3 form
            char* shortDir = (char*)malloc(SAL_MAX_PATH_UTF8);
            if (shortDir != NULL && SalGetShortPathName(curDir, shortDir, SAL_MAX_PATH_UTF8) &&
                WideLengthU8(shortDir) < MAX_PATH - 1)
            {
                started = SalCreateProcess(NULL, cmdLine, NULL, NULL, FALSE,
                                           CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS, NULL,
                                           shortDir, &si, &pi);
                if (!started)
                    err = GetLastError();
            }
            free(shortDir);
        }

        if (!started)
        {
            // E2: name the program, quote the system's reason, point to the setting
            char shown[2 * MAX_PATH + 4];
            lstrcpyn(shown, program, 2 * MAX_PATH);
            if (strlen(program) >= 2 * MAX_PATH)
            {
                SalU8TrimIncompleteTail(shown); // never cut a UTF-8 sequence in half
                strcat(shown, "...");
            }
            char* text = (char*)malloc(4 * MAX_PATH);
            if (text != NULL)
            {
                _snprintf_s(text, 4 * MAX_PATH, _TRUNCATE, LoadStrU8(IDS_CMDSHELL_ERREXEC), shown, GetErrorText(err));
                ShowCommandShellError(HWindow, text);
                free(text);
            }
        }
        else
        {
            HANDLES(CloseHandle(pi.hProcess));
            HANDLES(CloseHandle(pi.hThread));
        }
    }

    free(cmdLine);
    free(args);
    free(program);
}
