#pragma once

#include <windows.h>
#include <unknwn.h>

/* Minimal C declarations for the Direct2D/DirectWrite COM ABI consumed by
   ui_renderer.c.  Current SDK convenience headers contain C++-only syntax;
   the binary interfaces themselves remain ordinary COM vtables. */

typedef UINT32 D2D1_FACTORY_TYPE;
typedef UINT32 D2D1_RENDER_TARGET_TYPE;
typedef UINT32 D2D1_ALPHA_MODE;
typedef UINT32 D2D1_RENDER_TARGET_USAGE;
typedef UINT32 D2D1_FEATURE_LEVEL;
typedef UINT32 D2D1_TEXT_ANTIALIAS_MODE;
typedef UINT32 D2D1_DRAW_TEXT_OPTIONS;
typedef UINT32 DWRITE_FACTORY_TYPE;
typedef UINT32 DWRITE_FONT_WEIGHT;
typedef UINT32 DWRITE_FONT_STYLE;
typedef UINT32 DWRITE_FONT_STRETCH;
typedef UINT32 DWRITE_TEXT_ALIGNMENT;
typedef UINT32 DWRITE_PARAGRAPH_ALIGNMENT;
typedef UINT32 DWRITE_WORD_WRAPPING;
typedef UINT32 DWRITE_MEASURING_MODE;

#define D2D1_FACTORY_TYPE_SINGLE_THREADED 0U
#define D2D1_RENDER_TARGET_TYPE_SOFTWARE 1U
#define D2D1_ALPHA_MODE_IGNORE 3U
#define D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE 2U
#define D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE 2U
#define D2D1_DRAW_TEXT_OPTIONS_CLIP 2U
#define DWRITE_FACTORY_TYPE_SHARED 0U
#define DWRITE_FONT_WEIGHT_NORMAL 400U
#define DWRITE_FONT_WEIGHT_SEMI_BOLD 600U
#define DWRITE_FONT_STYLE_NORMAL 0U
#define DWRITE_FONT_STRETCH_NORMAL 5U
#define DWRITE_TEXT_ALIGNMENT_LEADING 0U
#define DWRITE_PARAGRAPH_ALIGNMENT_NEAR 0U
#define DWRITE_PARAGRAPH_ALIGNMENT_CENTER 2U
#define DWRITE_WORD_WRAPPING_WRAP 0U
#define DWRITE_WORD_WRAPPING_NO_WRAP 1U
#define DWRITE_MEASURING_MODE_NATURAL 0U
#define MAG_DXGI_FORMAT_B8G8R8A8_UNORM 87U

typedef struct D2D1_PIXEL_FORMAT
{
  UINT32 format;
  D2D1_ALPHA_MODE alphaMode;
} D2D1_PIXEL_FORMAT;

typedef struct D2D1_RENDER_TARGET_PROPERTIES
{
  D2D1_RENDER_TARGET_TYPE type;
  D2D1_PIXEL_FORMAT pixelFormat;
  FLOAT dpiX;
  FLOAT dpiY;
  D2D1_RENDER_TARGET_USAGE usage;
  D2D1_FEATURE_LEVEL minLevel;
} D2D1_RENDER_TARGET_PROPERTIES;

typedef struct D2D1_COLOR_F
{
  FLOAT r;
  FLOAT g;
  FLOAT b;
  FLOAT a;
} D2D1_COLOR_F;

typedef struct D2D1_RECT_F
{
  FLOAT left;
  FLOAT top;
  FLOAT right;
  FLOAT bottom;
} D2D1_RECT_F;

typedef struct D2D1_POINT_2F
{
  FLOAT x;
  FLOAT y;
} D2D1_POINT_2F;

typedef struct D2D1_ROUNDED_RECT
{
  D2D1_RECT_F rect;
  FLOAT radiusX;
  FLOAT radiusY;
} D2D1_ROUNDED_RECT;

typedef struct DWRITE_TEXT_METRICS
{
  FLOAT left;
  FLOAT top;
  FLOAT width;
  FLOAT widthIncludingTrailingWhitespace;
  FLOAT height;
  FLOAT layoutWidth;
  FLOAT layoutHeight;
  UINT32 maxBidiReorderingDepth;
  UINT32 lineCount;
} DWRITE_TEXT_METRICS;

typedef struct ID2D1Factory ID2D1Factory;
typedef struct ID2D1DCRenderTarget ID2D1DCRenderTarget;
typedef struct ID2D1SolidColorBrush ID2D1SolidColorBrush;
typedef struct IDWriteFactory IDWriteFactory;
typedef struct IDWriteTextFormat IDWriteTextFormat;
typedef struct IDWriteTextLayout IDWriteTextLayout;

typedef struct ID2D1FactoryVtbl
{
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(ID2D1Factory*, REFIID, void**);
  ULONG (STDMETHODCALLTYPE* AddRef)(ID2D1Factory*);
  ULONG (STDMETHODCALLTYPE* Release)(ID2D1Factory*);
  void* ReloadSystemMetrics;
  void* GetDesktopDpi;
  void* CreateRectangleGeometry;
  void* CreateRoundedRectangleGeometry;
  void* CreateEllipseGeometry;
  void* CreateGeometryGroup;
  void* CreateTransformedGeometry;
  void* CreatePathGeometry;
  void* CreateStrokeStyle;
  void* CreateDrawingStateBlock;
  void* CreateWicBitmapRenderTarget;
  void* CreateHwndRenderTarget;
  void* CreateDxgiSurfaceRenderTarget;
  HRESULT (STDMETHODCALLTYPE* CreateDCRenderTarget)(
    ID2D1Factory*,
    const D2D1_RENDER_TARGET_PROPERTIES*,
    ID2D1DCRenderTarget**);
} ID2D1FactoryVtbl;

struct ID2D1Factory
{
  const ID2D1FactoryVtbl* lpVtbl;
};

typedef struct ID2D1DCRenderTargetVtbl
{
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(ID2D1DCRenderTarget*, REFIID, void**);
  ULONG (STDMETHODCALLTYPE* AddRef)(ID2D1DCRenderTarget*);
  ULONG (STDMETHODCALLTYPE* Release)(ID2D1DCRenderTarget*);
  void* GetFactory;
  void* CreateBitmapBeforeSolidBrush[4];
  HRESULT (STDMETHODCALLTYPE* CreateSolidColorBrush)(
    ID2D1DCRenderTarget*,
    const D2D1_COLOR_F*,
    const void*,
    ID2D1SolidColorBrush**);
  void* CreateResourcesAfterSolidBrush[6];
  void (STDMETHODCALLTYPE* DrawLine)(
    ID2D1DCRenderTarget*,
    D2D1_POINT_2F,
    D2D1_POINT_2F,
    ID2D1SolidColorBrush*,
    FLOAT,
    void*);
  void (STDMETHODCALLTYPE* DrawRectangle)(
    ID2D1DCRenderTarget*,
    const D2D1_RECT_F*,
    ID2D1SolidColorBrush*,
    FLOAT,
    void*);
  void (STDMETHODCALLTYPE* FillRectangle)(
    ID2D1DCRenderTarget*,
    const D2D1_RECT_F*,
    ID2D1SolidColorBrush*);
  void (STDMETHODCALLTYPE* DrawRoundedRectangle)(
    ID2D1DCRenderTarget*,
    const D2D1_ROUNDED_RECT*,
    ID2D1SolidColorBrush*,
    FLOAT,
    void*);
  void (STDMETHODCALLTYPE* FillRoundedRectangle)(
    ID2D1DCRenderTarget*,
    const D2D1_ROUNDED_RECT*,
    ID2D1SolidColorBrush*);
  void* ShapesAfterRoundedRectangleBeforeDrawText[7];
  void (STDMETHODCALLTYPE* DrawText)(
    ID2D1DCRenderTarget*,
    const WCHAR*,
    UINT32,
    IDWriteTextFormat*,
    const D2D1_RECT_F*,
    ID2D1SolidColorBrush*,
    D2D1_DRAW_TEXT_OPTIONS,
    DWRITE_MEASURING_MODE);
  void* DrawingStateBeforeTextAntialias[6];
  void (STDMETHODCALLTYPE* SetTextAntialiasMode)(
    ID2D1DCRenderTarget*,
    D2D1_TEXT_ANTIALIAS_MODE);
  void* DrawingStateBeforeClear[12];
  void (STDMETHODCALLTYPE* Clear)(ID2D1DCRenderTarget*, const D2D1_COLOR_F*);
  void (STDMETHODCALLTYPE* BeginDraw)(ID2D1DCRenderTarget*);
  HRESULT (STDMETHODCALLTYPE* EndDraw)(ID2D1DCRenderTarget*, UINT64*, UINT64*);
  void* QueriesBeforeBindDC[7];
  HRESULT (STDMETHODCALLTYPE* BindDC)(ID2D1DCRenderTarget*, HDC, const RECT*);
} ID2D1DCRenderTargetVtbl;

struct ID2D1DCRenderTarget
{
  const ID2D1DCRenderTargetVtbl* lpVtbl;
};

typedef struct ID2D1SolidColorBrushVtbl
{
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(ID2D1SolidColorBrush*, REFIID, void**);
  ULONG (STDMETHODCALLTYPE* AddRef)(ID2D1SolidColorBrush*);
  ULONG (STDMETHODCALLTYPE* Release)(ID2D1SolidColorBrush*);
  void* ResourceAndBrushMethods[5];
  void (STDMETHODCALLTYPE* SetColor)(ID2D1SolidColorBrush*, const D2D1_COLOR_F*);
  void* GetColor;
} ID2D1SolidColorBrushVtbl;

struct ID2D1SolidColorBrush
{
  const ID2D1SolidColorBrushVtbl* lpVtbl;
};

typedef struct IDWriteTextFormatVtbl
{
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(IDWriteTextFormat*, REFIID, void**);
  ULONG (STDMETHODCALLTYPE* AddRef)(IDWriteTextFormat*);
  ULONG (STDMETHODCALLTYPE* Release)(IDWriteTextFormat*);
  HRESULT (STDMETHODCALLTYPE* SetTextAlignment)(IDWriteTextFormat*, DWRITE_TEXT_ALIGNMENT);
  HRESULT (STDMETHODCALLTYPE* SetParagraphAlignment)(IDWriteTextFormat*, DWRITE_PARAGRAPH_ALIGNMENT);
  HRESULT (STDMETHODCALLTYPE* SetWordWrapping)(IDWriteTextFormat*, DWRITE_WORD_WRAPPING);
} IDWriteTextFormatVtbl;

struct IDWriteTextFormat
{
  const IDWriteTextFormatVtbl* lpVtbl;
};

typedef struct IDWriteTextLayoutVtbl
{
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(IDWriteTextLayout*, REFIID, void**);
  ULONG (STDMETHODCALLTYPE* AddRef)(IDWriteTextLayout*);
  ULONG (STDMETHODCALLTYPE* Release)(IDWriteTextLayout*);
  void* TextFormatMethods[25];
  void* LayoutMethodsBeforeGetMetrics[32];
  HRESULT (STDMETHODCALLTYPE* GetMetrics)(IDWriteTextLayout*, DWRITE_TEXT_METRICS*);
} IDWriteTextLayoutVtbl;

struct IDWriteTextLayout
{
  const IDWriteTextLayoutVtbl* lpVtbl;
};

typedef struct IDWriteFactoryVtbl
{
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(IDWriteFactory*, REFIID, void**);
  ULONG (STDMETHODCALLTYPE* AddRef)(IDWriteFactory*);
  ULONG (STDMETHODCALLTYPE* Release)(IDWriteFactory*);
  void* MethodsBeforeCreateTextFormat[12];
  HRESULT (STDMETHODCALLTYPE* CreateTextFormat)(
    IDWriteFactory*,
    const WCHAR*,
    void*,
    DWRITE_FONT_WEIGHT,
    DWRITE_FONT_STYLE,
    DWRITE_FONT_STRETCH,
    FLOAT,
    const WCHAR*,
    IDWriteTextFormat**);
  void* CreateTypography;
  void* GetGdiInterop;
  HRESULT (STDMETHODCALLTYPE* CreateTextLayout)(
    IDWriteFactory*,
    const WCHAR*,
    UINT32,
    IDWriteTextFormat*,
    FLOAT,
    FLOAT,
    IDWriteTextLayout**);
} IDWriteFactoryVtbl;

struct IDWriteFactory
{
  const IDWriteFactoryVtbl* lpVtbl;
};

HRESULT WINAPI D2D1CreateFactory(
  D2D1_FACTORY_TYPE factoryType,
  REFIID iid,
  const void* factoryOptions,
  void** factory);
HRESULT WINAPI DWriteCreateFactory(
  DWRITE_FACTORY_TYPE factoryType,
  REFIID iid,
  IUnknown** factory);
