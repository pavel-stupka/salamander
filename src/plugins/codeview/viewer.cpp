// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// viewer.cpp - the codeview viewer window: thread/lock plumbing, the shared
// WebView2 host, menus, find, go-to-line, schemes, encodings, status bar and
// panel navigation.

#include "precomp.h"
#include "webhost.h"
#include "webkeeper.h"
#include "intake.h"
#include "langmap.h"
#include "schemes.h"
#include "webglue.h"
#include "viewer.h"
#include "darkmenu.h"

#include <algorithm>
#include <vector>

CWindowQueue ViewerWindowQueue("CodeView Viewers");
CThreadQueue ThreadQueue("CodeView Viewers");

extern CTcWebKeeper CvKeeper;
extern const wchar_t* CV_KEEPER_CLASS;

static HACCEL ViewerAccels = NULL;

#define CV_STATUS_HEIGHT 20

// ==========================================================================
// Init / release
// ==========================================================================

BOOL InitViewer()
{
    if (!InitializeWinLib(PluginNameEN, DLLInstance))
        return FALSE;
    SetWinLibStrings(LoadStr(IDS_INVALID_NUM), LoadStr(IDS_PLUGINNAME));
    SetupWinLibTheme(SalamanderGeneral); // feature 036: dark theme for WinLib dialogs

    // Keys the frame owns. Keys pressed while focus is inside the WebView are
    // routed by the host's accelerator callback (webglue.cpp) to the same
    // commands, so both paths agree (contracts/host-page-interface.md S5).
    ACCEL acc[] = {
        {FVIRTKEY | FCONTROL, 'F', CM_EDIT_FIND},
        {FVIRTKEY, VK_F3, CM_EDIT_FINDNEXT},
        {FVIRTKEY | FSHIFT, VK_F3, CM_EDIT_FINDPREV},
        {FVIRTKEY, VK_F6, CM_EDIT_FINDNEXT},
        {FVIRTKEY | FSHIFT, VK_F6, CM_EDIT_FINDPREV},
        {FVIRTKEY | FCONTROL, 'G', CM_EDIT_GOTO},
        {FVIRTKEY, VK_ESCAPE, CM_FILE_CLOSE},
        {FVIRTKEY, VK_F2, CM_VIEW_WRAP},
        {FVIRTKEY | FCONTROL, 'W', CM_VIEW_WRAP},
        {FVIRTKEY, VK_F8, CM_ENCODING_NEXT},
        {FVIRTKEY | FSHIFT, VK_F8, CM_ENCODING_PREV},
        {FVIRTKEY | FCONTROL, VK_OEM_PLUS, CM_VIEW_ZOOMIN},
        {FVIRTKEY | FCONTROL, VK_ADD, CM_VIEW_ZOOMIN},
        {FVIRTKEY | FCONTROL, VK_OEM_MINUS, CM_VIEW_ZOOMOUT},
        {FVIRTKEY | FCONTROL, VK_SUBTRACT, CM_VIEW_ZOOMOUT},
        {FVIRTKEY | FCONTROL, '0', CM_VIEW_ZOOMRESET},
        {FVIRTKEY | FCONTROL, VK_NUMPAD0, CM_VIEW_ZOOMRESET},
        {FVIRTKEY, VK_F9, CM_SCHEME_NEXT},
        {FVIRTKEY | FSHIFT, VK_F9, CM_SCHEME_PREV},
        {FVIRTKEY | FCONTROL, VK_NEXT, CM_NEXTFILE},
        {FVIRTKEY | FCONTROL, VK_PRIOR, CM_PREVFILE},
    };
    ViewerAccels = CreateAcceleratorTable(acc, (int)(sizeof(acc) / sizeof(acc[0])));
    return TRUE;
}

void ReleaseViewer()
{
    if (ViewerAccels != NULL)
    {
        DestroyAcceleratorTable(ViewerAccels);
        ViewerAccels = NULL;
    }
    DarkMenuReleaseFont();
    ReleaseWinLib(DLLInstance);
}

// ==========================================================================
// Viewer thread (the demoview/mdview model: one thread per window)
// ==========================================================================

class CViewerThread : public CThread
{
protected:
    char* Name;
    int Left, Top, Width, Height;
    UINT ShowCmd;
    BOOL AlwaysOnTop, ReturnLock;
    HANDLE Continue;
    HANDLE* Lock;
    BOOL* LockOwner;
    BOOL* Success;
    int EnumFilesSourceUID, EnumFilesCurrentIndex;

public:
    CViewerThread(const char* name, int left, int top, int width, int height, UINT showCmd,
                  BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock, BOOL* lockOwner, HANDLE contEvent,
                  BOOL* success, int enumFilesSourceUID, int enumFilesCurrentIndex)
        : CThread("CodeView Viewer")
    {
        Name = _strdup(name);
        Left = left;
        Top = top;
        Width = width;
        Height = height;
        ShowCmd = showCmd;
        AlwaysOnTop = alwaysOnTop;
        ReturnLock = returnLock;
        Continue = contEvent;
        Lock = lock;
        LockOwner = lockOwner;
        Success = success;
        EnumFilesSourceUID = enumFilesSourceUID;
        EnumFilesCurrentIndex = enumFilesCurrentIndex;
    }
    virtual ~CViewerThread() { free(Name); }
    virtual unsigned Body();
};

unsigned CViewerThread::Body()
{
    CALL_STACK_MESSAGE1("CViewerThread::Body()");
    CViewerWindow* window = new CViewerWindow(EnumFilesSourceUID, EnumFilesCurrentIndex);
    if (window != NULL)
    {
        if (ReturnLock)
        {
            *Lock = window->GetLock();
            *LockOwner = TRUE;
        }
        if (!ReturnLock || *Lock != NULL)
        {
            if (g_savePos && g_wndPlacement.length != 0)
            {
                WINDOWPLACEMENT place = g_wndPlacement;
                RECT monitorRect, workRect;
                SalamanderGeneral->MultiMonGetClipRectByRect(&place.rcNormalPosition, &workRect, &monitorRect);
                OffsetRect(&place.rcNormalPosition, workRect.left - monitorRect.left, workRect.top - monitorRect.top);
                SalamanderGeneral->MultiMonEnsureRectVisible(&place.rcNormalPosition, TRUE);
                Left = place.rcNormalPosition.left;
                Top = place.rcNormalPosition.top;
                Width = place.rcNormalPosition.right - place.rcNormalPosition.left;
                Height = place.rcNormalPosition.bottom - place.rcNormalPosition.top;
                ShowCmd = place.showCmd;
            }
            if (window->CreateEx(AlwaysOnTop ? WS_EX_TOPMOST : 0, CWINDOW_CLASSNAME2, "Code Viewer",
                                 WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, Left, Top, Width, Height,
                                 NULL, NULL, DLLInstance, window) != NULL)
            {
                SalamanderGeneral->ThemeApplyToTopLevel(window->HWindow); // feature 036: dark title bar
                ShowWindow(window->HWindow, ShowCmd);
                SetForegroundWindow(window->HWindow);
                UpdateWindow(window->HWindow);
                *Success = TRUE;
            }
            else if (ReturnLock && *Lock != NULL)
                HANDLES(CloseHandle(*Lock));
        }
    }

    BOOL openFile = *Success && Name != NULL;
    SetEvent(Continue);
    Continue = NULL;
    Lock = NULL;
    LockOwner = NULL;
    Success = NULL;

    if (openFile)
    {
        window->OpenFile(Name, FALSE);
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            if (!TranslateAccelerator(window->HWindow, ViewerAccels, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
    if (window != NULL)
        delete window;
    return 0;
}

// ==========================================================================
// CPluginInterfaceForViewer
// ==========================================================================

BOOL WINAPI CPluginInterfaceForViewer::CanViewFile(const char* name)
{
    // Cheap, dialog-free, first-8-KB decision. FALSE hands the file to the
    // next viewer in the user's list -- by default the built-in one, which
    // has hex mode and no size limit (spec FR-027).
    return CvCanView(name);
}

BOOL WINAPI CPluginInterfaceForViewer::ViewFile(const char* name, int left, int top, int width, int height,
                                                UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                                                BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                                                int enumFilesSourceUID, int enumFilesCurrentIndex)
{
    // Engine-unavailable fallback: ViewFile runs on the main thread, where the
    // internal text viewer may legally be opened (ViewFileInPluginViewer is
    // main-thread-only). Do this before spawning our own viewer thread.
    if (!CTcWebHost::RuntimeAvailable())
    {
        CSalamanderPluginInternalViewerData data;
        ZeroMemory(&data, sizeof(data));
        data.Size = sizeof(data);
        data.FileName = name;
        data.Mode = 0;
        data.Caption = NULL;
        data.WholeCaption = FALSE;
        int err = 0;
        SalamanderGeneral->ViewFileInPluginViewer(NULL, &data, FALSE, NULL, NULL, err);
        if (returnLock)
        {
            *lock = NULL;
            *lockOwner = FALSE;
        }
        return TRUE;
    }

    TRACE_I("codeview: ViewFile (t=" << GetTickCount64() << " ms)");

    // The FIRST actual view of a session is the only trigger for engine work:
    // it arms this plugin's session keeper so every later view attaches to a
    // warm browser tree (065 FR-001 parity -- nothing happens before this).
    if (g_keepReady)
    {
        TcWebKeeperConfig kc;
        kc.ClassName = CV_KEEPER_CLASS;
        kc.Instance = DLLInstance;
        kc.TraceName = "codeview keeper";
        CvKeeper.Arm(kc);
    }

    HANDLE contEvent = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (contEvent == NULL)
        return FALSE;
    BOOL success = FALSE;
    CViewerThread* t = new CViewerThread(name, left, top, width, height, showCmd, alwaysOnTop, returnLock,
                                         lock, lockOwner, contEvent, &success, enumFilesSourceUID,
                                         enumFilesCurrentIndex);
    if (t != NULL)
    {
        if (t->Create(ThreadQueue) != NULL)
            WaitForSingleObject(contEvent, INFINITE);
        else
            delete t;
    }
    HANDLES(CloseHandle(contEvent));
    return success;
}

// ==========================================================================
// Find / Go-to-line dialogs
// ==========================================================================

struct CvFindDlgData
{
    wchar_t* Text;
    BOOL* Case;
    BOOL* WholeWord;
};

static INT_PTR CALLBACK FindDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // feature 049: raw dialog proc - the two-touchpoint theme pattern
    if (msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC)
    {
        INT_PTR brush;
        if (SalamanderGeneral->ThemeHandleCtlColor(msg, wParam, lParam, &brush))
            return brush;
    }
    CvFindDlgData* d = (CvFindDlgData*)GetWindowLongPtr(hDlg, DWLP_USER);
    switch (msg)
    {
    case WM_INITDIALOG:
        SetWindowLongPtr(hDlg, DWLP_USER, lParam);
        d = (CvFindDlgData*)lParam;
        SetDlgItemTextW(hDlg, IDC_FIND_TEXT, d->Text);
        CheckDlgButton(hDlg, IDC_FIND_CASE, *d->Case ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_FIND_WHOLEWORD, *d->WholeWord ? BST_CHECKED : BST_UNCHECKED);
        SalamanderGeneral->ThemeApplyToDialog(hDlg);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK && d != NULL)
        {
            GetDlgItemTextW(hDlg, IDC_FIND_TEXT, d->Text, 256);
            *d->Case = IsDlgButtonChecked(hDlg, IDC_FIND_CASE) == BST_CHECKED;
            *d->WholeWord = IsDlgButtonChecked(hDlg, IDC_FIND_WHOLEWORD) == BST_CHECKED;
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

static INT_PTR CALLBACK GotoDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC)
    {
        INT_PTR brush;
        if (SalamanderGeneral->ThemeHandleCtlColor(msg, wParam, lParam, &brush))
            return brush;
    }
    wchar_t* out = (wchar_t*)GetWindowLongPtr(hDlg, DWLP_USER);
    switch (msg)
    {
    case WM_INITDIALOG:
        SetWindowLongPtr(hDlg, DWLP_USER, lParam);
        SalamanderGeneral->ThemeApplyToDialog(hDlg);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK && out != NULL)
        {
            GetDlgItemTextW(hDlg, IDC_GOTO_TEXT, out, 32);
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

// ==========================================================================
// CViewerWindow
// ==========================================================================

CViewerWindow::CViewerWindow(int enumFilesSourceUID, int enumFilesCurrentIndex)
    : CWindow(ooStatic)
{
    Lock = NULL;
    Name = NULL;
    Web = NULL;
    HStatus = NULL;
    BgBrush = NULL;
    HSchemeMenu = NULL;
    HLangMenu = NULL;
    HEncodingMenu = NULL;
    ForcedEncoding = -1;
    ForcedLanguage = -1;
    DocVersion = 0;
    FindText[0] = 0;
    FindCase = FALSE;
    FindWholeWord = FALSE;
    FindCurrent = 0;
    FindTotal = 0;
    CaretLine = 1;
    CaretCol = 1;
    PageReady = FALSE;
    DarkMenus = SalamanderGeneral->IsDarkThemeActive();
    EnumFilesSourceUID = enumFilesSourceUID;
    EnumFilesCurrentIndex = enumFilesCurrentIndex;
}

CViewerWindow::~CViewerWindow()
{
    if (Lock != NULL)
    {
        SetEvent(Lock);
        Lock = NULL;
    }
    if (Name != NULL)
    {
        free(Name);
        Name = NULL;
    }
    if (BgBrush != NULL)
    {
        DeleteObject(BgBrush);
        BgBrush = NULL;
    }
}

HANDLE CViewerWindow::GetLock()
{
    if (Lock == NULL)
        Lock = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    return Lock;
}

void CViewerWindow::OpenFile(const char* name, BOOL setLock)
{
    if (Name != NULL)
        free(Name);
    Name = _strdup(name);
    ForcedEncoding = -1;
    ForcedLanguage = -1;
    FindCurrent = FindTotal = 0;
    CaretLine = CaretCol = 1;

    if (!CvLoadFile(Name, ForcedEncoding, ForcedLanguage, Intake))
    {
        // Between CanViewFile and here the file changed, vanished, or turned
        // out binary: say so in the window instead of rendering garbage, and
        // offer the built-in viewer (spec FR-029).
        ShowBlockedNotice(LoadStr(Intake.Band == cvBandDeclined ? IDS_BINARY_NOTICE : IDS_LOAD_ERROR));
        UpdateTitle();
        UpdateStatus();
        if (setLock && Lock != NULL)
        {
            SetEvent(Lock);
            Lock = NULL;
        }
        return;
    }

    DocVersion++;
    if (Web != NULL && Web->IsReady() && PageReady)
        SendInit(TRUE); // same window, next file: swap content, no navigation
    else if (Web != NULL && Web->IsReady())
        Web->Navigate(DocVersion, CvSchemeFragment(CvEffectiveScheme()));

    UpdateTitle();
    UpdateStatus();
    RefreshChecks();

    // The file has been read into memory; a temporary copy may go now.
    if (setLock && Lock != NULL)
    {
        SetEvent(Lock);
        Lock = NULL;
    }
}

void CViewerWindow::ShowBlockedNotice(const char* text)
{
    SalamanderGeneral->SalMessageBox(HWindow, text, LoadStr(IDS_PLUGINNAME),
                                     MB_OK | MB_ICONINFORMATION);
}

void CViewerWindow::ApplyScheme(BOOL rebuildBrush)
{
    const CvScheme* s = CvEffectiveScheme();
    if (rebuildBrush)
    {
        if (BgBrush != NULL)
            DeleteObject(BgBrush);
        BgBrush = CreateSolidBrush(s->Bg);
        InvalidateRect(HWindow, NULL, TRUE);
    }
    if (Web != NULL)
        Web->SetBackgroundColor(s->Bg);
}

void CViewerWindow::SendInit(BOOL swap)
{
    if (Web == NULL || !Web->IsReady())
        return;
    Web->PostWebMessageJson(CvMsgInit(Intake, CvEffectiveScheme(), swap));
}

void CViewerWindow::BuildMenu()
{
    HMENU menu = CreateMenu();
    HMENU file = CreatePopupMenu();
    AppendMenuA(file, MF_STRING, CM_NEXTFILE, LoadStr(IDS_MENU_FILE_NEXTFILE));
    AppendMenuA(file, MF_STRING, CM_PREVFILE, LoadStr(IDS_MENU_FILE_PREVFILE));
    AppendMenuA(file, MF_SEPARATOR, 0, NULL);
    AppendMenuA(file, MF_STRING, CM_FILE_CLOSE, LoadStr(IDS_MENU_FILE_CLOSE));
    AppendMenuA(menu, MF_POPUP | MF_STRING, (UINT_PTR)file, LoadStr(IDS_MENU_FILE));

    HMENU edit = CreatePopupMenu();
    AppendMenuA(edit, MF_STRING, CM_EDIT_COPY, LoadStr(IDS_MENU_EDIT_COPY));
    AppendMenuA(edit, MF_STRING, CM_EDIT_SELALL, LoadStr(IDS_MENU_EDIT_SELALL));
    AppendMenuA(edit, MF_SEPARATOR, 0, NULL);
    AppendMenuA(edit, MF_STRING, CM_EDIT_FIND, LoadStr(IDS_MENU_EDIT_FIND));
    AppendMenuA(edit, MF_STRING, CM_EDIT_FINDNEXT, LoadStr(IDS_MENU_EDIT_FINDNEXT));
    AppendMenuA(edit, MF_STRING, CM_EDIT_FINDPREV, LoadStr(IDS_MENU_EDIT_FINDPREV));
    AppendMenuA(edit, MF_SEPARATOR, 0, NULL);
    AppendMenuA(edit, MF_STRING, CM_EDIT_GOTO, LoadStr(IDS_MENU_EDIT_GOTO));
    AppendMenuA(menu, MF_POPUP | MF_STRING, (UINT_PTR)edit, LoadStr(IDS_MENU_EDIT));

    HMENU view = CreatePopupMenu();
    HSchemeMenu = CreatePopupMenu();
    for (int i = 0; i < CvSchemeCount; i++)
        AppendMenuA(HSchemeMenu, MF_STRING, CM_SCHEME_FIRST + i, LoadStr(IDS_SCHEME_FIRST + i));
    AppendMenuA(HSchemeMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(HSchemeMenu, MF_STRING, CM_VIEW_FOLLOWAPP, LoadStr(IDS_MENU_VIEW_FOLLOWAPP));
    AppendMenuA(view, MF_POPUP | MF_STRING, (UINT_PTR)HSchemeMenu, LoadStr(IDS_MENU_VIEW_SCHEME));

    // The language picker lists only what can actually be applied: the
    // languages a shipped grammar backs, plus "Automatic" (spec FR-007).
    HLangMenu = CreatePopupMenu();
    AppendMenuA(HLangMenu, MF_STRING, CM_LANG_AUTO, LoadStr(IDS_MENU_LANG_AUTO));
    AppendMenuA(HLangMenu, MF_SEPARATOR, 0, NULL);
    {
        HMENU sub = NULL;
        int inSub = 0;
        char group[2] = {0, 0};
        for (int i = 0; i < CvLanguageCount; i++)
        {
            if (CvLanguages[i].Grammar == NULL)
                continue;
            char first = (char)toupper((unsigned char)CvLanguages[i].Display[0]);
            if (sub == NULL || first != group[0] || inSub >= 30)
            {
                if (sub != NULL)
                {
                    char label[8] = {group[0], 0};
                    AppendMenuA(HLangMenu, MF_POPUP | MF_STRING, (UINT_PTR)sub, label);
                }
                sub = CreatePopupMenu();
                group[0] = first;
                inSub = 0;
            }
            AppendMenuA(sub, MF_STRING, CM_LANG_FIRST + i, CvLanguages[i].Display);
            inSub++;
        }
        if (sub != NULL)
        {
            char label[8] = {group[0], 0};
            AppendMenuA(HLangMenu, MF_POPUP | MF_STRING, (UINT_PTR)sub, label);
        }
    }
    AppendMenuA(view, MF_POPUP | MF_STRING, (UINT_PTR)HLangMenu, LoadStr(IDS_MENU_VIEW_LANGUAGE));

    HEncodingMenu = CreatePopupMenu();
    static const char* encNames[] = {"UTF-8", "UTF-8 with BOM", "UTF-16 LE", "UTF-16 BE", "System code page"};
    for (int i = 0; i < 5; i++)
        AppendMenuA(HEncodingMenu, MF_STRING, CM_ENCODING_FIRST + i, encNames[i]);
    AppendMenuA(view, MF_POPUP | MF_STRING, (UINT_PTR)HEncodingMenu, LoadStr(IDS_MENU_VIEW_ENCODING));

    AppendMenuA(view, MF_SEPARATOR, 0, NULL);
    AppendMenuA(view, MF_STRING, CM_VIEW_WRAP, LoadStr(IDS_MENU_VIEW_WRAP));
    AppendMenuA(view, MF_STRING, CM_VIEW_LINENUMS, LoadStr(IDS_MENU_VIEW_LINENUMS));
    AppendMenuA(view, MF_STRING, CM_VIEW_WHITESPACE, LoadStr(IDS_MENU_VIEW_WHITESPACE));
    AppendMenuA(view, MF_SEPARATOR, 0, NULL);
    AppendMenuA(view, MF_STRING, CM_VIEW_ZOOMIN, LoadStr(IDS_MENU_VIEW_ZOOMIN));
    AppendMenuA(view, MF_STRING, CM_VIEW_ZOOMOUT, LoadStr(IDS_MENU_VIEW_ZOOMOUT));
    AppendMenuA(view, MF_STRING, CM_VIEW_ZOOMRESET, LoadStr(IDS_MENU_VIEW_ZOOMRESET));
    AppendMenuA(menu, MF_POPUP | MF_STRING, (UINT_PTR)view, LoadStr(IDS_MENU_VIEW));

    HMENU help = CreatePopupMenu();
    AppendMenuA(help, MF_STRING, CM_HELP_ABOUT, LoadStr(IDS_MENU_HELP_ABOUT));
    AppendMenuA(menu, MF_POPUP | MF_STRING, (UINT_PTR)help, LoadStr(IDS_MENU_HELP));

    SetMenu(HWindow, menu);
    if (DarkMenus)
        DarkMenuApply(menu); // feature 037: owner-drawn dark menu bar
    RefreshChecks();
}

void CViewerWindow::RefreshChecks()
{
    if (HSchemeMenu != NULL)
    {
        const CvScheme* eff = CvEffectiveScheme();
        for (int i = 0; i < CvSchemeCount; i++)
            CheckMenuItem(HSchemeMenu, CM_SCHEME_FIRST + i,
                          MF_BYCOMMAND | (&CvSchemes[i] == eff ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(HSchemeMenu, CM_VIEW_FOLLOWAPP,
                      MF_BYCOMMAND | (g_followApp ? MF_CHECKED : MF_UNCHECKED));
    }
    HMENU menu = GetMenu(HWindow);
    if (menu != NULL)
    {
        CheckMenuItem(menu, CM_VIEW_WRAP, MF_BYCOMMAND | (g_wrap ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(menu, CM_VIEW_LINENUMS, MF_BYCOMMAND | (g_lineNumbers ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(menu, CM_VIEW_WHITESPACE, MF_BYCOMMAND | (g_whitespace ? MF_CHECKED : MF_UNCHECKED));
    }
    if (HEncodingMenu != NULL)
        for (int i = 0; i < 5; i++)
            CheckMenuItem(HEncodingMenu, CM_ENCODING_FIRST + i,
                          MF_BYCOMMAND | ((int)Intake.Encoding == i ? MF_CHECKED : MF_UNCHECKED));
    if (HLangMenu != NULL)
        CheckMenuItem(HLangMenu, CM_LANG_AUTO,
                      MF_BYCOMMAND | (ForcedLanguage < 0 ? MF_CHECKED : MF_UNCHECKED));
}

void CViewerWindow::LayoutChildren()
{
    RECT rc;
    GetClientRect(HWindow, &rc);
    int statusH = (HStatus != NULL) ? CV_STATUS_HEIGHT : 0;
    if (HStatus != NULL)
        SetWindowPos(HStatus, NULL, 0, rc.bottom - statusH, rc.right, statusH, SWP_NOZORDER);
    if (Web != NULL)
        Web->Resize(rc.right, rc.bottom - statusH);
}

void CViewerWindow::UpdateTitle()
{
    // The name is UTF-8 and may be outside the code page, so the title is set
    // through the wide API (feature 069's rule for every user-visible caption).
    std::string title = Name != NULL ? Name : "";
    title += " - ";
    title += LoadStr(IDS_WINDOW_TITLE);
    if (g_zoom != 100)
    {
        char z[16];
        _snprintf_s(z, _TRUNCATE, " (%d%%)", g_zoom);
        title += z;
    }
    wchar_t* w = SplU8ToWAlloc(title.c_str());
    if (w != NULL)
    {
        SetWindowTextW(HWindow, w);
        free(w);
    }
}

void CViewerWindow::UpdateStatus()
{
    if (HStatus == NULL)
        return;
    static const char* encNames[] = {"UTF-8", "UTF-8 BOM", "UTF-16 LE", "UTF-16 BE", "ANSI"};
    const char* eol = LoadStr(Intake.Eol == cvEolCRLF    ? IDS_STATUS_EOL_CRLF
                              : Intake.Eol == cvEolLF    ? IDS_STATUS_EOL_LF
                              : Intake.Eol == cvEolCR    ? IDS_STATUS_EOL_CR
                              : Intake.Eol == cvEolMixed ? IDS_STATUS_EOL_MIXED
                                                         : IDS_STATUS_EOL_NONE);
    char lines[64];
    _snprintf_s(lines, _TRUNCATE, LoadStr(IDS_STATUS_LINES), Intake.LineCount);
    char lncol[64];
    _snprintf_s(lncol, _TRUNCATE, LoadStr(IDS_STATUS_LNCOL), CaretLine, CaretCol);

    char text[512];
    _snprintf_s(text, _TRUNCATE, "%s | %s | %s | %s | %s | %d%%",
                lines, lncol,
                encNames[Intake.Encoding <= cvEncAnsi ? Intake.Encoding : 0], eol,
                CvLanguageDisplay(Intake.Language), g_zoom);
    wchar_t* w = SplU8ToWAlloc(text);
    if (w != NULL)
    {
        SetWindowTextW(HStatus, w);
        free(w);
    }
}

void CViewerWindow::SelectScheme(int idx)
{
    if (idx < 0 || idx >= CvSchemeCount)
        return;
    g_followApp = 0;
    lstrcpynA(g_scheme, CvSchemes[idx].Id, 32);
    if (CvSchemes[idx].Dark)
        lstrcpynA(g_schemeDark, CvSchemes[idx].Id, 32);
    else
        lstrcpynA(g_schemeLight, CvSchemes[idx].Id, 32);
    ApplyScheme(TRUE);
    if (Web != NULL && Web->IsReady())
        Web->PostWebMessageJson(CvMsgSetTheme(&CvSchemes[idx]));
    RefreshChecks();
}

void CViewerWindow::CycleScheme(int dir)
{
    const CvScheme* eff = CvEffectiveScheme();
    int cur = 0;
    for (int i = 0; i < CvSchemeCount; i++)
        if (&CvSchemes[i] == eff)
            cur = i;
    SelectScheme((cur + dir + CvSchemeCount) % CvSchemeCount);
}

void CViewerWindow::SelectLanguage(int language)
{
    ForcedLanguage = language;
    Intake.Language = language >= 0 ? language
                                    : CvIdentifyLanguage(Name != NULL ? Name : "", Intake.Utf8.c_str(),
                                                         (std::min<size_t>)(Intake.Utf8.size(), 8192));
    if (Web != NULL && Web->IsReady())
        Web->PostWebMessageJson(CvMsgSetLanguage(Intake.Language));
    UpdateStatus();
    RefreshChecks();
}

void CViewerWindow::SelectEncoding(int encoding)
{
    if (Name == NULL || encoding < 0 || encoding > cvEncAnsi)
        return;
    ForcedEncoding = encoding;
    if (!CvLoadFile(Name, ForcedEncoding, ForcedLanguage, Intake))
        return;
    DocVersion++;
    if (Web != NULL && Web->IsReady())
        SendInit(TRUE);
    UpdateStatus();
    RefreshChecks();
    if (Intake.InvalidBytes > 0)
    {
        char msg[256];
        _snprintf_s(msg, _TRUNCATE, LoadStr(IDS_ENCODING_INVALID), Intake.InvalidBytes);
        SalamanderGeneral->SalMessageBox(HWindow, msg, LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONINFORMATION);
    }
}

void CViewerWindow::CycleEncoding(int dir)
{
    int cur = (int)Intake.Encoding;
    SelectEncoding(((cur + dir) % 5 + 5) % 5);
}

void CViewerWindow::DoFind(BOOL prompt, int dir)
{
    if (prompt)
    {
        CvFindDlgData d = {FindText, &FindCase, &FindWholeWord};
        if (DialogBoxParamW(HLanguage, MAKEINTRESOURCEW(IDD_FIND), HWindow, FindDlgProc, (LPARAM)&d) != IDOK)
            return;
        dir = 0; // a new term always searches from the top of the view
    }
    if (FindText[0] == 0)
        return;
    if (Web != NULL && Web->IsReady())
        Web->PostWebMessageJson(CvMsgFind(FindText, FindCase, FindWholeWord, dir));
}

void CViewerWindow::DoGotoLine()
{
    wchar_t buf[32] = {0};
    if (DialogBoxParamW(HLanguage, MAKEINTRESOURCEW(IDD_GOTO), HWindow, GotoDlgProc, (LPARAM)buf) != IDOK)
        return;
    int line = _wtoi(buf);
    int col = 1;
    const wchar_t* colon = wcschr(buf, L':');
    if (colon != NULL)
        col = _wtoi(colon + 1);
    if (line <= 0)
        return;
    if (Web != NULL && Web->IsReady())
        Web->PostWebMessageJson(CvMsgGotoLine(line, col));
}

void CViewerWindow::SetZoom(int pct)
{
    g_zoom = (std::max)(50, (std::min)(300, pct));
    if (Web != NULL)
        Web->SetZoomPercent(g_zoom);
    UpdateTitle();
    UpdateStatus();
}

void CViewerWindow::NextFile(int dir)
{
    // Panel navigation (spec FR-041). The API documents the buffer as "at
    // least MAX_PATH"; a UTF-8 path can be longer than MAX_PATH characters, so
    // the buffer is sized for the UTF-8 worst case rather than MAX_PATH.
    if (EnumFilesSourceUID == -1)
        return;
    // BUFFER SIZE: spl_gen.h:2703 documents "at least MAX_PATH", but the core
    // fills this buffer with lstrcpyn(fileName, ..., SAL_MAX_PATH_UTF8)
    // (src/salamdr6.cpp:205,223) and its own callers declare
    // char[SAL_MAX_PATH_UTF8] (src/viewer3.cpp:967). A plugin that believed the
    // header would take a buffer overflow on a long path. SAL_MAX_PATH_UTF8
    // lives in the core-only header src/common/salpath.h, so the value is
    // restated here; at ~96 KB it is heap, not stack.
    const size_t kMaxPathUtf8 = 3 * 32767 + 1;
    std::vector<char> nameBuf(kMaxPathUtf8, 0);
    char* fileName = &nameBuf[0];
    BOOL noMoreFiles = FALSE;
    BOOL srcBusy = FALSE;
    int index = EnumFilesCurrentIndex;
    BOOL ok = SalamanderGeneral->GetNextFileNameForViewer(EnumFilesSourceUID, &index, Name,
                                                          dir > 0 ? FALSE : TRUE, TRUE, fileName,
                                                          &noMoreFiles, &srcBusy);
    if (!ok || fileName[0] == 0)
        return;
    // The next file must pass the same gate as an F3 would; if it does not,
    // say so in place instead of rendering it (spec FR-029).
    if (!CvCanView(fileName))
    {
        ShowBlockedNotice(LoadStr(IDS_BINARY_NOTICE));
        return;
    }
    EnumFilesCurrentIndex = index;
    OpenFile(fileName, FALSE);
}

void CViewerWindow::ShowContextMenu(int x, int y, BOOL hasSelection)
{
    HMENU menu = CreatePopupMenu();
    AppendMenuA(menu, MF_STRING | (hasSelection ? 0 : MF_GRAYED), CM_EDIT_COPY, LoadStr(IDS_MENU_EDIT_COPY));
    AppendMenuA(menu, MF_STRING, CM_EDIT_SELALL, LoadStr(IDS_MENU_EDIT_SELALL));
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, CM_EDIT_FIND, LoadStr(IDS_MENU_EDIT_FIND));
    AppendMenuA(menu, MF_STRING, CM_EDIT_GOTO, LoadStr(IDS_MENU_EDIT_GOTO));
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, CM_VIEW_WRAP, LoadStr(IDS_MENU_VIEW_WRAP));
    AppendMenuA(menu, MF_STRING, CM_VIEW_LINENUMS, LoadStr(IDS_MENU_VIEW_LINENUMS));
    if (DarkMenus)
        DarkMenuApply(menu);
    POINT pt = {x, y};
    ClientToScreen(HWindow, &pt);
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, HWindow, NULL);
    if (DarkMenus)
        DarkMenuRelease(menu); // free the owner-draw paint data before the menu goes
    DestroyMenu(menu);
}

void CViewerWindow::OnPageMessage(const std::wstring& json)
{
    CvPageMessage m;
    if (!CvParsePageMessage(json, m))
        return; // unknown or malformed: ignored by contract
    if (m.Type == L"ready")
    {
        PageReady = TRUE;
        SendInit(FALSE);
    }
    else if (m.Type == L"findResult")
    {
        FindCurrent = m.Current;
        FindTotal = m.Total;
        if (FindTotal == 0)
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_NOT_FOUND), LoadStr(IDS_PLUGINNAME),
                                             MB_OK | MB_ICONINFORMATION);
        UpdateStatus();
    }
    else if (m.Type == L"caret")
    {
        CaretLine = m.Line;
        CaretCol = m.Col;
        UpdateStatus();
    }
    else if (m.Type == L"contextMenu")
        ShowContextMenu(m.X, m.Y, m.HasSelection);
    else if (m.Type == L"highlightAborted")
    {
        // TRACE streams into a narrow ostream, so the reason is narrowed here
        // rather than streamed as wide text.
        char reason[128] = {0};
        if (!m.Reason.empty())
            WideCharToMultiByte(CP_ACP, 0, m.Reason.c_str(), -1, reason, sizeof(reason) - 1, NULL, NULL);
        TRACE_I("codeview: highlighting aborted (" << (reason[0] ? reason : "?") << ")");
    }
}

void CViewerWindow::EngineFailed()
{
    SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_ENGINE_UNAVAILABLE), LoadStr(IDS_PLUGINNAME),
                                     MB_OK | MB_ICONINFORMATION);
    PostMessage(HWindow, WM_CLOSE, 0, 0);
}

LRESULT CViewerWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        ViewerWindowQueue.Add(new CWindowQueueItem(HWindow));
        BuildMenu();
        ApplyScheme(TRUE);

        HStatus = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_NOPREFIX,
                                  0, 0, 0, 0, HWindow, NULL, DLLInstance, NULL);

        if (!CTcWebHost::RuntimeAvailable())
        {
            EngineFailed();
            return 0;
        }
        Web = new CTcWebHost();
        TcWebHostConfig cfg;
        // The interceptor reads the decoded text straight from this window's
        // intake; nothing is copied for serving. TextPtr is a member, so the
        // pointer the callback captures stays valid for the window's life.
        TextPtr = &Intake.Utf8;
        CvConfigureHost(cfg, &TextPtr);

        CTcWebHost::Callbacks cb;
        CViewerWindow* self = this;
        cb.OnReady = [self]()
        {
            self->Web->SetZoomPercent(g_zoom);
            self->Web->Navigate(self->DocVersion, CvSchemeFragment(CvEffectiveScheme()));
        };
        cb.OnInitFailed = [self]() { self->EngineFailed(); };
        cb.OnProcessFailed = [self]() { self->EngineFailed(); };
        cb.OnZoomChanged = [self](int pct)
        {
            g_zoom = pct;
            self->UpdateTitle();
            self->UpdateStatus();
        };
        cb.OnWebMessage = [self](const std::wstring& json) { self->OnPageMessage(json); };
        cb.OnActivateLink = [](const std::wstring&) {}; // nothing is linkable in a code view

        Web->Create(HWindow, TcWebUserDataFolder(), cfg, cb);
        // BEFORE the controller exists (mdview's pattern): the shared host
        // caches the colour and applies it before put_IsVisible, so the
        // WebView surface never flashes its white default (spec FR-015).
        Web->SetBackgroundColor(CvEffectiveScheme()->Bg);
        LayoutChildren();

        // The first view after installation explains where the built-in viewer
        // went (spec FR-012). Once only, and never modal-blocking the render.
        if (!g_hintShown)
        {
            g_hintShown = TRUE;
            PostMessage(HWindow, WM_USER_VIEWERCFGCHNG + 1, 0, 0);
        }
        return 0;
    }

    case WM_USER_VIEWERCFGCHNG + 1:
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_FIRSTRUN_HINT), LoadStr(IDS_PLUGINNAME),
                                         MB_OK | MB_ICONINFORMATION);
        return 0;

    case WM_USER_VIEWERCFGCHNG:
        ApplyScheme(TRUE);
        if (Web != NULL && Web->IsReady())
        {
            Web->PostWebMessageJson(CvMsgSetView());
            Web->PostWebMessageJson(CvMsgSetTheme(CvEffectiveScheme()));
        }
        RefreshChecks();
        UpdateStatus();
        return 0;

    case WM_ERASEBKGND:
    {
        // Paint the scheme colour, not the class brush's white: this is what
        // keeps a dark scheme dark from the very first frame (spec FR-015).
        if (BgBrush != NULL)
        {
            RECT rc;
            GetClientRect(HWindow, &rc);
            FillRect((HDC)wParam, &rc, BgBrush);
            return TRUE;
        }
        break;
    }

    case WM_SIZE:
        LayoutChildren();
        return 0;

    case WM_SETFOCUS:
        if (Web != NULL)
            Web->Focus();
        return 0;

    case WM_COMMAND:
    {
        int cmd = LOWORD(wParam);
        if (cmd >= CM_SCHEME_FIRST && cmd < CM_SCHEME_FIRST + CvSchemeCount)
        {
            SelectScheme(cmd - CM_SCHEME_FIRST);
            return 0;
        }
        if (cmd >= CM_LANG_FIRST && cmd < CM_LANG_FIRST + CvLanguageCount)
        {
            SelectLanguage(cmd - CM_LANG_FIRST);
            return 0;
        }
        if (cmd >= CM_ENCODING_FIRST && cmd < CM_ENCODING_FIRST + 5)
        {
            SelectEncoding(cmd - CM_ENCODING_FIRST);
            return 0;
        }
        switch (cmd)
        {
        case CM_FILE_CLOSE:
            PostMessage(HWindow, WM_CLOSE, 0, 0);
            return 0;
        case CM_LANG_AUTO:
            SelectLanguage(-1);
            return 0;
        case CM_EDIT_FIND:
            DoFind(TRUE, 0);
            return 0;
        case CM_EDIT_FINDNEXT:
            DoFind(FindText[0] == 0, +1);
            return 0;
        case CM_EDIT_FINDPREV:
            DoFind(FindText[0] == 0, -1);
            return 0;
        case CM_EDIT_GOTO:
            DoGotoLine();
            return 0;
        case CM_VIEW_FOLLOWAPP:
            g_followApp = !g_followApp;
            ApplyScheme(TRUE);
            if (Web != NULL && Web->IsReady())
                Web->PostWebMessageJson(CvMsgSetTheme(CvEffectiveScheme()));
            RefreshChecks();
            return 0;
        case CM_SCHEME_NEXT:
            CycleScheme(+1);
            return 0;
        case CM_SCHEME_PREV:
            CycleScheme(-1);
            return 0;
        case CM_ENCODING_NEXT:
            CycleEncoding(+1);
            return 0;
        case CM_ENCODING_PREV:
            CycleEncoding(-1);
            return 0;
        case CM_VIEW_WRAP:
            g_wrap = !g_wrap;
            goto viewChanged;
        case CM_VIEW_LINENUMS:
            g_lineNumbers = !g_lineNumbers;
            goto viewChanged;
        case CM_VIEW_WHITESPACE:
            g_whitespace = !g_whitespace;
            goto viewChanged;
        viewChanged:
            if (Web != NULL && Web->IsReady())
                Web->PostWebMessageJson(CvMsgSetView());
            RefreshChecks();
            return 0;
        case CM_VIEW_ZOOMIN:
            SetZoom(g_zoom + 10);
            return 0;
        case CM_VIEW_ZOOMOUT:
            SetZoom(g_zoom - 10);
            return 0;
        case CM_VIEW_ZOOMRESET:
            SetZoom(100);
            return 0;
        case CM_NEXTFILE:
            NextFile(+1);
            return 0;
        case CM_PREVFILE:
            NextFile(-1);
            return 0;
        case CM_HELP_ABOUT:
            OnAbout(HWindow);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        if (g_savePos)
        {
            g_wndPlacement.length = sizeof(g_wndPlacement);
            GetWindowPlacement(HWindow, &g_wndPlacement);
        }
        break;

    case WM_DESTROY:
        if (DarkMenus)
            DarkMenuRelease(GetMenu(HWindow)); // free owner-draw paint data
        if (Web != NULL)
        {
            Web->Destroy();
            delete Web;
            Web = NULL;
        }
        ViewerWindowQueue.Remove(HWindow);
        PostQuitMessage(0);
        return 0;

    case WM_MEASUREITEM:
        if (DarkMenus && wParam == 0 && DarkMenuMeasureItem((MEASUREITEMSTRUCT*)lParam))
            return TRUE;
        break;

    case WM_DRAWITEM:
        if (DarkMenus && wParam == 0 && DarkMenuDrawItem((const DRAWITEMSTRUCT*)lParam))
            return TRUE;
        break;

    case WM_MENUCHAR:
        if (DarkMenus)
        {
            // Owner-drawn items lose automatic '&' mnemonic matching.
            LRESULT r = DarkMenuHandleMenuChar((HMENU)lParam, wParam);
            if (HIWORD(r) != MNC_IGNORE)
                return r;
        }
        break;
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
