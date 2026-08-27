// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// schemes.h - the shipped colour schemes (spec FR-013, research D16).
//
// A scheme is a VS Code theme shipped in web\shiki\themes; the page reads the
// full theme, the host only needs the editor background and foreground so it
// can paint the window and the WebView surface in the scheme's colour BEFORE
// the page has rendered anything (spec FR-015: no white flash, ever).

#pragma once

struct CvScheme
{
    const char* Id;    // stable id, persisted and sent to the page
    BOOL Dark;         // polarity: picks the slot in follow-application mode
    COLORREF Bg;       // editor.background
    COLORREF Fg;       // editor.foreground
};

extern const CvScheme CvSchemes[];
extern const int CvSchemeCount;

// Index of a scheme id, or -1. NULL/empty yields -1.
int CvFindScheme(const char* id);

// The scheme actually in force: the explicit choice, or -- in follow mode --
// the light/dark slot chosen by the application's own theme.
const CvScheme* CvEffectiveScheme();
