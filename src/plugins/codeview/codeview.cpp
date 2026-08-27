// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// codeview.cpp - plugin entry point, CPluginInterface, viewer registration.

#include "precomp.h"
#include "langmap.h"
#include "schemes.h"
#include "webkeeper.h"

// plugin interface objects
CPluginInterface PluginInterface;
CPluginInterfaceForViewer InterfaceForViewer;

const char* PluginNameEN = "CodeView";

HINSTANCE DLLInstance = NULL;
HINSTANCE HLanguage = NULL;

CSalamanderGeneralAbstract* SalamanderGeneral = NULL;
CSalamanderDebugAbstract* SalamanderDebug = NULL;
int SalamanderVersion = 0;
CSalamanderGUIAbstract* SalamanderGUI = NULL;

// The plugin's own session keeper (contracts/webview-host-sharing.md S3.3):
// its own hidden controller, its own window class, armed at this plugin's
// first actual view and never earlier.
CTcWebKeeper CvKeeper;

const wchar_t* CV_KEEPER_CLASS = L"TandemCvKeeperWnd";

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DLLInstance = hinstDLL;
        INITCOMMONCONTROLSEX ic;
        ic.dwSize = sizeof(ic);
        ic.dwICC = ICC_BAR_CLASSES; // no ICC_STANDARD_CLASSES (constitution VI)
        InitCommonControlsEx(&ic);
    }
    return TRUE;
}

char* LoadStr(int resID)
{
    return SalamanderGeneral->LoadStr(HLanguage, resID);
}

void OnAbout(HWND hParent)
{
    char buf[1000];
    _snprintf_s(buf, _TRUNCATE,
                "%s " VERSINFO_VERSION "\n\n" VERSINFO_COPYRIGHT "\n\n%s",
                LoadStr(IDS_PLUGINNAME), LoadStr(IDS_PLUGIN_DESCRIPTION));
    SalamanderGeneral->SalMessageBox(hParent, buf, LoadStr(IDS_ABOUT), MB_OK | MB_ICONINFORMATION);
}

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    SalamanderDebug = salamander->GetSalamanderDebug();
    SalamanderVersion = salamander->GetVersion();
    HANDLES_CAN_USE_TRACE();
    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    {
        MessageBox(salamander->GetParentWindow(), REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   PluginNameEN, MB_OK | MB_ICONERROR);
        return NULL;
    }

    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), PluginNameEN);
    if (HLanguage == NULL)
        return NULL;

    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderGUI = salamander->GetSalamanderGUI();

    if (!InitViewer())
        return NULL;

    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION | FUNCTION_VIEWER,
                                   VERSINFO_VERSION_NO_PLATFORM, VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION), "CODEVIEW", NULL, NULL);

    salamander->SetPluginHomePageURL(LoadStr(IDS_PLUGIN_HOME));

    return &PluginInterface;
}

//
// CPluginInterface
//

void WINAPI CPluginInterface::About(HWND parent) { OnAbout(parent); }

BOOL WINAPI CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);
    BOOL ret = ViewerWindowQueue.Empty();
    if (!ret && (force || SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_VIEWER_OPENWNDS),
                                                           LoadStr(IDS_PLUGINNAME),
                                                           MB_YESNO | MB_ICONQUESTION) == IDYES))
    {
        ret = ViewerWindowQueue.CloseAllWindows(force) || force;
    }
    if (ret)
    {
        if (!ThreadQueue.KillAll(force) && !force)
            ret = FALSE;
        else
        {
            CvKeeper.Disarm(); // release the session keeper before unload
            ReleaseViewer();
        }
    }
    return ret;
}

void WINAPI CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    // Register the claimed file types (contracts/claimed-types.md S3). Every
    // AddViewer call inserts ONE row at the top of the Viewers list, so the
    // rows are issued in reverse: the generated table is in display order and
    // the last call ends up highest. With force=FALSE this happens only on the
    // load that installs the plugin -- later edits by the user are permanent
    // (spec FR-011).
    for (int i = CvMaskRowCount - 1; i >= 0; i--)
        salamander->AddViewer(CvMaskRows[i], FALSE);

    // A "restore default file types" requested from the configuration dialog
    // is carried out here: this is the only place the core hands out the
    // connect interface (spl_base.h:638). force=TRUE adds a mask only when it
    // is not present already, so families the user kept are not duplicated.
    if (g_restoreTypes)
    {
        for (int i = CvMaskRowCount - 1; i >= 0; i--)
            salamander->AddViewer(CvMaskRows[i], TRUE);
        g_restoreTypes = FALSE;
    }

    HBITMAP hBmp = (HBITMAP)HANDLES(LoadImage(DLLInstance, MAKEINTRESOURCE(IDB_PLUGINICO),
                                              IMAGE_BITMAP, 16, 16, LR_DEFAULTCOLOR));
    if (hBmp != NULL)
    {
        salamander->SetBitmapWithIcons(hBmp);
        HANDLES(DeleteObject(hBmp));
        salamander->SetPluginIcon(0);
        salamander->SetPluginMenuAndToolbarIcon(0);
    }
}

void WINAPI CPluginInterface::Event(int event, DWORD param)
{
    if (event == PLUGINEVENT_SETTINGCHANGE)
        ViewerWindowQueue.BroadcastMessage(WM_USER_VIEWERCFGCHNG, 0, 0);
}

CPluginInterfaceForViewerAbstract* WINAPI CPluginInterface::GetInterfaceForViewer()
{
    return &InterfaceForViewer;
}
