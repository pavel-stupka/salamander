// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// webhost.cpp - the product's shared WebView2 host. See webhost.h for why this
// lives in src/common and what a plugin may and may not configure.
//
// Every setting applied in ApplyControllerReady() below is an INVARIANT of the
// product's rendering lockdown except the two flags TcWebHostConfig exposes.
// Adding a relaxation here relaxes it for every WebView2 consumer at once --
// which is exactly why it is centralised.

#include "precomp.h"

// WRL's implements.h (pulled in by <wrl.h> and WebView2EnvironmentOptions.h) is
// incompatible with the debug leak-tracking "new" macro. Suspend it across
// these headers, then restore it (precedent: mdview's webview.cpp).
#pragma push_macro("new")
#undef new
#include <wrl.h>
#include <shlwapi.h>
#include "WebView2.h"
#include "WebView2EnvironmentOptions.h"
#pragma pop_macro("new")

#include "webhost.h"

using namespace Microsoft::WRL;

// ==========================================================================
// the one browser-arguments set (contract S2.2)
// ==========================================================================
//
// AdditionalBrowserArguments apply only when the shared browser process
// STARTS; arguments passed by environments created afterwards are silently
// ignored. Extending this set is a coordinated change here, never a per-plugin
// override -- an override would take effect or not depending on which plugin
// happened to start the tree first.
static ComPtr<CoreWebView2EnvironmentOptions> TcWebBuildEnvOptions()
{
    auto options = Make<CoreWebView2EnvironmentOptions>();
    options->put_AdditionalBrowserArguments(
        L"--disable-background-networking --disable-sync --disable-component-update "
        L"--disable-features=msWebOOUI,msPdfOOUI");
    return options;
}

std::wstring TcWebUserDataFolder()
{
    wchar_t path[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path)))
    {
        // App-neutral, NOT per-plugin: the warm browser tree is shared by user
        // data folder, so every consumer must name the same one.
        std::wstring p = path;
        p += L"\\Tandem Commander\\WebView2";
        return p;
    }
    return std::wstring();
}

// ==========================================================================
// Impl
// ==========================================================================

struct CTcWebHostImpl
{
    HWND parent = NULL;
    CTcWebHost::Callbacks cb;
    TcWebHostConfig cfg;
    ComPtr<ICoreWebView2Environment> env;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    EventRegistrationToken navTok{}, newWinTok{}, resTok{}, procTok{}, accelTok{}, zoomTok{},
        navDoneTok{}, msgTok{}, dlTok{}, permTok{};
    bool ready = false;
    bool comInit = false;
    COREWEBVIEW2_COLOR bgColor{};
    bool bgColorSet = false;
    std::wstring userDataFolder;
    std::wstring baseUrl;      // https://<host>/<document>
    std::wstring originPrefix; // https://<host>/
    int pendingZoom = 100;
};

static std::wstring PathOnly(const std::wstring& u)
{
    size_t q = u.find_first_of(L"?#");
    return q == std::wstring::npos ? u : u.substr(0, q);
}

static void MakeAndSetResponse(CTcWebHostImpl* impl, ICoreWebView2WebResourceRequestedEventArgs* args,
                               const BYTE* data, size_t n, int status, const wchar_t* reason,
                               const std::wstring& headers)
{
    IStream* stream = NULL;
    if (data != NULL && n > 0)
        stream = SHCreateMemStream(data, (UINT)n);
    ComPtr<ICoreWebView2WebResourceResponse> resp;
    if (SUCCEEDED(impl->env->CreateWebResourceResponse(stream, status, reason, headers.c_str(), &resp)) && resp)
        args->put_Response(resp.Get());
    if (stream)
        stream->Release();
}

// The Content-Security-Policy is applied by the HOST, not by a <meta> tag in
// the page: a header is enforced before parsing and cannot be displaced by
// document content. 'default-src none' plus an explicit allow-list means a
// bug in the page cannot open a channel the interceptor does not see.
// ('wasm-unsafe-eval' is needed by the Oniguruma tokenizer, which instantiates
//  an inlined WASM binary -- specs/070-.../spike-results.md S2.)
static const wchar_t* kCspScripted =
    L"Content-Security-Policy: default-src 'none'; script-src 'self' 'wasm-unsafe-eval'; "
    L"style-src 'self'; connect-src 'self'; img-src 'none'; object-src 'none'; "
    L"worker-src 'self'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'";
static const wchar_t* kCspStatic =
    L"Content-Security-Policy: default-src 'none'; style-src 'self' 'unsafe-inline'; "
    L"img-src 'self' data:; object-src 'none'; base-uri 'none'; form-action 'none'; "
    L"frame-ancestors 'none'";

static void ServeRequest(CTcWebHostImpl* impl, ICoreWebView2WebResourceRequestedEventArgs* args)
{
    ComPtr<ICoreWebView2WebResourceRequest> req;
    if (FAILED(args->get_Request(&req)) || !req)
        return;
    LPWSTR uriRaw = NULL;
    req->get_Uri(&uriRaw);
    std::wstring u = uriRaw ? uriRaw : L"";
    if (uriRaw)
        CoTaskMemFree(uriRaw);

    std::wstring path = PathOnly(u);
    bool ours = path.compare(0, impl->originPrefix.size(), impl->originPrefix) == 0;
    if (ours && impl->cfg.Serve)
    {
        std::wstring rel = path.substr(impl->originPrefix.size());
        TcWebResponse r;
        if (impl->cfg.Serve(rel, r))
        {
            std::wstring headers = L"Content-Type: " + r.ContentType;
            if (rel == impl->cfg.DocumentPath)
            {
                headers += L"\r\n";
                headers += impl->cfg.ScriptsEnabled ? kCspScripted : kCspStatic;
            }
            if (!r.ExtraHeaders.empty())
            {
                headers += L"\r\n";
                headers += r.ExtraHeaders;
            }
            MakeAndSetResponse(impl, args, r.Data, r.Size, r.Status, r.Reason, headers);
            return;
        }
    }
    // Default-deny. This is an invariant: a plugin can only ADD answers, never
    // widen what happens to everything else.
    MakeAndSetResponse(impl, args, NULL, 0, 403, L"Forbidden", L"");
}

static void ApplyBackgroundColor(CTcWebHostImpl* impl)
{
    if (!impl->bgColorSet || !impl->controller)
        return;
    ComPtr<ICoreWebView2Controller2> ctl2;
    if (SUCCEEDED(impl->controller.As(&ctl2)) && ctl2)
        ctl2->put_DefaultBackgroundColor(impl->bgColor);
}

// Reads every locked-down setting back and complains loudly in a debug build.
// A single regressed setting is otherwise invisible until it matters
// (contracts/rendering-lockdown.md S5 item 2).
static void AssertLockdown(CTcWebHostImpl* impl, ICoreWebView2Settings* st)
{
#ifdef _DEBUG
    BOOL b = TRUE;
    auto check = [&](const char* what, HRESULT hr, BOOL value, BOOL expected)
    {
        if (SUCCEEDED(hr) && value != expected)
            TRACE_E(impl->cfg.TraceName << ": lockdown regression, " << what << " is not "
                                        << (expected ? "TRUE" : "FALSE"));
    };
    check("IsScriptEnabled", st->get_IsScriptEnabled(&b), b, impl->cfg.ScriptsEnabled ? TRUE : FALSE);
    check("IsWebMessageEnabled", st->get_IsWebMessageEnabled(&b), b, impl->cfg.WebMessagesEnabled ? TRUE : FALSE);
    check("AreDefaultContextMenusEnabled", st->get_AreDefaultContextMenusEnabled(&b), b, FALSE);
    check("AreDevToolsEnabled", st->get_AreDevToolsEnabled(&b), b, FALSE);
    check("IsStatusBarEnabled", st->get_IsStatusBarEnabled(&b), b, FALSE);
    check("IsBuiltInErrorPageEnabled", st->get_IsBuiltInErrorPageEnabled(&b), b, FALSE);
    check("AreHostObjectsAllowed", st->get_AreHostObjectsAllowed(&b), b, FALSE);
    check("AreDefaultScriptDialogsEnabled", st->get_AreDefaultScriptDialogsEnabled(&b), b, FALSE);
#else
    (void)impl;
    (void)st;
#endif
}

static void ApplyControllerReady(CTcWebHostImpl* impl, ICoreWebView2Controller* ctl)
{
    impl->controller = ctl;
    if (FAILED(ctl->get_CoreWebView2(&impl->webview)) || !impl->webview)
    {
        if (impl->cb.OnInitFailed)
            impl->cb.OnInitFailed();
        return;
    }
    ICoreWebView2* wv = impl->webview.Get();

    // --- settings lockdown ---
    ComPtr<ICoreWebView2Settings> st;
    if (SUCCEEDED(wv->get_Settings(&st)) && st)
    {
        st->put_IsScriptEnabled(impl->cfg.ScriptsEnabled ? TRUE : FALSE);
        st->put_IsWebMessageEnabled(impl->cfg.WebMessagesEnabled ? TRUE : FALSE);
        st->put_AreDefaultContextMenusEnabled(FALSE);
        st->put_AreDevToolsEnabled(FALSE);
        st->put_IsStatusBarEnabled(FALSE);
        st->put_IsBuiltInErrorPageEnabled(FALSE);
        st->put_AreDefaultScriptDialogsEnabled(FALSE); // no alert()/confirm() from content
        st->put_IsZoomControlEnabled(TRUE);            // engine owns Ctrl+wheel; synced back
        st->put_AreHostObjectsAllowed(FALSE);
        ComPtr<ICoreWebView2Settings3> st3;
        if (SUCCEEDED(st.As(&st3)) && st3)
            st3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
        ComPtr<ICoreWebView2Settings4> st4;
        if (SUCCEEDED(st.As(&st4)) && st4)
        {
            st4->put_IsGeneralAutofillEnabled(FALSE);
            st4->put_IsPasswordAutosaveEnabled(FALSE);
        }
        ComPtr<ICoreWebView2Settings5> st5;
        if (SUCCEEDED(st.As(&st5)) && st5)
            st5->put_IsPinchZoomEnabled(FALSE);
        ComPtr<ICoreWebView2Settings6> st6;
        if (SUCCEEDED(st.As(&st6)) && st6)
            st6->put_IsSwipeNavigationEnabled(FALSE);
        ComPtr<ICoreWebView2Settings8> st8;
        if (SUCCEEDED(st.As(&st8)) && st8)
            st8->put_IsReputationCheckingRequired(FALSE);
        AssertLockdown(impl, st.Get());
    }

    // --- navigation gate (cancel everything but our own document) ---
    wv->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [impl](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT
            {
                LPWSTR uri = NULL;
                args->get_Uri(&uri);
                std::wstring u = uri ? uri : L"";
                if (uri)
                    CoTaskMemFree(uri);
                if (u.rfind(impl->baseUrl, 0) == 0)
                    return S_OK; // our document + #fragments
                args->put_Cancel(TRUE);
                if (impl->cb.OnActivateLink)
                    impl->cb.OnActivateLink(u);
                return S_OK;
            })
            .Get(),
        &impl->navTok);

    wv->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [impl](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT
            {
                args->put_Handled(TRUE);
                LPWSTR uri = NULL;
                args->get_Uri(&uri);
                std::wstring u = uri ? uri : L"";
                if (uri)
                    CoTaskMemFree(uri);
                if (impl->cb.OnActivateLink)
                    impl->cb.OnActivateLink(u);
                return S_OK;
            })
            .Get(),
        &impl->newWinTok);

    // --- offline content serving + default-deny net ---
    wv->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [impl](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT
            {
                ServeRequest(impl, args);
                return S_OK;
            })
            .Get(),
        &impl->resTok);
    // The deprecated filter raises WebResourceRequested only for requests
    // issued by DOCUMENTS. A module worker's imports (codeview's tokenizer:
    // shiki/engine.js, grammar and theme .mjs) come from the WORKER source
    // kind and would bypass the interceptor entirely -- they'd hit the real
    // network as https://<host>.invalid/..., fail, and kill the worker. Ask
    // for all source kinds where the runtime can (111+); fall back for older.
    {
        ComPtr<ICoreWebView2_22> wv22;
        HRESULT hrFilter = E_NOINTERFACE;
        if (SUCCEEDED(wv->QueryInterface(IID_PPV_ARGS(&wv22))) && wv22)
            hrFilter = wv22->AddWebResourceRequestedFilterWithRequestSourceKinds(
                L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL,
                COREWEBVIEW2_WEB_RESOURCE_REQUEST_SOURCE_KINDS_ALL);
        if (FAILED(hrFilter))
        {
            TRACE_I(impl->cfg.TraceName << ": worker-request interception unavailable, document-only filter");
            wv->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
        }
    }

    // --- newer-interface lockdowns: cancel anything that would take the user
    //     out of the viewer. Each is a QueryInterface, silently skipped on an
    //     older runtime (contracts/webview-host-sharing.md S2). ---
    ComPtr<ICoreWebView2_4> wv4;
    if (SUCCEEDED(wv->QueryInterface(IID_PPV_ARGS(&wv4))) && wv4)
    {
        wv4->add_DownloadStarting(
            Callback<ICoreWebView2DownloadStartingEventHandler>(
                [](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT
                {
                    args->put_Cancel(TRUE);
                    args->put_Handled(TRUE);
                    return S_OK;
                })
                .Get(),
            &impl->dlTok);
    }
    wv->add_PermissionRequested(
        Callback<ICoreWebView2PermissionRequestedEventHandler>(
            [](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT
            {
                // Deny is the whole answer here: this args type has put_State
                // but no put_Handled (unlike the download/new-window ones).
                args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY);
                return S_OK;
            })
            .Get(),
        &impl->permTok);

    wv->add_ProcessFailed(
        Callback<ICoreWebView2ProcessFailedEventHandler>(
            [impl](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs*) -> HRESULT
            {
                if (impl->cb.OnProcessFailed)
                    impl->cb.OnProcessFailed();
                return S_OK;
            })
            .Get(),
        &impl->procTok);

    // --- focus the content once each navigation completes (arrows/PgUp work
    //     immediately after F3, without a mouse click) ---
    wv->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [impl](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT
            {
                TRACE_I(impl->cfg.TraceName << ": navigation completed (t=" << GetTickCount64() << " ms)");
                if (impl->controller)
                    impl->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
                return S_OK;
            })
            .Get(),
        &impl->navDoneTok);

    if (impl->cfg.WebMessagesEnabled)
    {
        wv->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [impl](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
                {
                    // Only messages from our own origin are even considered; the
                    // plugin still validates the payload (schema, bounds).
                    LPWSTR src = NULL;
                    args->get_Source(&src);
                    std::wstring s = src ? src : L"";
                    if (src)
                        CoTaskMemFree(src);
                    if (s.rfind(impl->originPrefix, 0) != 0)
                        return S_OK;
                    LPWSTR json = NULL;
                    if (SUCCEEDED(args->get_WebMessageAsJson(&json)) && json)
                    {
                        if (impl->cb.OnWebMessage)
                            impl->cb.OnWebMessage(json);
                        CoTaskMemFree(json);
                    }
                    return S_OK;
                })
                .Get(),
            &impl->msgTok);
    }

    // --- keep persisted zoom in sync with engine-driven zoom ---
    ctl->add_ZoomFactorChanged(
        Callback<ICoreWebView2ZoomFactorChangedEventHandler>(
            [impl](ICoreWebView2Controller* sender, IUnknown*) -> HRESULT
            {
                double f = 1.0;
                if (SUCCEEDED(sender->get_ZoomFactor(&f)) && impl->cb.OnZoomChanged)
                    impl->cb.OnZoomChanged((int)(f * 100.0 + 0.5));
                return S_OK;
            })
            .Get(),
        &impl->zoomTok);

    // --- accelerator routing (focus lives inside the WebView2 HWND) ---
    ctl->add_AcceleratorKeyPressed(
        Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
            [impl](ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs* args) -> HRESULT
            {
                COREWEBVIEW2_KEY_EVENT_KIND kind;
                if (FAILED(args->get_KeyEventKind(&kind)))
                    return S_OK;
                if (kind != COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN &&
                    kind != COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN)
                    return S_OK;
                UINT vk = 0;
                args->get_VirtualKey(&vk);
                bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                int cmd = impl->cfg.Accelerator ? impl->cfg.Accelerator(vk, ctrl, shift) : 0;
                if (cmd)
                {
                    args->put_Handled(TRUE);
                    PostMessage(impl->parent, WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
                }
                return S_OK;
            })
            .Get(),
        &impl->accelTok);

    RECT rc;
    GetClientRect(impl->parent, &rc);
    ctl->put_Bounds(rc);
    ctl->put_ZoomFactor(impl->pendingZoom / 100.0);
    ApplyBackgroundColor(impl); // must precede visibility: no white blip
    ctl->put_IsVisible(TRUE);
    ctl->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);

    impl->ready = true;
    TRACE_I(impl->cfg.TraceName << ": controller ready (t=" << GetTickCount64() << " ms)");
    if (impl->cb.OnReady)
        impl->cb.OnReady();
}

// ==========================================================================
// CTcWebHost
// ==========================================================================

CTcWebHost::CTcWebHost() : p(new CTcWebHostImpl()) {}
CTcWebHost::~CTcWebHost()
{
    Destroy();
    delete p;
}

bool CTcWebHost::RuntimeAvailable()
{
    LPWSTR v = NULL;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(NULL, &v);
    bool ok = SUCCEEDED(hr) && v != NULL && v[0] != 0;
    if (v)
        CoTaskMemFree(v);
    return ok;
}

bool CTcWebHost::Create(HWND parent, const std::wstring& userDataFolder,
                        const TcWebHostConfig& config, const Callbacks& cb)
{
    p->parent = parent;
    p->cb = cb;
    p->cfg = config;
    p->userDataFolder = userDataFolder;
    p->originPrefix = L"https://" + config.VirtualHost + L"/";
    p->baseUrl = p->originPrefix + config.DocumentPath;

    HRESULT hrco = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hrco))
        p->comInit = true; // S_OK or S_FALSE -> balance in Destroy

    auto options = TcWebBuildEnvOptions();

    CTcWebHostImpl* impl = p;
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        NULL, userDataFolder.empty() ? NULL : userDataFolder.c_str(), options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [impl](HRESULT r, ICoreWebView2Environment* env) -> HRESULT
            {
                if (FAILED(r) || !env)
                {
                    if (impl->cb.OnInitFailed)
                        impl->cb.OnInitFailed();
                    return S_OK;
                }
                impl->env = env;
                HRESULT r2 = env->CreateCoreWebView2Controller(
                    impl->parent,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [impl](HRESULT rc, ICoreWebView2Controller* ctl) -> HRESULT
                        {
                            if (FAILED(rc) || !ctl)
                            {
                                if (impl->cb.OnInitFailed)
                                    impl->cb.OnInitFailed();
                                return S_OK;
                            }
                            ApplyControllerReady(impl, ctl);
                            return S_OK;
                        })
                        .Get());
                if (FAILED(r2) && impl->cb.OnInitFailed)
                    impl->cb.OnInitFailed();
                return S_OK;
            })
            .Get());

    if (FAILED(hr))
    {
        if (cb.OnInitFailed)
            cb.OnInitFailed();
        return false;
    }
    return true;
}

bool CTcWebHost::IsReady() const { return p->ready; }

void CTcWebHost::Resize(int cx, int cy)
{
    if (p->controller)
    {
        RECT rc = {0, 0, cx, cy};
        p->controller->put_Bounds(rc);
    }
}

void CTcWebHost::SetZoomPercent(int pct)
{
    p->pendingZoom = pct;
    if (p->controller)
        p->controller->put_ZoomFactor(pct / 100.0);
}

void CTcWebHost::SetBackgroundColor(COLORREF color)
{
    // no GetGValue/GetBValue: their (WORD) cast trips /RTCc in debug builds
    p->bgColor = {0xFF, (BYTE)(color & 0xFF), (BYTE)((color >> 8) & 0xFF), (BYTE)((color >> 16) & 0xFF)};
    p->bgColorSet = true;
    ApplyBackgroundColor(p);
}

void CTcWebHost::Navigate(int version, const std::wstring& fragment)
{
    if (!p->webview)
        return;
    std::wstring url = p->baseUrl;
    url += L"?v=";
    wchar_t num[16];
    _itow_s(version, num, 10);
    url += num;
    if (!fragment.empty())
    {
        url += L"#";
        url += fragment;
    }
    p->webview->Navigate(url.c_str());
}

void CTcWebHost::PostWebMessageJson(const std::wstring& json)
{
    if (p->webview && p->cfg.WebMessagesEnabled)
        p->webview->PostWebMessageAsJson(json.c_str());
}

void CTcWebHost::Focus()
{
    if (p->controller)
        p->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
}

void CTcWebHost::Destroy()
{
    if (p->controller)
    {
        p->controller->Close();
        p->controller.Reset();
    }
    p->webview.Reset();
    p->env.Reset();
    p->ready = false;
    if (p->comInit)
    {
        CoUninitialize();
        p->comInit = false;
    }
}
