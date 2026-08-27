// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// webhost.h - CTcWebHost: the product's shared, locked-down WebView2 hosting
// surface.
//
// Lifted out of src/plugins/mdview/webview.cpp by feature 070 because the
// shared-engine contract requires the second WebView2 consumer to move the
// common code here instead of copying it
// (architecture/11-webview2-integration.md S2.5,
//  specs/070-source-viewer-plugin/contracts/webview-host-sharing.md).
//
// What is shared: environment options (the ONE browser-arguments set), the
// canonical user data folder, the availability gate, environment/controller
// creation, the whole settings lockdown, navigation/new-window/resource
// interception, accelerator routing, zoom and background colour.
//
// What stays per-plugin: everything in TcWebHostConfig below -- the virtual
// host name, whether scripts and web messages are enabled, what the
// interceptor serves, and which keys map to which command.
//
// This header is COM-free on purpose so plugin code can hold a host by
// pointer without dragging WRL into its translation units.

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

struct CTcWebHostImpl;

// One answer from a plugin's resource interceptor.
struct TcWebResponse
{
    const BYTE* Data = nullptr;   // borrowed; must outlive the call
    size_t Size = 0;
    std::wstring ContentType;     // e.g. L"text/html; charset=utf-8"
    std::wstring ExtraHeaders;    // additional headers, CRLF-separated, no trailing CRLF
    int Status = 200;
    const wchar_t* Reason = L"OK";
};

// Per-plugin configuration. Anything not here is an invariant of the shared
// lockdown and cannot be relaxed by a plugin.
struct TcWebHostConfig
{
    // Private origin, host part only (e.g. L"codeview.invalid"). The document
    // is always <https://host/DocumentPath>.
    std::wstring VirtualHost;
    std::wstring DocumentPath = L"doc.html";

    // mdview keeps both FALSE (feature 021 lockdown). codeview enables both:
    // its highlighting engine is bundled script and it needs a small typed
    // message channel (spec clarification 2026-08-26, FR-030..033).
    bool ScriptsEnabled = false;
    bool WebMessagesEnabled = false;

    // Answer a request for 'path' (the URL path, query and fragment stripped,
    // WITHOUT the leading slash). Return false to deny -- the host then serves
    // 403 (default-deny is the invariant, not the plugin's choice).
    std::function<bool(const std::wstring& path, TcWebResponse& out)> Serve;

    // Map a key to a plugin command id, or return 0 to let the engine have it.
    std::function<int(UINT vk, bool ctrl, bool shift)> Accelerator;

    // Diagnostics prefix for TRACE lines (e.g. L"codeview").
    std::string TraceName = "webhost";
};

class CTcWebHost
{
public:
    struct Callbacks
    {
        std::function<void()> OnReady;                            // controller ready
        std::function<void(const std::wstring&)> OnActivateLink;  // navigation was refused
        std::function<void()> OnInitFailed;                       // env/controller failure
        std::function<void()> OnProcessFailed;                    // renderer crashed
        std::function<void(int)> OnZoomChanged;                   // engine-driven zoom, percent
        std::function<void(const std::wstring&)> OnWebMessage;    // only when WebMessagesEnabled
    };

    CTcWebHost();
    ~CTcWebHost();

    bool Create(HWND parent, const std::wstring& userDataFolder,
                const TcWebHostConfig& config, const Callbacks& cb);
    bool IsReady() const;
    void Resize(int cx, int cy);
    void SetZoomPercent(int pct);
    void Focus();

    // Surface colour shown before/between navigations (kills the white flash).
    // Callable before the controller exists; applied when it becomes ready.
    void SetBackgroundColor(COLORREF color);

    // Navigates to the configured document. 'version' cache-busts the URL when
    // the served content changed; 'fragment' scrolls within the same document.
    void Navigate(int version, const std::wstring& fragment = std::wstring());

    // Sends a JSON string to the page (no-op unless WebMessagesEnabled).
    void PostWebMessageJson(const std::wstring& json);

    void Destroy();

    static bool RuntimeAvailable();

private:
    CTcWebHostImpl* p;
    CTcWebHost(const CTcWebHost&) = delete;
    CTcWebHost& operator=(const CTcWebHost&) = delete;
};

// --- shared-engine contract helpers ---------------------------------------
//
// architecture/11-webview2-integration.md S2.1: every WebView2 environment in
// the product uses this one folder. A different folder spawns a second, cold
// browser tree and gains nothing from any keeper.
std::wstring TcWebUserDataFolder();
