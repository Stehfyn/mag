#pragma once

#include "targetver.h"
#include "resource.h"
// Windows Header Files
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define COBJMACROS
#include <windows.h>
#include <windowsx.h>
#include <htmlhelp.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <Shlwapi.h>
// C RunTime Header Files
#include <math.h>
#include <tchar.h>

// Application Extension Header Files
#include "wingdix.h"
#include "winuserx.h"
#include "dwmapix.h"
#include "d3dkmtx.h"

#define ABS(x)         ((x < 0) ? -x : x)
#define RECTWIDTH(rc)  (ABS(rc.right - rc.left))
#define RECTHEIGHT(rc) (ABS(rc.bottom - rc.top))
#define CLAMP(x,lo,hi) (max(min(x, hi),lo))
