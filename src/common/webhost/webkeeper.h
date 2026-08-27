// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// webkeeper.h - session keeper for the shared WebView2 browser tree.
//
// Lifted out of src/plugins/mdview/webview.cpp by feature 070
// (contracts/webview-host-sharing.md S1; original design and invariants:
//  specs/065-mdview-instant-render/contracts/keeper.md).
//
// The keeper holds one hidden, suspended controller so the shared browser
// process tree never shuts down between viewer windows. Any ONE live
// controller keeps the tree warm for every consumer, so whichever plugin the
// user reaches first warms the others -- but each plugin arms its OWN keeper
// at its OWN first use, never earlier (065 FR-001: zero background work and
// zero footprint before the plugin's first real use).
//
// Everything here is MAIN-THREAD-ONLY (the main message loop dispatches the
// async completions), idempotent, and silent on every failure path: a keeper
// that cannot arm simply leaves the next view to pay the cold start.

#pragma once

#include <windows.h>

struct TcWebKeeperConfig
{
    // Window class for the hidden holder window. MUST be unique per plugin:
    // two plugins registering the same class name in the same process would
    // fight over it (mdview uses L"TandemMdKeeperWnd").
    const wchar_t* ClassName = nullptr;
    // The plugin's own module handle -- the class is registered against it and
    // must be unregistered with it when the plugin unloads.
    HINSTANCE Instance = nullptr;
    // Prefix for TRACE lines, e.g. "codeview keeper".
    const char* TraceName = "keeper";
};

class CTcWebKeeper
{
public:
    // Arms the keeper if it is not already arming/armed. Silent on failure.
    void Arm(const TcWebKeeperConfig& config);
    // Releases the controller and unregisters the window class. Safe to call
    // when unarmed -- feature 069 (F-P6-01) requires the class to be released
    // on the unload path even then.
    void Disarm();
    bool Armed() const;

private:
    void ReleaseAll();
    void UnregisterClassIfRegistered();
    void ControllerReady(void* controller); // ICoreWebView2Controller*

    friend struct CTcWebKeeperAccess;
    void* State = nullptr; // CTcWebKeeperState*, created lazily
};
