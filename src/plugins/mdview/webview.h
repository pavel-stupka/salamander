// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// webview.h - CMdWebHost: the locked-down WebView2 rendering surface for
// mdview. All WebView2/COM usage is confined to webview.cpp; this header is
// COM-free so viewer.cpp can hold a CMdWebHost by pointer. See
// specs/021-mdview-html-renderer/contracts/webhost.md.

#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include "htmlgen.h"

struct CMdWebHostImpl; // defined in webview.cpp

class CMdWebHost
{
public:
    struct Callbacks
    {
        std::function<void()> OnReady;                           // controller ready -> render
        std::function<void(const std::wstring&)> OnActivateLink; // navigation gate
        std::function<void()> OnInitFailed;                      // env/controller/runtime failure
        std::function<void()> OnProcessFailed;                   // renderer crashed
        std::function<void(int)> OnZoomChanged;                  // engine-driven zoom (percent)
    };

    CMdWebHost();
    ~CMdWebHost();

    // Starts async environment+controller creation parented to 'parent'.
    bool Create(HWND parent, const std::wstring& userDataFolder, const Callbacks& cb);
    bool IsReady() const;
    void Resize(int cx, int cy);
    void SetZoomPercent(int pct);
    void Focus(); // move keyboard focus into the rendered content

    // Sets the surface color shown before/between navigations (kills the white
    // flash on open). Callable before the controller exists; applied on ready.
    void SetBackgroundColor(COLORREF color);

    // Sets the document served by the interceptor (does not navigate).
    void SetDocument(const MdHtmlResult* doc, const std::wstring& docDir);
    // (Re)loads the current document; optional same-document '#fragment'.
    void Navigate(const std::wstring& fragment = std::wstring());
    void Destroy();

    static bool RuntimeAvailable();

private:
    CMdWebHostImpl* p;
    CMdWebHost(const CMdWebHost&) = delete;
    CMdWebHost& operator=(const CMdWebHost&) = delete;
};

// --- feature 065: shared-engine contract + session keeper ------------------
//
// The WebView2 browser-process tree is shared by user data folder; the
// helpers below (and the environment-options helper inside webview.cpp) are
// the single source of truth for the app-wide sharing contract every future
// WebView2 consumer inherits. See architecture/11-webview2-integration.md
// and specs/065-mdview-instant-render/contracts/keeper.md.

// Canonical user data folder: %LOCALAPPDATA%\Tandem Commander\WebView2.
// Used by the viewer windows and the keeper alike.
std::wstring MdUserDataFolder();

// Best-effort removal of the pre-065 mdview.WebView2 cache folder (cache
// only, nothing is migrated). Once per session; main-thread-only; failures
// are silent and retried next session.
void MdCleanupOldUserDataFolder();

// Session keeper: a hidden environment+controller that keeps the shared
// browser tree alive so every view after the first attaches warm. All three
// are MAIN-THREAD-ONLY and idempotent; every failure path is silent (the
// next view may arm again). Disarm restores the current build's lifecycle.
void MdKeeperArm();
void MdKeeperDisarm();
bool MdKeeperArmed();
