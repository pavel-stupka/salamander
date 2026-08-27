// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// codeview - syntax-highlighting viewer for source and configuration files
// (feature 070). Built on the shared WebView2 host (src/common/webhost).

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

// --- configuration (persisted to the plugin registry key; data-model.md S6) ---
extern char g_scheme[32];      // active colour scheme id (stable ASCII)
extern int g_followApp;        // follow the application light/dark theme
extern char g_schemeLight[32]; // per-polarity slot for follow mode
extern char g_schemeDark[32];
extern char g_fontFamily[64]; // monospace family (default "Cascadia Mono")
extern int g_fontSize;        // px, 0 = page default
extern int g_tabWidth;        // 1..16, default 4
extern int g_highlightLimitKB; // full-highlight size gate, default 1024
extern int g_viewerLimitMB;    // decline-above gate, default 20
extern int g_maxLineLength;    // per-line tokenisation gate, default 20000
extern BOOL g_lineNumbers;     // gutter on/off
extern BOOL g_wrap;            // word wrap on/off
extern BOOL g_whitespace;      // render whitespace marks
extern int g_zoom;             // zoom percent (50..300)
extern BOOL g_savePos;         // persist window placement
extern WINDOWPLACEMENT g_wndPlacement;
extern BOOL g_keepReady;  // keep the WebView2 engine warm for the session
extern BOOL g_hintShown;  // first-open hint already displayed (FR-012)
extern BOOL g_restoreTypes; // pending "restore default file types" (applied by Connect)

// [0,0] broadcast to open viewer windows: configuration changed
#define WM_USER_VIEWERCFGCHNG (WM_APP + 3410)

// --- viewer menu command ids (our own top-level window) ---
#define CM_FILE_CLOSE 102
#define CM_EDIT_COPY 103
#define CM_EDIT_SELALL 104
#define CM_EDIT_FIND 105
#define CM_EDIT_FINDNEXT 106
#define CM_EDIT_FINDPREV 107
#define CM_EDIT_GOTO 108
#define CM_VIEW_FOLLOWAPP 109
#define CM_VIEW_ZOOMIN 110
#define CM_VIEW_ZOOMOUT 111
#define CM_VIEW_ZOOMRESET 112
#define CM_VIEW_WRAP 113
#define CM_VIEW_LINENUMS 114
#define CM_VIEW_WHITESPACE 115
#define CM_HELP_ABOUT 116
#define CM_NEXTFILE 117
#define CM_PREVFILE 118
#define CM_OPEN_BUILTIN 119
#define CM_SCHEME_FIRST 200 // 200..219 (one per scheme)
#define CM_SCHEME_NEXT 230
#define CM_SCHEME_PREV 231
// (CM_LANG_* removed 2026-08-27 with the View > Language menu, FR-007 amendment)
#define CM_ENCODING_FIRST 4000 // 4000.. (one per conversion table)
#define CM_ENCODING_NEXT 3990
#define CM_ENCODING_PREV 3991

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

// A menu extension with NO menu items. It exists only so the viewer thread can
// get one action executed on the MAIN thread: ViewFileInPluginViewer is
// documented main-thread-only ("omezeni: hlavni thread", spl_gen.h:1912), and
// PostMenuExtCommand is the one documented cross-thread route to it. The core
// pulls this interface at load regardless of the FUNCTION_* flags
// (src/plugins1.cpp:2289), so no menu ever shows.
#define CV_MENUCMD_OPEN_BUILTIN 1

// Hands 'nameUtf8' to the built-in viewer from any thread. Returns FALSE when
// the request could not even be queued.
BOOL CvRequestBuiltinViewer(const char* nameUtf8);

class CPluginInterfaceForMenuExt : public CPluginInterfaceForMenuExtAbstract
{
public:
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask) { return 0; }
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                        int id, DWORD eventMask);
    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id) { return FALSE; }
    virtual void WINAPI BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander) {}
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
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt();
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
