// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "schemes.h"

// RGB() takes r,g,b; the theme colours below are written as #RRGGBB above each
// entry so they can be checked against web\shiki\themes at a glance.
// The 12 shipped schemes: 5 light, 7 dark (spec FR-013 needs at least 3 + 3).
const CvScheme CvSchemes[] = {
    // --- light ---
    {"github-light", FALSE, RGB(0xFF, 0xFF, 0xFF), RGB(0x24, 0x29, 0x2E)},        // #ffffff
    {"light-plus", FALSE, RGB(0xFF, 0xFF, 0xFF), RGB(0x00, 0x00, 0x00)},          // #FFFFFF
    {"one-light", FALSE, RGB(0xFA, 0xFA, 0xFA), RGB(0x38, 0x3A, 0x42)},           // #FAFAFA
    {"solarized-light", FALSE, RGB(0xFD, 0xF6, 0xE3), RGB(0x65, 0x7B, 0x83)},     // #FDF6E3
    {"catppuccin-latte", FALSE, RGB(0xEF, 0xF1, 0xF5), RGB(0x4C, 0x4F, 0x69)},    // #eff1f5
    // --- dark ---
    {"github-dark", TRUE, RGB(0x24, 0x29, 0x2E), RGB(0xE1, 0xE4, 0xE8)},          // #24292e
    {"dark-plus", TRUE, RGB(0x1E, 0x1E, 0x1E), RGB(0xD4, 0xD4, 0xD4)},            // #1E1E1E
    {"one-dark-pro", TRUE, RGB(0x28, 0x2C, 0x34), RGB(0xAB, 0xB2, 0xBF)},         // #282c34
    {"solarized-dark", TRUE, RGB(0x00, 0x2B, 0x36), RGB(0x83, 0x94, 0x96)},       // #002B36
    {"catppuccin-mocha", TRUE, RGB(0x1E, 0x1E, 0x2E), RGB(0xCD, 0xD6, 0xF4)},     // #1e1e2e
    {"gruvbox-dark-medium", TRUE, RGB(0x28, 0x28, 0x28), RGB(0xEB, 0xDB, 0xB2)},  // #282828
    {"nord", TRUE, RGB(0x2E, 0x34, 0x40), RGB(0xD8, 0xDE, 0xE9)},                 // #2e3440
};
const int CvSchemeCount = (int)(sizeof(CvSchemes) / sizeof(CvSchemes[0]));

int CvFindScheme(const char* id)
{
    if (id == NULL || *id == 0)
        return -1;
    for (int i = 0; i < CvSchemeCount; i++)
        if (SalamanderGeneral->StrICmp(CvSchemes[i].Id, id) == 0)
            return i;
    return -1;
}

const CvScheme* CvEffectiveScheme()
{
    if (g_followApp)
    {
        // The application's own Default/Dark theme decides the polarity; the
        // per-polarity slots let the user pick which light and which dark
        // scheme that means (mdview's EffectiveTheme pattern).
        BOOL dark = SalamanderGeneral->IsDarkThemeActive();
        int i = CvFindScheme(dark ? g_schemeDark : g_schemeLight);
        if (i < 0)
        {
            // Fall back to the first scheme of the right polarity.
            for (int k = 0; k < CvSchemeCount; k++)
                if ((CvSchemes[k].Dark != FALSE) == (dark != FALSE))
                    return &CvSchemes[k];
            return &CvSchemes[0];
        }
        return &CvSchemes[i];
    }
    int i = CvFindScheme(g_scheme);
    return &CvSchemes[i < 0 ? 0 : i];
}
