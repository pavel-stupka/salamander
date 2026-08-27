// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// darkmenu.h - owner-drawn dark rendering for the viewer's native menu bar
// and its popups (feature 037). The main application's menus are dark because
// they are owner-drawn (core CMenuPopup/CMenuBar); native menus cannot follow
// the Dark theme with documented APIs, so mdview applies the same technique
// locally. Colors come exclusively from the engine palette exported in
// feature 036 (CSalamanderGeneralAbstract::GetThemeSysColor/-Brush); the
// engine owns those brushes - never DeleteObject them here. Activate only
// when IsDarkThemeActive() was TRUE at window creation; in Default theme the
// menu must stay untouched native.

#pragma once

// Converts every item of 'bar' (and, recursively, of all submenus) to
// MFT_OWNERDRAW with attached paint data, and sets the dark background brush
// via MENUINFO. Call once after the menu is fully built (SetMenu done).
void DarkMenuApply(HMENU bar);

// Frees the paint data attached by DarkMenuApply. Call before the menu is
// destroyed (WM_DESTROY); the HMENU itself is destroyed with the window.
void DarkMenuRelease(HMENU bar);

// WM_MEASUREITEM / WM_DRAWITEM handlers; return FALSE if the item is not one
// of ours (caller falls through to default handling).
BOOL DarkMenuMeasureItem(MEASUREITEMSTRUCT* mis);
BOOL DarkMenuDrawItem(const DRAWITEMSTRUCT* dis);

// Owner-drawn items lose automatic '&' mnemonic matching; call on WM_MENUCHAR
// (lParam is the HMENU). Returns MAKELRESULT(index, MNC_EXECUTE/MNC_SELECT)
// or MAKELRESULT(0, MNC_IGNORE) when no item matches.
LRESULT DarkMenuHandleMenuChar(HMENU menu, WPARAM wParam);

// Releases the cached menu font (module cleanup; call from ReleaseViewer).
void DarkMenuReleaseFont();
