#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

struct MAGPRESENTATIONSETTINGS;

#define MAG_UI_MAX_DRAW_COMMANDS 128
#define MAG_UI_MAX_TEXT_LENGTH 64

typedef enum GRAPHICSAPI
{
  GRAPHICS_API_OPENGL = 0,
  GRAPHICS_API_GDI,
  GRAPHICS_API_D3D9,
  GRAPHICS_API_D3D11,
  GRAPHICS_API_D3D12,
  GRAPHICS_API_VULKAN,
  GRAPHICS_API_COUNT
} GRAPHICSAPI;

typedef enum UIGRAPHICSAPI
{
  UI_GRAPHICS_API_NATIVE = 0,
  UI_GRAPHICS_API_DIRECT2D,
  UI_GRAPHICS_API_COUNT
} UIGRAPHICSAPI;

typedef enum TEXTRENDERER
{
  TEXT_RENDERER_DIRECTWRITE = 0,
  TEXT_RENDERER_GPU_GLYPH_ATLAS,
  TEXT_RENDERER_GDI,
  TEXT_RENDERER_COUNT
} TEXTRENDERER;

typedef enum CAPTUREAPI
{
  CAPTURE_API_GDI_BITBLT = 0,
  CAPTURE_API_DXGI_DESKTOP_DUPLICATION,
  CAPTURE_API_WINDOWS_GRAPHICS_CAPTURE,
  CAPTURE_API_DWM_THUMBNAIL,
  CAPTURE_API_DWM_PRIVATE_VISUAL,
  CAPTURE_API_COUNT
} CAPTUREAPI;

typedef enum MAGROWORDER
{
  MAG_ROW_ORDER_TOP_DOWN = 0,
  MAG_ROW_ORDER_BOTTOM_UP
} MAGROWORDER;

typedef enum MAGALPHAMODE
{
  MAG_ALPHA_MODE_IGNORE = 0,
  MAG_ALPHA_MODE_STRAIGHT,
  MAG_ALPHA_MODE_PREMULTIPLIED
} MAGALPHAMODE;

typedef struct MAGPIXELBUFFER
{
  BYTE*        pixels;
  UINT         width;
  UINT         height;
  UINT         stride;
  MAGROWORDER  rowOrder;
  MAGALPHAMODE alphaMode;
} MAGPIXELBUFFER, *LPMAGPIXELBUFFER;

typedef struct MAGCOLORF
{
  FLOAT r;
  FLOAT g;
  FLOAT b;
  FLOAT a;
} MAGCOLORF;

typedef enum MAGUIDRAWCOMMANDTYPE
{
  MAG_UI_DRAW_FILL_RECT = 0,
  MAG_UI_DRAW_STROKE_RECT,
  MAG_UI_DRAW_TEXT
} MAGUIDRAWCOMMANDTYPE;

typedef struct MAGUIDRAWCOMMAND
{
  MAGUIDRAWCOMMANDTYPE type;
  RECT                 rect;
  MAGCOLORF            color;
  FLOAT                thickness;
  FLOAT                fontSize;
  TCHAR                text[MAG_UI_MAX_TEXT_LENGTH];
} MAGUIDRAWCOMMAND;

typedef struct MAGGLYPHINFO
{
  RECT rect;
  INT  advance;
} MAGGLYPHINFO;

typedef struct MAGGLYPHATLAS
{
  const BYTE*         alpha;
  UINT                width;
  UINT                height;
  UINT                stride;
  WCHAR               firstCharacter;
  UINT                glyphCount;
  const MAGGLYPHINFO* glyphs;
} MAGGLYPHATLAS;

typedef struct MAGUIDRAWLIST
{
  MAGUIDRAWCOMMAND commands[MAG_UI_MAX_DRAW_COMMANDS];
  UINT             count;
  const MAGGLYPHATLAS* glyphAtlas;
} MAGUIDRAWLIST, *LPMAGUIDRAWLIST;

typedef struct MAGCPUCOMPOSITOR
{
  BYTE*  pixels;
  SIZE_T capacity;
  UINT64 generation;
} MAGCPUCOMPOSITOR, *LPMAGCPUCOMPOSITOR;

typedef struct MAGPRESENTINTENT
{
  BOOL restartSequence;
  BOOL synchronize;
} MAGPRESENTINTENT;

typedef struct MAGGRAPHICSBACKEND
{
  GRAPHICSAPI api;
  LPCTSTR     name;
  BOOL        implemented;
  BOOL (*IsAvailable)(LPTSTR reason, UINT reasonCount);
  BOOL (*Create)(
    HWND hWnd,
    SIZE clientSize,
    const struct MAGPRESENTATIONSETTINGS* presentation,
    void** stateOut);
  void (*Destroy)(HWND hWnd, void* state);
  BOOL (*Resize)(HWND hWnd, void* state, SIZE clientSize);
  BOOL (*SetPresentationEnabled)(HWND hWnd, void* state, BOOL enabled);
  BOOL (*Render)(
    HWND hWnd,
    void* state,
    const MAGPIXELBUFFER* frame,
    const MAGUIDRAWLIST* ui,
    const MAGPRESENTINTENT* intent);
  HANDLE (*GetFrameWaitHandle)(void* state);
  UINT64 (*GetResourceGeneration)(void* state);
  BOOL (*GetNextEstimatedFrameTime)(void* state, LONGLONG* frameTime);
  BOOL (*GetObservedPresentationTarget)(void* state, UINT* target);
} MAGGRAPHICSBACKEND;

void magUiDrawListReset(LPMAGUIDRAWLIST list);
BOOL magUiDrawListAppendFill(LPMAGUIDRAWLIST list, const RECT* rect, MAGCOLORF color);
BOOL magUiDrawListAppendStroke(LPMAGUIDRAWLIST list, const RECT* rect, MAGCOLORF color, FLOAT thickness);
BOOL magUiDrawListAppendText(
  LPMAGUIDRAWLIST list,
  const RECT* rect,
  MAGCOLORF color,
  FLOAT fontSize,
  LPCTSTR value);

BOOL magGraphicsComposeFrame(
  LPMAGCPUCOMPOSITOR compositor,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui,
  LPMAGPIXELBUFFER output);
BOOL magGraphicsReserveCpuCompositor(
  LPMAGCPUCOMPOSITOR compositor,
  UINT width,
  UINT height);
void magGraphicsDestroyCpuCompositor(LPMAGCPUCOMPOSITOR compositor);

UINT magGraphicsGetBackendCount(void);
const MAGGRAPHICSBACKEND* magGraphicsGetBackendAt(UINT index);
const MAGGRAPHICSBACKEND* magGraphicsGetBackend(GRAPHICSAPI api);
BOOL magGraphicsSetPresentationEnabledNoop(HWND hWnd, void* state, BOOL enabled);
SIZE magGraphicsChooseReservoirSize(HWND hWnd, SIZE minimumSize);
BOOL magGraphicsIsInputDesktop(void);

extern const MAGGRAPHICSBACKEND g_magGraphicsOpenGLBackend;
extern const MAGGRAPHICSBACKEND g_magGraphicsGdiBackend;
extern const MAGGRAPHICSBACKEND g_magGraphicsD3D9Backend;
extern const MAGGRAPHICSBACKEND g_magGraphicsD3D11Backend;
extern const MAGGRAPHICSBACKEND g_magGraphicsD3D12Backend;
extern const MAGGRAPHICSBACKEND g_magGraphicsVulkanBackend;

#ifdef __cplusplus
}
#endif
