// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// codeview - source and configuration file viewer plugin for Tandem Commander
//
//****************************************************************************

#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <CommDlg.h>
#include <ShellAPI.h>
#include <shlobj.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#include <limits.h>
#include <process.h>
#include <commctrl.h>
#include <ostream>
#include <stdio.h>
#include <time.h>

#include <string>
#include <vector>
#include <utility>
#include <cwctype>
#include <cstdlib>
#include <cwchar>

#if defined(_DEBUG) && defined(_MSC_VER)
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

// UTF-8 <-> UTF-16 helpers (plugin interface 104): every char* path crossing the
// Salamander interface is UTF-8. Include before the spl_* headers.
#include "splunicode.h"

#include "versinfo.rh2"

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_view.h"
#include "spl_vers.h"
#include "spl_gui.h"

#include "dbg.h"
#include "mhandles.h"
#include "arraylt.h"
#include "winliblt.h"
#include "auxtools.h"

#include "codeview.h"
#include "codeview.rh2"
#include "lang\lang.rh"
