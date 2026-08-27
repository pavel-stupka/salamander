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

// --- message building (host -> page). All values are host-controlled; file
//     content never travels through here (contract S3). ---
std::wstring CvMsgInit(const CvIntake& intake, const char* schemeId, BOOL swap);
std::wstring CvMsgSetTheme(const char* schemeId);
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
