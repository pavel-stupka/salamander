// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// webglue.h - codeview's side of the shared WebView2 host: the asset
// interceptor, the accelerator map and the typed message channel.
// Contract: contracts/host-page-interface.md, contracts/rendering-lockdown.md.

#pragma once

#include <string>
#include "webhost.h"

struct CvIntake;

// Fills in everything the shared host needs for a codeview controller
// (virtual host, scripts on, web messages on, serve + accelerator callbacks).
// 'textProvider' hands out the decoded document for the "text" resource; it is
// owned by the viewer window and must outlive the host.
void CvConfigureHost(TcWebHostConfig& cfg, const std::string** textProvider);

struct CvScheme;

// --- message building (host -> page). All values are host-controlled; file
//     content never travels through here (contract S3). ---
//
// init and setTheme carry the scheme's editor colours ("themeInfo") so the
// page can paint the right background/foreground IMMEDIATELY -- before (and
// independently of) the worker loading the full theme. This is what keeps a
// plain-band file re-themeable and what closes the colour gap between the
// page's first paint and the first tokenized line (spec FR-015).
std::wstring CvMsgInit(const CvIntake& intake, const CvScheme* scheme, BOOL swap);
std::wstring CvMsgSetTheme(const CvScheme* scheme);

// URL fragment "bg=RRGGBB&fg=RRGGBB&polarity=dark" for Navigate(): viewer.js
// applies it synchronously at module start, before the page's first paint.
std::wstring CvSchemeFragment(const CvScheme* scheme);
std::wstring CvMsgSetView();
std::wstring CvMsgSetLanguage(int language);
std::wstring CvMsgFind(const wchar_t* term, BOOL caseSensitive, BOOL wholeWord, int dir);
std::wstring CvMsgGotoLine(int line, int col);

// --- message parsing (page -> host). Returns FALSE for anything that is not a
//     known message with a well-formed payload; the caller then ignores it. ---
struct CvPageMessage
{
    std::wstring Type;
    int Current = 0, Total = 0;   // findResult
    int Line = 0, Col = 0;        // caret
    int X = 0, Y = 0;             // contextMenu
    BOOL HasSelection = FALSE;
    int Lines = 0;                // rendered
    std::wstring Reason;          // highlightAborted
};
BOOL CvParsePageMessage(const std::wstring& json, CvPageMessage& out);
