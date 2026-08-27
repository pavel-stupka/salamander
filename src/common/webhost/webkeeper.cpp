// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// webkeeper.cpp - session keeper for the shared WebView2 browser tree.
// See webkeeper.h; behaviour is unchanged from mdview's feature-065 keeper,
// only the state is per-instance so several plugins can each hold their own.

#include "precomp.h"

#pragma push_macro("new")
#undef new
#include <wrl.h>
#include "WebView2.h"
#include "WebView2EnvironmentOptions.h"
#pragma pop_macro("new")

#include "webhost.h"
#include "webkeeper.h"

using namespace Microsoft::WRL;

// The keeper must build its environment with the SAME options as every other
// consumer (contract S2.2). webhost.cpp owns that set; it is rebuilt here
// through the same code path by including the one definition below.
static ComPtr<CoreWebView2EnvironmentOptions> TcWebKeeperEnvOptions()
{
    auto options = Make<CoreWebView2EnvironmentOptions>();
    options->put_AdditionalBrowserArguments(
        L"--disable-background-networking --disable-sync --disable-component-update "
        L"--disable-features=msWebOOUI,msPdfOOUI");
    return options;
}

#define TC_KEEPER_DIED (WM_APP + 1)

struct CTcWebKeeperState
{
    enum EState
    {
        kUnarmed,
        kArming,
        kArmed
    };
    EState state = kUnarmed;
    int gen = 0; // bumped on every release; async callbacks from an older arm bail out
    HWND hwnd = NULL;
    bool comInit = false;
    bool classRegistered = false;
    TcWebKeeperConfig cfg;
    ComPtr<ICoreWebView2Environment> env;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    EventRegistrationToken procTok{}, exitTok{};
};

// The hidden window needs to reach its keeper from a static window procedure.
static LRESULT CALLBACK TcKeeperWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    if (msg == TC_KEEPER_DIED)
    {
        CTcWebKeeper* self = (CTcWebKeeper*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (self != NULL)
        {
            // The shared browser tree died under us: quiet teardown; the next
            // view arms a fresh keeper (065 R7 - never a dialog, never a loop).
            self->Disarm();
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

struct CTcWebKeeperAccess
{
    static CTcWebKeeperState* S(CTcWebKeeper* k)
    {
        if (k->State == nullptr)
            k->State = new CTcWebKeeperState();
        return (CTcWebKeeperState*)k->State;
    }
};

#define KST CTcWebKeeperAccess::S(this)

void CTcWebKeeper::ReleaseAll()
{
    CTcWebKeeperState* s = KST;
    if (s->webview && s->procTok.value != 0)
        s->webview->remove_ProcessFailed(s->procTok);
    s->procTok = {};
    if (s->env && s->exitTok.value != 0)
    {
        ComPtr<ICoreWebView2Environment5> env5;
        if (SUCCEEDED(s->env.As(&env5)) && env5)
            env5->remove_BrowserProcessExited(s->exitTok);
    }
    s->exitTok = {};
    if (s->controller)
    {
        s->controller->Close();
        s->controller.Reset();
    }
    s->webview.Reset();
    s->env.Reset();
    if (s->hwnd != NULL)
    {
        SetWindowLongPtrW(s->hwnd, GWLP_USERDATA, 0);
        DestroyWindow(s->hwnd);
        s->hwnd = NULL;
    }
    if (s->comInit)
    {
        CoUninitialize();
        s->comInit = false;
    }
    s->state = CTcWebKeeperState::kUnarmed;
    s->gen++;
}

void CTcWebKeeper::ControllerReady(void* controllerRaw)
{
    CTcWebKeeperState* s = KST;
    ICoreWebView2Controller* ctl = (ICoreWebView2Controller*)controllerRaw;
    s->controller = ctl;
    ctl->put_IsVisible(FALSE);
    if (FAILED(ctl->get_CoreWebView2(&s->webview)) || !s->webview)
    {
        ReleaseAll();
        return;
    }
    HWND hwnd = s->hwnd;
    s->webview->add_ProcessFailed(
        Callback<ICoreWebView2ProcessFailedEventHandler>(
            [hwnd](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs*) -> HRESULT
            {
                if (hwnd != NULL)
                    PostMessage(hwnd, TC_KEEPER_DIED, 0, 0);
                return S_OK;
            })
            .Get(),
        &s->procTok);
    ComPtr<ICoreWebView2Environment5> env5;
    if (SUCCEEDED(s->env.As(&env5)) && env5)
        env5->add_BrowserProcessExited(
            Callback<ICoreWebView2BrowserProcessExitedEventHandler>(
                [hwnd](ICoreWebView2Environment*, ICoreWebView2BrowserProcessExitedEventArgs*) -> HRESULT
                {
                    if (hwnd != NULL)
                        PostMessage(hwnd, TC_KEEPER_DIED, 0, 0);
                    return S_OK;
                })
                .Get(),
            &s->exitTok);

    // Shrink the idle footprint; both are best-effort (the installed Evergreen
    // runtime governs which interfaces exist at run time).
    ComPtr<ICoreWebView2_19> wv19;
    if (SUCCEEDED(s->webview.As(&wv19)) && wv19)
        wv19->put_MemoryUsageTargetLevel(COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_LOW);
    ComPtr<ICoreWebView2_3> wv3;
    if (SUCCEEDED(s->webview.As(&wv3)) && wv3)
        wv3->TrySuspend(Callback<ICoreWebView2TrySuspendCompletedHandler>(
                            [](HRESULT, BOOL) -> HRESULT
                            { return S_OK; })
                            .Get());

    s->state = CTcWebKeeperState::kArmed;
    TRACE_I(s->cfg.TraceName << ": armed (t=" << GetTickCount64() << " ms)");
}

void CTcWebKeeper::Arm(const TcWebKeeperConfig& config)
{
    CTcWebKeeperState* s = KST;
    if (s->state != CTcWebKeeperState::kUnarmed)
        return; // idempotent while Arming/Armed
    if (config.ClassName == nullptr || config.Instance == nullptr)
        return;
    s->cfg = config;

    if (!s->classRegistered)
    {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = TcKeeperWndProc;
        wc.hInstance = config.Instance;
        wc.lpszClassName = config.ClassName;
        if (RegisterClassW(&wc) == 0)
            return; // silent; the next view tries again
        s->classRegistered = true;
    }
    s->hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, config.ClassName, L"", WS_POPUP,
                              0, 0, 1, 1, NULL, NULL, config.Instance, this);
    if (s->hwnd == NULL)
        return; // silent

    HRESULT hrco = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hrco))
        s->comInit = true; // S_OK or S_FALSE -> balanced in ReleaseAll

    s->state = CTcWebKeeperState::kArming;
    const int gen = s->gen;
    CTcWebKeeper* self = this;
    TRACE_I(s->cfg.TraceName << ": arming (t=" << GetTickCount64() << " ms)");

    std::wstring udf = TcWebUserDataFolder();
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        NULL, udf.empty() ? NULL : udf.c_str(), TcWebKeeperEnvOptions().Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [self, gen](HRESULT r, ICoreWebView2Environment* env) -> HRESULT
            {
                CTcWebKeeperState* st = CTcWebKeeperAccess::S(self);
                if (gen != st->gen)
                    return S_OK; // disarmed while the creation was in flight
                if (FAILED(r) || env == NULL)
                {
                    self->Disarm();
                    return S_OK;
                }
                st->env = env;
                HRESULT r2 = env->CreateCoreWebView2Controller(
                    st->hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [self, gen](HRESULT rc, ICoreWebView2Controller* ctl) -> HRESULT
                        {
                            CTcWebKeeperState* st2 = CTcWebKeeperAccess::S(self);
                            if (gen != st2->gen)
                            {
                                if (ctl != NULL)
                                    ctl->Close();
                                return S_OK;
                            }
                            if (FAILED(rc) || ctl == NULL)
                            {
                                self->Disarm();
                                return S_OK;
                            }
                            self->ControllerReady(ctl);
                            return S_OK;
                        })
                        .Get());
                if (FAILED(r2))
                    self->Disarm();
                return S_OK;
            })
            .Get());
    if (FAILED(hr))
        Disarm(); // silent; the next view may try again
}

// Feature 069 (F-P6-01): the keeper window class was registered with the
// plugin's own module handle and never unregistered, so it outlived a Plugins
// Manager Unload. After a reload the DLL usually lands on the same base, the
// atom is still there, RegisterClassW fails with ERROR_CLASS_ALREADY_EXISTS
// and arming silently stopped working for the rest of the session. (Accepting
// ERROR_CLASS_ALREADY_EXISTS instead would create a window on a window
// procedure in unmapped memory: a crash.)
void CTcWebKeeper::UnregisterClassIfRegistered()
{
    CTcWebKeeperState* s = KST;
    if (!s->classRegistered || s->cfg.ClassName == nullptr)
        return;
    if (UnregisterClassW(s->cfg.ClassName, s->cfg.Instance))
        s->classRegistered = false;
    else // a window of the class still exists - keep the flag so we do not
        TRACE_E(s->cfg.TraceName << ": UnregisterClassW failed"); // register twice
}

void CTcWebKeeper::Disarm()
{
    CTcWebKeeperState* s = KST;
    if (s->state != CTcWebKeeperState::kUnarmed)
    {
        TRACE_I(s->cfg.TraceName << ": disarmed");
        ReleaseAll();
    }
    // The class is released here and not in ReleaseAll: that also runs when the
    // shared browser process dies mid-session, and there the class must stay so
    // the next view can re-arm without re-registering. This is the unload path
    // and must run even when the keeper is already unarmed.
    UnregisterClassIfRegistered();
}

bool CTcWebKeeper::Armed() const
{
    CTcWebKeeperState* s = (CTcWebKeeperState*)State;
    return s != nullptr && s->state != CTcWebKeeperState::kUnarmed;
}
