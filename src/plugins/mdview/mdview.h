// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// --- global plugin data ---
extern const char* PluginNameEN;
extern HINSTANCE DLLInstance; // .SPL (language-independent) resources
extern HINSTANCE HLanguage;   // .SLG (language-dependent) resources

extern CSalamanderGeneralAbstract* SalamanderGeneral;
extern CSalamanderGUIAbstract* SalamanderGUI;

char* LoadStr(int resID);

BOOL InitViewer();
void ReleaseViewer();

// --- configuration (persisted to the plugin registry key) ---
extern char g_scheme[32];      // active color scheme id (stable ASCII)
extern int g_followSys;        // follow OS light/dark
extern char g_schemeLight[32]; // per-polarity slot for follow-system mode
extern char g_schemeDark[32];
extern int g_zoom;     // zoom percent (50..300)
extern BOOL g_savePos; // persist window placement
extern WINDOWPLACEMENT g_wndPlacement;
extern BOOL g_keepReady; // feature 065: keep the WebView2 engine warm
                         // for the session after the first view (FR-008)

// [0,0] broadcast to open viewer windows: configuration changed
#define WM_USER_VIEWERCFGCHNG (WM_APP + 3400)

// --- viewer menu command ids (our own top-level window) ---
#define CM_FILE_OPENTEXT 101
#define CM_FILE_CLOSE 102
#define CM_EDIT_COPY 103
#define CM_EDIT_SELALL 104
#define CM_EDIT_FIND 105
#define CM_EDIT_FINDNEXT 106
#define CM_EDIT_FINDPREV 107
#define CM_VIEW_FOLLOWSYS 108
#define CM_VIEW_ZOOMIN 109
#define CM_VIEW_ZOOMOUT 110
#define CM_VIEW_ZOOMRESET 111
#define CM_HELP_ABOUT 112
#define CM_NEXTFILE 113
#define CM_PREVFILE 114
#define CM_VIEW_REMOTEIMG 115
#define CM_SCHEME_FIRST 200 // 200..209 (one per theme)
#define CM_SCHEME_NEXT 210
#define CM_SCHEME_PREV 211

void OnAbout(HWND hParent);

//
// CPluginInterface
//

class CPluginInterfaceForViewer : public CPluginInterfaceForViewerAbstract
{
public:
    virtual BOOL WINAPI ViewFile(const char* name, int left, int top, int width, int height,
                                 UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                                 BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                                 int enumFilesSourceUID, int enumFilesCurrentIndex);
    virtual BOOL WINAPI CanViewFile(const char* name);
};

class CPluginInterface : public CPluginInterfaceAbstract
{
public:
    virtual void WINAPI About(HWND parent);
    virtual BOOL WINAPI Release(HWND parent, BOOL force);
    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI Configuration(HWND parent);
    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander);
    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) {}
    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() { return NULL; }
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer();
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt() { return NULL; }
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() { return NULL; }
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; }
    virtual void WINAPI Event(int event, DWORD param);
    virtual void WINAPI ClearHistory(HWND parent) {}
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}
    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

extern CWindowQueue ViewerWindowQueue;
extern CThreadQueue ThreadQueue;
extern CPluginInterface PluginInterface;
