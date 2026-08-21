// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// mdview.cpp - plugin entry point, CPluginInterface, configuration, About.

#include "precomp.h"
#include "render.h"
#include "htmlgen.h"
#include "webview.h" // feature 065: MdKeeperDisarm (COM-free header)

// plugin interface objects
CPluginInterface PluginInterface;
CPluginInterfaceForViewer InterfaceForViewer;

const char* PluginNameEN = "MDView";

HINSTANCE DLLInstance = NULL;
HINSTANCE HLanguage = NULL;

CSalamanderGeneralAbstract* SalamanderGeneral = NULL;
CSalamanderDebugAbstract* SalamanderDebug = NULL;
int SalamanderVersion = 0;
CSalamanderGUIAbstract* SalamanderGUI = NULL;

// --- configuration ---
char g_scheme[32] = "paper";
int g_followSys = 0;
char g_schemeLight[32] = "paper";
char g_schemeDark[32] = "graphite";
int g_zoom = 100;
BOOL g_savePos = TRUE;
WINDOWPLACEMENT g_wndPlacement = {0};
BOOL g_keepReady = TRUE; // default on (spec 065 FR-008)

#define CURRENT_CONFIG_VERSION 1
static const char* CONFIG_VERSION = "Version";
static const char* CONFIG_SCHEME = "ColorScheme";
static const char* CONFIG_FOLLOWSYS = "FollowSystemTheme";
static const char* CONFIG_SCHEMELIGHT = "SchemeLight";
static const char* CONFIG_SCHEMEDARK = "SchemeDark";
static const char* CONFIG_ZOOM = "ZoomPercent";
static const char* CONFIG_SAVEPOS = "SavePosition";
static const char* CONFIG_WNDPLACEMENT = "WindowPlacement";
static const char* CONFIG_KEEPREADY = "KeepReady";

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
                                   LoadStr(IDS_PLUGIN_DESCRIPTION), "MDVIEW", NULL, NULL);

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
            MdKeeperDisarm(); // feature 065: release the session keeper before unload
            ReleaseViewer();
        }
    }
    return ret;
}

static void ClampScheme(char* s)
{
    if (MdThemeById(s) == NULL)
        lstrcpynA(s, "paper", 32);
}

void WINAPI CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    if (regKey != NULL)
    {
        DWORD ver = CURRENT_CONFIG_VERSION;
        registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &ver, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_SCHEME, REG_SZ, g_scheme, sizeof(g_scheme));
        registry->GetValue(regKey, CONFIG_FOLLOWSYS, REG_DWORD, &g_followSys, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_SCHEMELIGHT, REG_SZ, g_schemeLight, sizeof(g_schemeLight));
        registry->GetValue(regKey, CONFIG_SCHEMEDARK, REG_SZ, g_schemeDark, sizeof(g_schemeDark));
        registry->GetValue(regKey, CONFIG_ZOOM, REG_DWORD, &g_zoom, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_SAVEPOS, REG_DWORD, &g_savePos, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_WNDPLACEMENT, REG_BINARY, &g_wndPlacement, sizeof(WINDOWPLACEMENT));
        registry->GetValue(regKey, CONFIG_KEEPREADY, REG_DWORD, &g_keepReady, sizeof(DWORD));
    }
    // corruption tolerance (FR-063): clamp to valid values
    ClampScheme(g_scheme);
    if (MdThemeById(g_schemeLight) == NULL || MdThemeById(g_schemeLight)->dark)
        lstrcpynA(g_schemeLight, "paper", 32);
    if (MdThemeById(g_schemeDark) == NULL || !MdThemeById(g_schemeDark)->dark)
        lstrcpynA(g_schemeDark, "graphite", 32);
    if (g_zoom < 50 || g_zoom > 300)
        g_zoom = 100;
    g_keepReady = g_keepReady ? TRUE : FALSE; // missing/corrupt -> stays default (on)
}

void WINAPI CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    DWORD v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_SCHEME, REG_SZ, g_scheme, -1);
    registry->SetValue(regKey, CONFIG_FOLLOWSYS, REG_DWORD, &g_followSys, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_SCHEMELIGHT, REG_SZ, g_schemeLight, -1);
    registry->SetValue(regKey, CONFIG_SCHEMEDARK, REG_SZ, g_schemeDark, -1);
    registry->SetValue(regKey, CONFIG_ZOOM, REG_DWORD, &g_zoom, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_SAVEPOS, REG_DWORD, &g_savePos, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_WNDPLACEMENT, REG_BINARY, &g_wndPlacement, sizeof(WINDOWPLACEMENT));
    registry->SetValue(regKey, CONFIG_KEEPREADY, REG_DWORD, &g_keepReady, sizeof(DWORD));
}

static INT_PTR CALLBACK CfgDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // feature 049: raw dialog proc - the two-touchpoint theme pattern
    // (WM_INITDIALOG apply + WM_CTLCOLOR* fallback; the FindDlgProc precedent)
    if (msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC)
    {
        INT_PTR brush;
        if (SalamanderGeneral->ThemeHandleCtlColor(msg, wParam, lParam, &brush))
            return brush;
    }

    switch (msg)
    {
    case WM_INITDIALOG:
        CheckDlgButton(hDlg, IDC_CFG_KEEPREADY, g_keepReady ? BST_CHECKED : BST_UNCHECKED);
        SalamanderGeneral->ThemeApplyToDialog(hDlg);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            BOOL keep = IsDlgButtonChecked(hDlg, IDC_CFG_KEEPREADY) == BST_CHECKED;
            if (g_keepReady && !keep)
                MdKeeperDisarm(); // turning off releases the kept-ready engine now (FR-008)
            g_keepReady = keep;   // turning on applies from the next view
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void WINAPI CPluginInterface::Configuration(HWND parent)
{
    // feature 065: one option - keep the rendering engine ready (FR-008)
    DialogBoxW(HLanguage, MAKEINTRESOURCEW(IDD_CFG), parent, CfgDlgProc);
}

void WINAPI CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");
    // register the Markdown viewer masks (respect later user overrides)
    salamander->AddViewer("*.md;*.markdown", FALSE);

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
