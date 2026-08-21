// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// viewer.cpp - the mdview viewer window: WebView2 host, thread/lock plumbing,
// menu, keyboard, search, zoom, color schemes, link security gate. The v1
// RTF/RichEdit path was retired in feature 021 (single HTML backend, FR-038a).

#include "precomp.h"
#include "render.h"
#include "htmlgen.h"
#include "webview.h"
#include "viewer.h"
#include "darkmenu.h"

CWindowQueue ViewerWindowQueue("MDView Viewers");
CThreadQueue ThreadQueue("MDView Viewers");

static HACCEL ViewerAccels = NULL;

#define SIZE_GATE (20LL * 1024 * 1024)

// ==========================================================================
// small local helpers
// ==========================================================================

static std::string WToUtf8Str(const std::wstring& w)
{
    if (w.empty())
        return std::string();
    int need = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string s;
    s.resize(need > 0 ? need : 0);
    if (need > 0)
        WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], need, NULL, NULL);
    return s;
}

// percent-decode a URI path component into UTF-16
static std::wstring UriDecode(const std::wstring& in)
{
    std::string bytes;
    for (size_t i = 0; i < in.size(); i++)
    {
        wchar_t c = in[i];
        if (c == L'%' && i + 2 < in.size())
        {
            auto hex = [](wchar_t h) -> int
            {
                if (h >= L'0' && h <= L'9')
                    return h - L'0';
                if (h >= L'a' && h <= L'f')
                    return h - L'a' + 10;
                if (h >= L'A' && h <= L'F')
                    return h - L'A' + 10;
                return -1;
            };
            int hi = hex(in[i + 1]), lo = hex(in[i + 2]);
            if (hi >= 0 && lo >= 0)
            {
                bytes += (char)((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        if (c < 0x80)
            bytes += (char)c;
        else
        {
            char buf[8];
            int n = WideCharToMultiByte(CP_UTF8, 0, &c, 1, buf, sizeof(buf), NULL, NULL);
            bytes.append(buf, n > 0 ? n : 0);
        }
    }
    int need = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), NULL, 0);
    std::wstring w;
    w.resize(need > 0 ? need : 0);
    if (need > 0)
        MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), &w[0], need);
    return w;
}

// ==========================================================================
// Init / release
// ==========================================================================

BOOL InitViewer()
{
    if (!InitializeWinLib(PluginNameEN, DLLInstance))
        return FALSE;
    SetWinLibStrings(LoadStr(IDS_INVALID_NUM), LoadStr(IDS_PLUGINNAME));
    SetupWinLibTheme(SalamanderGeneral); // feature 036: dark theme for WinLib dialogs

    ACCEL acc[] = {
        {FVIRTKEY | FCONTROL, 'F', CM_EDIT_FIND},
        {FVIRTKEY, VK_F3, CM_EDIT_FINDNEXT},
        {FVIRTKEY | FSHIFT, VK_F3, CM_EDIT_FINDPREV},
        {FVIRTKEY, VK_ESCAPE, CM_FILE_CLOSE},
        {FVIRTKEY | FCONTROL, 'U', CM_FILE_OPENTEXT},
        {FVIRTKEY | FCONTROL, VK_OEM_PLUS, CM_VIEW_ZOOMIN},
        {FVIRTKEY | FCONTROL, VK_ADD, CM_VIEW_ZOOMIN},
        {FVIRTKEY | FCONTROL, VK_OEM_MINUS, CM_VIEW_ZOOMOUT},
        {FVIRTKEY | FCONTROL, VK_SUBTRACT, CM_VIEW_ZOOMOUT},
        {FVIRTKEY | FCONTROL, '0', CM_VIEW_ZOOMRESET},
        {FVIRTKEY | FCONTROL, VK_NUMPAD0, CM_VIEW_ZOOMRESET},
        {FVIRTKEY, VK_F9, CM_SCHEME_NEXT},
        {FVIRTKEY | FSHIFT, VK_F9, CM_SCHEME_PREV},
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
// Viewer thread (adapted from demoview)
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
        : CThread("MDView Viewer")
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
            if (window->CreateEx(AlwaysOnTop ? WS_EX_TOPMOST : 0, CWINDOW_CLASSNAME2, "Markdown Viewer",
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

static void SpawnViewer(const char* utf8, int enumUID, int enumIdx,
                        BOOL returnLock, HANDLE* lock, BOOL* lockOwner)
{
    HANDLE contEvent = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (contEvent == NULL)
        return;
    BOOL success = FALSE;
    CViewerThread* t = new CViewerThread(utf8, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                         SW_SHOWNORMAL, FALSE, returnLock, lock, lockOwner, contEvent,
                                         &success, enumUID, enumIdx);
    if (t != NULL)
    {
        if (t->Create(ThreadQueue) != NULL)
            WaitForSingleObject(contEvent, INFINITE);
        else
            delete t;
    }
    HANDLES(CloseHandle(contEvent));
}

// ==========================================================================
// CPluginInterfaceForViewer
// ==========================================================================

BOOL WINAPI CPluginInterfaceForViewer::CanViewFile(const char* name)
{
    WCHAR* wp = SplU8ToWExtAlloc(name);
    if (wp == NULL)
        return TRUE; // let ViewFile try
    HANDLE h = CreateFileW(wp, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    free(wp);
    if (h == INVALID_HANDLE_VALUE)
        return FALSE; // cascade to the internal text viewer
    BYTE buf[512];
    DWORD rd = 0;
    ReadFile(h, buf, sizeof(buf), &rd, NULL);
    CloseHandle(h);
    bool bom = rd >= 2 && ((buf[0] == 0xFF && buf[1] == 0xFE) || (buf[0] == 0xFE && buf[1] == 0xFF));
    if (!bom)
        for (DWORD i = 0; i < rd; i++)
            if (buf[i] == 0)
                return FALSE; // binary -> let the text/hex viewer handle it
    return TRUE;
}

BOOL WINAPI CPluginInterfaceForViewer::ViewFile(const char* name, int left, int top, int width, int height,
                                                UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                                                BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                                                int enumFilesSourceUID, int enumFilesCurrentIndex)
{
    // Engine-unavailable fallback: ViewFile runs on the main thread, where the
    // internal text viewer may legally be opened (ViewFileInPluginViewer is
    // main-thread-only). Do this before spawning our own viewer thread.
    if (!CMdWebHost::RuntimeAvailable())
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

    TRACE_I("mdview: ViewFile (t=" << GetTickCount64() << " ms)"); // R8 timing

    // feature 065: the FIRST actual view of a session is the only trigger for
    // engine work - it retires the pre-065 cache folder and (unless the user
    // opted out, FR-008) arms the session keeper that keeps the WebView2
    // browser tree warm so every later view attaches instantly. Both calls
    // are asynchronous, idempotent, and never block F3.
    MdCleanupOldUserDataFolder();
    if (g_keepReady)
        MdKeeperArm();

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
// Find dialog
// ==========================================================================

static INT_PTR CALLBACK FindDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // feature 049: raw dialog proc - the two-touchpoint theme pattern
    // (WM_INITDIALOG apply + WM_CTLCOLOR* fallback; the SFTP/ZIP precedent)
    if (msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC)
    {
        INT_PTR brush;
        if (SalamanderGeneral->ThemeHandleCtlColor(msg, wParam, lParam, &brush))
            return brush;
    }

    switch (msg)
    {
    case WM_INITDIALOG:
        SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)lParam);
        if (lParam)
            SetDlgItemTextW(hDlg, IDC_FIND_TEXT, (const WCHAR*)lParam);
        SalamanderGeneral->ThemeApplyToDialog(hDlg);
        SetFocus(GetDlgItem(hDlg, IDC_FIND_TEXT));
        return FALSE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            WCHAR* buf = (WCHAR*)GetWindowLongPtr(hDlg, DWLP_USER);
            if (buf)
                GetDlgItemTextW(hDlg, IDC_FIND_TEXT, buf, 256);
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

CViewerWindow::CViewerWindow(int enumFilesSourceUID, int enumFilesCurrentIndex) : CWindow(ooStatic)
{
    Lock = NULL;
    Name = NULL;
    Web = NULL;
    HSchemeMenu = NULL;
    BgBrush = NULL;
    Theme = MdThemeById(g_scheme);
    if (Theme == NULL)
        Theme = MdThemeDefault(false);
    Encoding = MDENC_UTF8;
    FindText[0] = 0;
    FindIndex = -1;
    RemoteAllowed = false;
    RenderPending = false;
    SourceMode = false;
    DarkMenus = false;
    EnumFilesSourceUID = enumFilesSourceUID;
    EnumFilesCurrentIndex = enumFilesCurrentIndex;
}

CViewerWindow::~CViewerWindow()
{
    if (Web != NULL)
    {
        Web->Destroy();
        delete Web;
        Web = NULL;
    }
    if (BgBrush != NULL)
    {
        DeleteObject(BgBrush);
        BgBrush = NULL;
    }
    free(Name);
}

HANDLE CViewerWindow::GetLock()
{
    if (Lock == NULL)
        Lock = NOHANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    return Lock;
}

const MdTheme* CViewerWindow::EffectiveTheme()
{
    if (g_followSys)
    {
        BOOL light = TRUE;
        if (SalamanderGeneral->IsDarkThemeActive())
        {
            // feature 036: the application's Dark theme (Options > Theme) takes
            // precedence over the OS light/dark setting
            light = FALSE;
        }
        else
        {
            HKEY k;
            if (RegOpenKeyExW(HKEY_CURRENT_USER,
                              L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                              0, KEY_READ, &k) == ERROR_SUCCESS)
            {
                DWORD v = 1, sz = sizeof(v), type = 0;
                if (RegQueryValueExW(k, L"AppsUseLightTheme", NULL, &type, (LPBYTE)&v, &sz) == ERROR_SUCCESS)
                    light = (v != 0);
                RegCloseKey(k);
            }
        }
        const MdTheme* t = MdThemeById(light ? g_schemeLight : g_schemeDark);
        if (t)
            return t;
    }
    const MdTheme* t = MdThemeById(g_scheme);
    return t ? t : MdThemeDefault(false);
}

void CViewerWindow::BuildMenu()
{
    HMENU bar = CreateMenu();

    HMENU file = CreatePopupMenu();
    AppendMenuA(file, MF_STRING, CM_FILE_OPENTEXT, LoadStr(IDS_MENU_FILE_OPENTEXT));
    AppendMenuA(file, MF_SEPARATOR, 0, NULL);
    AppendMenuA(file, MF_STRING, CM_FILE_CLOSE, LoadStr(IDS_MENU_FILE_CLOSE));
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)file, LoadStr(IDS_MENU_FILE));

    HMENU edit = CreatePopupMenu();
    AppendMenuA(edit, MF_STRING, CM_EDIT_FIND, LoadStr(IDS_MENU_EDIT_FIND));
    AppendMenuA(edit, MF_STRING, CM_EDIT_FINDNEXT, LoadStr(IDS_MENU_EDIT_FINDNEXT));
    AppendMenuA(edit, MF_STRING, CM_EDIT_FINDPREV, LoadStr(IDS_MENU_EDIT_FINDPREV));
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)edit, LoadStr(IDS_MENU_EDIT));

    HMENU view = CreatePopupMenu();
    HSchemeMenu = CreatePopupMenu();
    for (int i = 0; i < MdThemeCount; i++)
        AppendMenuA(HSchemeMenu, MF_STRING, CM_SCHEME_FIRST + i, LoadStr(MdThemes[i].nameStrId));
    AppendMenuA(HSchemeMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(HSchemeMenu, MF_STRING, CM_VIEW_FOLLOWSYS, LoadStr(IDS_MENU_VIEW_FOLLOWSYS));
    AppendMenuA(view, MF_POPUP, (UINT_PTR)HSchemeMenu, LoadStr(IDS_MENU_VIEW_SCHEME));
    AppendMenuA(view, MF_SEPARATOR, 0, NULL);
    AppendMenuA(view, MF_STRING, CM_VIEW_ZOOMIN, LoadStr(IDS_MENU_VIEW_ZOOMIN));
    AppendMenuA(view, MF_STRING, CM_VIEW_ZOOMOUT, LoadStr(IDS_MENU_VIEW_ZOOMOUT));
    AppendMenuA(view, MF_STRING, CM_VIEW_ZOOMRESET, LoadStr(IDS_MENU_VIEW_ZOOMRESET));
    AppendMenuA(view, MF_SEPARATOR, 0, NULL);
    AppendMenuA(view, MF_STRING, CM_VIEW_REMOTEIMG, LoadStr(IDS_MENU_VIEW_REMOTEIMG));
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)view, LoadStr(IDS_MENU_VIEW));

    HMENU help = CreatePopupMenu();
    AppendMenuA(help, MF_STRING, CM_HELP_ABOUT, LoadStr(IDS_MENU_HELP_ABOUT));
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)help, LoadStr(IDS_MENU_HELP));

    SetMenu(HWindow, bar);
    if (DarkMenus)
        DarkMenuApply(bar); // owner-drawn dark rendering (feature 037)
    RefreshSchemeChecks();
}

void CViewerWindow::RefreshSchemeChecks()
{
    if (HSchemeMenu == NULL)
        return;
    const MdTheme* t = MdThemeById(g_scheme);
    int idx = t ? MdThemeIndex(t) : 0;
    CheckMenuRadioItem(HSchemeMenu, CM_SCHEME_FIRST, CM_SCHEME_FIRST + MdThemeCount - 1,
                       CM_SCHEME_FIRST + idx, MF_BYCOMMAND);
    CheckMenuItem(HSchemeMenu, CM_VIEW_FOLLOWSYS, MF_BYCOMMAND | (g_followSys ? MF_CHECKED : MF_UNCHECKED));
    HMENU bar = GetMenu(HWindow);
    if (bar)
    {
        CheckMenuItem(bar, CM_VIEW_REMOTEIMG, MF_BYCOMMAND | (RemoteAllowed ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(bar, CM_FILE_OPENTEXT, MF_BYCOMMAND | (SourceMode ? MF_CHECKED : MF_UNCHECKED));
    }
}

void CViewerWindow::UpdateTitle()
{
    std::wstring title = FilePathW;
    if (!title.empty())
        title += L" - ";
    title += L"Markdown Viewer";
    if (SourceMode)
        title += L" [Source]";
    if (Encoding == MDENC_ANSI)
        title += L" [ANSI]";
    else if (Encoding == MDENC_UTF16LE || Encoding == MDENC_UTF16BE)
        title += L" [UTF-16]";
    wchar_t z[16];
    _snwprintf_s(z, _TRUNCATE, L" (%d%%)", g_zoom);
    title += z;
    SetWindowTextW(HWindow, title.c_str());
}

// (Re)generate Html from DecodedText with the current theme/find/consent/source
// state, without navigating.
void CViewerWindow::RebuildHtml()
{
    Theme = EffectiveTheme();
    if (BgBrush != NULL)
        DeleteObject(BgBrush);
    BgBrush = CreateSolidBrush(Theme->docBg);
    if (Web != NULL)
        Web->SetBackgroundColor(Theme->docBg);
    std::string srcUtf8 = WToUtf8Str(DecodedText);
    std::wstring findTerm = FindText[0] ? std::wstring(FindText) : std::wstring();
    if (SourceMode)
        MdBuildSourceHtml(srcUtf8, *Theme, Html, findTerm);
    else
        MdRenderHtml(srcUtf8, *Theme, DocDir, Html, findTerm, RemoteAllowed);
    UpdateTitle();
}

// Serve the current Html and (if the surface is ready) navigate; otherwise
// defer until OnReady. SetDocument bumps the doc version -> full reload.
void CViewerWindow::ShowDocument(const std::wstring& fragment)
{
    if (Web != NULL)
    {
        Web->SetDocument(&Html, DocDir);
        Web->SetZoomPercent(g_zoom);
        if (Web->IsReady())
            Web->Navigate(fragment);
        else
            RenderPending = true;
    }
}

void CViewerWindow::Regenerate(const std::wstring& fragment)
{
    RebuildHtml();
    ShowDocument(fragment);
}

void CViewerWindow::RenderDocument()
{
    std::vector<BYTE> bytes;
    bool loaded = false;

    WCHAR* wext = Name ? SplU8ToWExtAlloc(Name) : NULL;
    if (wext != NULL)
    {
        HANDLE h = CreateFileW(wext, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        free(wext);
        if (h != INVALID_HANDLE_VALUE)
        {
            LARGE_INTEGER sz;
            if (GetFileSizeEx(h, &sz))
            {
                if (sz.QuadPart > SIZE_GATE)
                {
                    // Too large to render as Markdown; show the raw source
                    // instead (reading is cheap - it is the parse/layout that the
                    // gate protects against). Capped at 64 MB.
                    LONGLONG cap = 64LL * 1024 * 1024;
                    DWORD n = (DWORD)(sz.QuadPart < cap ? sz.QuadPart : cap);
                    bytes.resize(n);
                    DWORD rd = 0;
                    if (ReadFile(h, n ? &bytes[0] : NULL, n, &rd, NULL))
                        bytes.resize(rd);
                    else
                        bytes.clear();
                    CloseHandle(h);
                    Encoding = MdDetectDecode(bytes.data(), bytes.size(), DecodedText);
                    SourceMode = true;
                    RefreshSchemeChecks();
                    Regenerate();
                    return;
                }
                DWORD n = (DWORD)sz.QuadPart;
                bytes.resize(n);
                DWORD rd = 0;
                if (ReadFile(h, n ? &bytes[0] : NULL, n, &rd, NULL))
                {
                    bytes.resize(rd);
                    loaded = true;
                }
            }
            CloseHandle(h);
        }
    }

    if (!loaded)
    {
        DecodedText = L"# Cannot open\n\nThe file could not be opened.";
        Regenerate();
        return;
    }

    Encoding = MdDetectDecode(bytes.data(), bytes.size(), DecodedText);
    if (Encoding == MDENC_BINARY)
    {
        // CanViewFile already declines binary (cascades to the text viewer); this
        // is a belt-and-suspenders placeholder if we are reached anyway.
        DecodedText = L"# Not a text file\n\nThis file does not appear to be text.";
        SourceMode = false;
    }
    Regenerate();
}

void CViewerWindow::SetZoom(int pct)
{
    if (pct < 50)
        pct = 50;
    if (pct > 300)
        pct = 300;
    g_zoom = pct;
    if (Web != NULL)
        Web->SetZoomPercent(g_zoom);
    UpdateTitle();
}

void CViewerWindow::SelectScheme(int idx)
{
    if (idx < 0 || idx >= MdThemeCount)
        return;
    lstrcpynA(g_scheme, MdThemes[idx].id, sizeof(g_scheme));
    if (MdThemes[idx].dark)
        lstrcpynA(g_schemeDark, MdThemes[idx].id, sizeof(g_schemeDark));
    else
        lstrcpynA(g_schemeLight, MdThemes[idx].id, sizeof(g_schemeLight));
    RefreshSchemeChecks();
    Regenerate();
}

void CViewerWindow::CycleScheme(int dir)
{
    const MdTheme* t = MdThemeById(g_scheme);
    int idx = t ? MdThemeIndex(t) : 0;
    idx = (idx + dir + MdThemeCount) % MdThemeCount;
    SelectScheme(idx);
}

void CViewerWindow::DoFind(BOOL forward, BOOL prompt)
{
    if (prompt || FindText[0] == 0)
    {
        if (DialogBoxParamW(HLanguage, MAKEINTRESOURCEW(IDD_FIND), HWindow, FindDlgProc,
                            (LPARAM)FindText) != IDOK)
            return;
        if (FindText[0] == 0)
            return;
        // Regenerate with <mark>s for the (new) term and re-serve it. SetDocument
        // bumps the doc version, so the single Navigate below is a full reload
        // that carries the new marks (fixes the v021 double-navigation race).
        FindIndex = -1;
        RebuildHtml();
        if (Web != NULL)
            Web->SetDocument(&Html, DocDir);
        if (Html.matchCount <= 0 && Web != NULL && Web->IsReady())
            Web->Navigate(); // reload to clear any previous term's highlights
    }
    if (Html.matchCount <= 0)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_NOT_FOUND), LoadStr(IDS_WINDOW_TITLE),
                                         MB_OK | MB_ICONINFORMATION);
        return;
    }
    FindIndex += forward ? 1 : -1;
    if (FindIndex < 0)
        FindIndex = Html.matchCount - 1;
    if (FindIndex >= Html.matchCount)
        FindIndex = 0;
    if (Web != NULL && Web->IsReady())
    {
        Web->Navigate(L"mdfind-" + std::to_wstring(FindIndex));
        Web->Focus();
    }
}

void CViewerWindow::EngineFailed()
{
    // The missing-runtime case is handled up front in ViewFile on the main
    // thread; if we reach here the controller failed to initialize despite the
    // runtime being present. We cannot safely open the internal text viewer from
    // this worker thread (ViewFileInPluginViewer is main-thread-only), so report
    // and close.
    SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_ENGINE_UNAVAILABLE),
                                     LoadStr(IDS_WINDOW_TITLE), MB_OK | MB_ICONWARNING);
    DestroyWindow(HWindow);
}

// Handles a cancelled navigation (link click) routed from the WebView2 host.
void CViewerWindow::ActivateLink(const std::wstring& uri)
{
    const std::wstring host = L"https://mdview.invalid/";
    if (uri.compare(0, host.size(), host) == 0)
    {
        // relative link inside the document
        std::wstring rel = uri.substr(host.size());
        size_t hash = rel.find(L'#');
        if (hash != std::wstring::npos)
            rel = rel.substr(0, hash);
        size_t q = rel.find(L'?');
        if (q != std::wstring::npos)
            rel = rel.substr(0, q);
        if (rel.empty() || rel == L"doc.html")
            return; // our own document
        rel = UriDecode(rel);

        std::wstring lower = rel;
        for (wchar_t& ch : lower)
            ch = (wchar_t)towlower(ch);
        bool isMd = (lower.size() > 3 && lower.substr(lower.size() - 3) == L".md") ||
                    (lower.size() > 9 && lower.substr(lower.size() - 9) == L".markdown");
        for (wchar_t& ch : rel)
            if (ch == L'/')
                ch = L'\\';
        if (isMd && !DocDir.empty() && rel.find(L':') == std::wstring::npos && rel.compare(0, 2, L"\\\\") != 0)
        {
            std::wstring full = DocDir;
            if (!full.empty() && full.back() != L'\\')
                full += L'\\';
            full += rel;
            char* u8 = SplWToU8Alloc(full.c_str());
            if (u8 != NULL)
            {
                SpawnViewer(u8, -1, -1, FALSE, NULL, NULL);
                free(u8);
            }
            return;
        }
        // other local target: show the resolved path only (never launched)
        std::wstring full = DocDir;
        if (!full.empty() && full.back() != L'\\')
            full += L'\\';
        full += rel;
        std::wstring msg = full + L"\n\n";
        char* u8 = SplWToU8Alloc(msg.c_str());
        SalamanderGeneral->SalMessageBox(HWindow, u8 ? u8 : LoadStr(IDS_LINK_BLOCKED),
                                         LoadStr(IDS_WINDOW_TITLE), MB_OK | MB_ICONINFORMATION);
        if (u8)
            free(u8);
        return;
    }

    // absolute scheme: allowlist http/https/mailto only (no ftp; FR-034)
    std::wstring scheme;
    size_t colon = uri.find(L':');
    if (colon != std::wstring::npos && colon > 0)
    {
        scheme = uri.substr(0, colon);
        for (wchar_t& ch : scheme)
            ch = (wchar_t)towlower(ch);
    }
    if (scheme == L"http" || scheme == L"https" || scheme == L"mailto")
        ShellExecuteW(HWindow, L"open", uri.c_str(), NULL, NULL, SW_SHOWNORMAL);
    else
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_LINK_BLOCKED),
                                         LoadStr(IDS_WINDOW_TITLE), MB_OK | MB_ICONWARNING);
}

void CViewerWindow::OpenFile(const char* name, BOOL setLock)
{
    if (setLock && Lock != NULL)
    {
        SetEvent(Lock);
        Lock = NULL;
    }
    free(Name);
    Name = _strdup(name);

    WCHAR* wp = SplU8ToWAlloc(name);
    if (wp != NULL)
    {
        FilePathW = wp;
        std::wstring d = wp;
        size_t slash = d.find_last_of(L"\\/");
        DocDir = (slash != std::wstring::npos) ? d.substr(0, slash) : L"";
        free(wp);
    }
    RenderDocument();
}

LRESULT CViewerWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // first-paint background: the shared class brush is white; the brush
        // must exist before ShowWindow so no white frame is ever visible
        Theme = EffectiveTheme();
        BgBrush = CreateSolidBrush(Theme->docBg);

        // app-theme snapshot: menus follow the theme active at creation
        // (reopen adopts, 036 convention)
        DarkMenus = SalamanderGeneral->IsDarkThemeActive() != FALSE;
        BuildMenu();
        ViewerWindowQueue.Add(new CWindowQueueItem(HWindow));

        if (!CMdWebHost::RuntimeAvailable())
        {
            // defer the fatal path until after OpenFile set Name (so OpenAsText works)
            PostMessage(HWindow, WM_APP + 1, 0, 0);
            break;
        }
        Web = new CMdWebHost();
        CMdWebHost::Callbacks cb;
        cb.OnReady = [this]()
        {
            if (RenderPending)
            {
                RenderPending = false;
                Web->SetDocument(&Html, DocDir);
                Web->SetZoomPercent(g_zoom);
                Web->Navigate();
            }
        };
        cb.OnActivateLink = [this](const std::wstring& uri)
        { ActivateLink(uri); };
        cb.OnInitFailed = [this]()
        { PostMessage(HWindow, WM_APP + 1, 0, 0); };
        cb.OnProcessFailed = [this]()
        { PostMessage(HWindow, WM_APP + 1, 0, 0); };
        cb.OnZoomChanged = [this](int pct)
        { g_zoom = pct; UpdateTitle(); };
        Web->Create(HWindow, MdUserDataFolder(), cb);
        Web->SetBackgroundColor(Theme->docBg); // applied when the controller is ready
        break;
    }

    case WM_APP + 1: // engine unavailable / failed -> fall back to text viewer
        EngineFailed();
        return 0;

    case WM_ERASEBKGND:
        if (BgBrush != NULL)
        {
            RECT r;
            GetClientRect(HWindow, &r);
            FillRect((HDC)wParam, &r, BgBrush);
            return 1;
        }
        break;

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
            // owner-drawn items lose automatic '&' mnemonic matching
            LRESULT r = DarkMenuHandleMenuChar((HMENU)lParam, wParam);
            if (HIWORD(r) != MNC_IGNORE)
                return r;
        }
        break;

    case WM_SIZE:
    {
        if (Web != NULL)
        {
            RECT r;
            GetClientRect(HWindow, &r);
            Web->Resize(r.right, r.bottom);
        }
        break;
    }

    case WM_SETFOCUS:
        // forward frame focus into the WebView2 content so keyboard scrolling
        // (arrows / PgUp / PgDn) works without a mouse click
        if (Web != NULL && Web->IsReady())
            Web->Focus();
        return 0;

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id >= CM_SCHEME_FIRST && id < CM_SCHEME_FIRST + MdThemeCount)
        {
            SelectScheme(id - CM_SCHEME_FIRST);
            return 0;
        }
        switch (id)
        {
        case CM_FILE_CLOSE:
            DestroyWindow(HWindow);
            return 0;
        case CM_FILE_OPENTEXT: // "View Source" toggle (Ctrl+U)
            SourceMode = !SourceMode;
            FindIndex = -1;
            RefreshSchemeChecks();
            Regenerate();
            return 0;
        case CM_EDIT_FIND:
            DoFind(TRUE, TRUE);
            return 0;
        case CM_EDIT_FINDNEXT:
            DoFind(TRUE, FALSE);
            return 0;
        case CM_EDIT_FINDPREV:
            DoFind(FALSE, FALSE);
            return 0;
        case CM_VIEW_FOLLOWSYS:
            g_followSys = !g_followSys;
            RefreshSchemeChecks();
            Regenerate();
            return 0;
        case CM_VIEW_REMOTEIMG:
            RemoteAllowed = !RemoteAllowed;
            RefreshSchemeChecks();
            Regenerate();
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
        case CM_SCHEME_NEXT:
            CycleScheme(1);
            return 0;
        case CM_SCHEME_PREV:
            CycleScheme(-1);
            return 0;
        case CM_HELP_ABOUT:
            OnAbout(HWindow);
            return 0;
        }
        break;
    }

    case WM_USER_VIEWERCFGCHNG:
        RefreshSchemeChecks();
        Regenerate();
        return 0;

    case WM_DESTROY:
    {
        if (g_savePos)
        {
            g_wndPlacement.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(HWindow, &g_wndPlacement);
        }
        if (DarkMenus)
            DarkMenuRelease(GetMenu(HWindow)); // free owner-draw paint data
        if (Web != NULL)
            Web->Destroy();
        ViewerWindowQueue.Remove(HWindow);
        PostQuitMessage(0);
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
