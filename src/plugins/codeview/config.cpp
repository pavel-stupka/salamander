// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// config.cpp - persisted settings and the configuration dialog
// (spec FR-038, data-model.md S6).

#include "precomp.h"
#include "langmap.h"
#include "schemes.h"
#include "webkeeper.h"

extern CTcWebKeeper CvKeeper;

// --- configuration (defaults are the shipped behaviour) ---
char g_scheme[32] = "github-dark";
int g_followApp = 1; // follow the application theme by default (FR-013)
char g_schemeLight[32] = "github-light";
char g_schemeDark[32] = "github-dark";
// Consolas, not Cascadia Mono: it is hand-hinted for ClearType at the sizes a
// code viewer uses, and it is what mdview's code blocks pick first. See the
// note in web/viewer.css.
char g_fontFamily[64] = "Consolas";
int g_fontSize = 0; // 0 = the page's own default
int g_tabWidth = 4;
int g_highlightLimitKB = 1024;
int g_viewerLimitMB = 20;
int g_maxLineLength = 20000;
BOOL g_lineNumbers = TRUE;
BOOL g_wrap = FALSE;
BOOL g_whitespace = FALSE;
int g_zoom = 100;
BOOL g_savePos = TRUE;
WINDOWPLACEMENT g_wndPlacement = {0};
BOOL g_keepReady = TRUE;
BOOL g_hintShown = FALSE;
BOOL g_restoreTypes = FALSE; // pending "restore default file types" (applied by Connect)

// 2: the default font family became Consolas (sharper than Cascadia Mono at
//    these sizes); a config still carrying the old DEFAULT is migrated, a
//    family the user actually chose is left alone.
#define CURRENT_CONFIG_VERSION 2
static const char* CONFIG_VERSION = "Version";
static const char* CONFIG_SCHEME = "ColorScheme";
static const char* CONFIG_FOLLOWAPP = "FollowAppTheme";
static const char* CONFIG_SCHEMELIGHT = "SchemeLight";
static const char* CONFIG_SCHEMEDARK = "SchemeDark";
static const char* CONFIG_FONTFAMILY = "FontFamily";
static const char* CONFIG_FONTSIZE = "FontSize";
static const char* CONFIG_TABWIDTH = "TabWidth";
static const char* CONFIG_HLLIMIT = "HighlightLimitKB";
static const char* CONFIG_VIEWLIMIT = "ViewerLimitMB";
static const char* CONFIG_MAXLINE = "MaxLineLength";
static const char* CONFIG_LINENUMBERS = "LineNumbers";
static const char* CONFIG_WRAP = "Wrap";
static const char* CONFIG_WHITESPACE = "ShowWhitespace";
static const char* CONFIG_ZOOM = "ZoomPercent";
static const char* CONFIG_SAVEPOS = "SavePosition";
static const char* CONFIG_WNDPLACEMENT = "WindowPlacement";
static const char* CONFIG_KEEPREADY = "KeepReady";
static const char* CONFIG_HINTSHOWN = "FirstRunHintShown";
static const char* CONFIG_RESTORETYPES = "RestoreDefaultTypes";

static void ClampInt(int* v, int lo, int hi, int def)
{
    if (*v < lo || *v > hi)
        *v = def;
}

// Dialog-side clamp: keeps what the user typed by moving it to the nearest
// legal value instead of resetting it to the factory default. 'zeroAllowed'
// carries the "0 = page default" convention of the font size.
static void ClampNear(int* v, int lo, int hi, BOOL zeroAllowed)
{
    if (zeroAllowed && *v <= 0)
    {
        *v = 0;
        return;
    }
    if (*v < lo)
        *v = lo;
    else if (*v > hi)
        *v = hi;
}

static void ClampSchemeSlot(char* s, BOOL wantDark, const char* def)
{
    int i = CvFindScheme(s);
    if (i < 0 || (CvSchemes[i].Dark != FALSE) != (wantDark != FALSE))
        lstrcpynA(s, def, 32);
}

void WINAPI CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    if (regKey != NULL)
    {
        DWORD ver = CURRENT_CONFIG_VERSION;
        registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &ver, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_SCHEME, REG_SZ, g_scheme, sizeof(g_scheme));
        registry->GetValue(regKey, CONFIG_FOLLOWAPP, REG_DWORD, &g_followApp, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_SCHEMELIGHT, REG_SZ, g_schemeLight, sizeof(g_schemeLight));
        registry->GetValue(regKey, CONFIG_SCHEMEDARK, REG_SZ, g_schemeDark, sizeof(g_schemeDark));
        registry->GetValue(regKey, CONFIG_FONTFAMILY, REG_SZ, g_fontFamily, sizeof(g_fontFamily));
        registry->GetValue(regKey, CONFIG_FONTSIZE, REG_DWORD, &g_fontSize, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_TABWIDTH, REG_DWORD, &g_tabWidth, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_HLLIMIT, REG_DWORD, &g_highlightLimitKB, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_VIEWLIMIT, REG_DWORD, &g_viewerLimitMB, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_MAXLINE, REG_DWORD, &g_maxLineLength, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_LINENUMBERS, REG_DWORD, &g_lineNumbers, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_WRAP, REG_DWORD, &g_wrap, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_WHITESPACE, REG_DWORD, &g_whitespace, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_ZOOM, REG_DWORD, &g_zoom, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_SAVEPOS, REG_DWORD, &g_savePos, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_WNDPLACEMENT, REG_BINARY, &g_wndPlacement, sizeof(WINDOWPLACEMENT));
        registry->GetValue(regKey, CONFIG_KEEPREADY, REG_DWORD, &g_keepReady, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_HINTSHOWN, REG_DWORD, &g_hintShown, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_RESTORETYPES, REG_DWORD, &g_restoreTypes, sizeof(DWORD));
        // Version 1 shipped "Cascadia Mono" as the default. Only a stored
        // value that is still exactly that default is moved to the new one --
        // a family the user typed themselves is theirs to keep.
        if (ver < 2 && strcmp(g_fontFamily, "Cascadia Mono") == 0)
            lstrcpynA(g_fontFamily, "Consolas", 64);
    }
    // Corruption tolerance: every value is clamped to something usable, so a
    // hand-edited or partially written key can never make the viewer unusable.
    if (CvFindScheme(g_scheme) < 0)
        lstrcpynA(g_scheme, "github-dark", 32);
    ClampSchemeSlot(g_schemeLight, FALSE, "github-light");
    ClampSchemeSlot(g_schemeDark, TRUE, "github-dark");
    if (g_fontFamily[0] == 0)
        lstrcpynA(g_fontFamily, "Consolas", 64);
    ClampInt(&g_fontSize, 0, 72, 0);
    if (g_fontSize != 0 && g_fontSize < 6)
        g_fontSize = 6;
    ClampInt(&g_tabWidth, 1, 16, 4);
    ClampInt(&g_highlightLimitKB, 64, 20480, 1024);
    ClampInt(&g_viewerLimitMB, 1, 256, 20);
    ClampInt(&g_maxLineLength, 1000, 100000, 20000);
    ClampInt(&g_zoom, 50, 300, 100);
    // The placement is a raw REG_BINARY blob; a truncated or hand-edited one
    // would be used verbatim (Body only tests length != 0).
    if (g_wndPlacement.length != sizeof(WINDOWPLACEMENT))
        ZeroMemory(&g_wndPlacement, sizeof(g_wndPlacement));
    g_keepReady = g_keepReady ? TRUE : FALSE;
    g_lineNumbers = g_lineNumbers ? TRUE : FALSE;
    g_wrap = g_wrap ? TRUE : FALSE;
    g_whitespace = g_whitespace ? TRUE : FALSE;
}

void WINAPI CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    DWORD v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_SCHEME, REG_SZ, g_scheme, -1);
    registry->SetValue(regKey, CONFIG_FOLLOWAPP, REG_DWORD, &g_followApp, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_SCHEMELIGHT, REG_SZ, g_schemeLight, -1);
    registry->SetValue(regKey, CONFIG_SCHEMEDARK, REG_SZ, g_schemeDark, -1);
    registry->SetValue(regKey, CONFIG_FONTFAMILY, REG_SZ, g_fontFamily, -1);
    registry->SetValue(regKey, CONFIG_FONTSIZE, REG_DWORD, &g_fontSize, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_TABWIDTH, REG_DWORD, &g_tabWidth, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_HLLIMIT, REG_DWORD, &g_highlightLimitKB, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_VIEWLIMIT, REG_DWORD, &g_viewerLimitMB, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_MAXLINE, REG_DWORD, &g_maxLineLength, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_LINENUMBERS, REG_DWORD, &g_lineNumbers, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_WRAP, REG_DWORD, &g_wrap, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_WHITESPACE, REG_DWORD, &g_whitespace, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_ZOOM, REG_DWORD, &g_zoom, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_SAVEPOS, REG_DWORD, &g_savePos, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_WNDPLACEMENT, REG_BINARY, &g_wndPlacement, sizeof(WINDOWPLACEMENT));
    registry->SetValue(regKey, CONFIG_KEEPREADY, REG_DWORD, &g_keepReady, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_HINTSHOWN, REG_DWORD, &g_hintShown, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_RESTORETYPES, REG_DWORD, &g_restoreTypes, sizeof(DWORD));
}

static void SetDlgInt(HWND hDlg, int id, int v)
{
    char b[24];
    _itoa_s(v, b, 10);
    SetDlgItemTextA(hDlg, id, b);
}

static int GetDlgInt(HWND hDlg, int id, int def)
{
    char b[24] = {0};
    if (GetDlgItemTextA(hDlg, id, b, sizeof(b)) == 0)
        return def;
    return atoi(b);
}

static INT_PTR CALLBACK CfgDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // "Restore Default File Types" only takes effect when the dialog is
    // confirmed; Cancel must leave every global exactly as it found it.
    static BOOL pendingRestoreTypes = FALSE;

    // feature 049: raw dialog proc - the two-touchpoint theme pattern
    if (msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC)
    {
        INT_PTR brush;
        if (SalamanderGeneral->ThemeHandleCtlColor(msg, wParam, lParam, &brush))
            return brush;
    }

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        pendingRestoreTypes = FALSE;
        // Wide in and out: g_fontFamily is UTF-8 (it is shipped to the page as
        // UTF-8), so the ANSI dialog calls turned every non-ASCII family name
        // into mojibake on the way in and into U+FFFD on the way out.
        wchar_t* wf = SplU8ToWAlloc(g_fontFamily);
        SetDlgItemTextW(hDlg, IDC_CFG_FONTNAME, wf != NULL ? wf : L"");
        if (wf != NULL)
            free(wf);
    }
        SetDlgInt(hDlg, IDC_CFG_FONTSIZE, g_fontSize);
        SetDlgInt(hDlg, IDC_CFG_TABWIDTH, g_tabWidth);
        SetDlgInt(hDlg, IDC_CFG_HLLIMIT, g_highlightLimitKB);
        SetDlgInt(hDlg, IDC_CFG_VIEWLIMIT, g_viewerLimitMB);
        SetDlgInt(hDlg, IDC_CFG_MAXLINE, g_maxLineLength);
        CheckDlgButton(hDlg, IDC_CFG_KEEPREADY, g_keepReady ? BST_CHECKED : BST_UNCHECKED);
        SalamanderGeneral->ThemeApplyToDialog(hDlg);
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_CFG_RESTORETYPES)
        {
            // "Restore default file types" re-issues only this plugin's rows
            // and touches nothing else (spec FR-012).
            //
            // The viewer list can only be written through
            // CSalamanderConnectAbstract, and the plugin interface hands that
            // out ONLY inside Connect() (spl_base.h:638) -- there is no way to
            // reach it from a dialog. So the request is recorded here and
            // Connect() performs it at the next load, which is also when the
            // core is prepared for the list to change. The user is told that.
            // Pending until OK: the button used to commit the global straight
            // away, so pressing Cancel afterwards still re-added every default
            // mask at the next start -- a change the user had just refused.
            pendingRestoreTypes = TRUE;
            SalamanderGeneral->SalMessageBox(hDlg, LoadStr(IDS_RESTORE_TYPES_DONE),
                                             LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONINFORMATION);
            return TRUE;
        }
        if (LOWORD(wParam) == IDOK)
        {
            wchar_t wfont[64] = {0};
            GetDlgItemTextW(hDlg, IDC_CFG_FONTNAME, wfont, 64);
            if (wfont[0] != 0)
            {
                char* u8 = SplWToU8Alloc(wfont);
                if (u8 != NULL)
                {
                    // A wide name can expand to three UTF-8 bytes per unit, so
                    // it may not fit -- and a plain truncation could cut a
                    // character in half, which no consumer of this UTF-8 value
                    // would accept. Drop the torn tail instead.
                    lstrcpynA(g_fontFamily, u8, sizeof(g_fontFamily));
                    size_t n = strlen(g_fontFamily);
                    while (n > 0 && ((BYTE)g_fontFamily[n - 1] & 0xC0) == 0x80)
                        n--; // step back over continuation bytes
                    if (n > 0)
                    {
                        BYTE lead = (BYTE)g_fontFamily[n - 1];
                        size_t seq = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2 : (lead & 0xF0) == 0xE0 ? 3 : 4;
                        if (n - 1 + seq > strlen(g_fontFamily))
                            g_fontFamily[n - 1] = 0; // the last character was cut
                    }
                    free(u8); // splunicode.h: the helpers allocate with malloc
                }
            }
            g_fontSize = GetDlgInt(hDlg, IDC_CFG_FONTSIZE, g_fontSize);
            g_tabWidth = GetDlgInt(hDlg, IDC_CFG_TABWIDTH, g_tabWidth);
            g_highlightLimitKB = GetDlgInt(hDlg, IDC_CFG_HLLIMIT, g_highlightLimitKB);
            g_viewerLimitMB = GetDlgInt(hDlg, IDC_CFG_VIEWLIMIT, g_viewerLimitMB);
            g_maxLineLength = GetDlgInt(hDlg, IDC_CFG_MAXLINE, g_maxLineLength);
            // Clamp to the NEAREST legal value, and to the same bounds the
            // loader enforces. Resetting to the factory default silently threw
            // away what the user typed, and the loader's extra "size >= 6"
            // rule meant a saved 3 came back as 6 at the next start.
            ClampNear(&g_fontSize, 6, 72, TRUE);
            ClampNear(&g_tabWidth, 1, 16, FALSE);
            ClampNear(&g_highlightLimitKB, 64, 20480, FALSE);
            ClampNear(&g_viewerLimitMB, 1, 256, FALSE);
            ClampNear(&g_maxLineLength, 1000, 100000, FALSE);

            if (pendingRestoreTypes)
                g_restoreTypes = TRUE;
            BOOL keep = IsDlgButtonChecked(hDlg, IDC_CFG_KEEPREADY) == BST_CHECKED;
            if (g_keepReady && !keep)
                CvKeeper.Disarm(); // turning off releases the kept-ready engine now
            g_keepReady = keep;    // turning on applies from the next view

            // Open windows pick up the view settings without a restart.
            ViewerWindowQueue.BroadcastMessage(WM_USER_VIEWERCFGCHNG, 0, 0);
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
    DialogBoxW(HLanguage, MAKEINTRESOURCEW(IDD_CFG), parent, CfgDlgProc);
}
