#pragma once
#ifdef __cplusplus
    #define EXTERN_C       extern "C"
    #define EXTERN_C_START extern "C" {
    #define EXTERN_C_END   }

    #if _MSC_VER >= 1900
        #define WIN_NOEXCEPT noexcept
    #else
        #define WIN_NOEXCEPT throw()
    #endif

    // 'noexcept' on typedefs is invalid prior to C++17
    #if _MSVC_LANG >= 201703
        #define WIN_NOEXCEPT_PFN noexcept
    #else
        #define WIN_NOEXCEPT_PFN
    #endif
#else
    #define EXTERN_C       extern
    #define EXTERN_C_START
    #define EXTERN_C_END
    #define WIN_NOEXCEPT
    #define WIN_NOEXCEPT_PFN
#endif
#if defined(_WIN32) || defined(_MPPC_)
#ifdef _68K_
#define STDMETHODCALLTYPE       __cdecl
#else
#define STDMETHODCALLTYPE       __stdcall
#endif
#define STDMETHODVCALLTYPE      __cdecl

#define STDAPICALLTYPE          __stdcall
#define STDAPIVCALLTYPE         __cdecl

#else

#define STDMETHODCALLTYPE       __export __stdcall
#define STDMETHODVCALLTYPE      __export __cdecl

#define STDAPICALLTYPE          __export __stdcall
#define STDAPIVCALLTYPE         __export __cdecl

#endif

typedef LONG HRESULT;
#define interface               struct
#define STDAPI                  EXTERN_C HRESULT STDAPICALLTYPE
typedef interface IDXGIDevice                 IDXGIDevice;
typedef interface IDXGIDevice2                 IDXGIDevice2;
typedef interface IDCompositionDevice                    IDCompositionDevice;
typedef interface IDCompositionTarget                    IDCompositionTarget;
typedef interface IDCompositionVisual2                    IDCompositionVisual2;
typedef interface IDCompositionDesktopDevice                    IDCompositionDesktopDevice;
typedef struct IDCompositionDevice { struct { void* tbl[]; }*v; } IDCompositionDevice;
typedef struct IDCompositionTarget { struct { void* tbl[]; }*v; } IDCompositionTarget;
typedef struct IDCompositionVisual { struct { void* tbl[]; }*v; } IDCompositionVisual;
typedef struct IDCompositionVisual2 { struct { void* tbl[]; }*v; } IDCompositionVisual2;
typedef struct IDCompositionDesktopDevice { struct { void* tbl[]; }*v; } IDCompositionDesktopDevice;
STDAPI DCompositionCreateDevice(_In_opt_ IDXGIDevice* dxgiDevice, _In_ REFIID iid, _Outptr_ void** dcompositionDevice);
STDAPI DCompositionCreateDevice3(_In_opt_ IDXGIDevice2* dxgiDevice2, _In_ REFIID iid, _Outptr_ void **dcompositionDevice);

static inline HRESULT IDCompositionDevice_Commit(IDCompositionDevice* this);
static inline HRESULT IDCompositionDevice_CreateTargetForHwnd(IDCompositionDevice* this, HWND hWnd, BOOL topmost, IDCompositionTarget** target);
static inline HRESULT IDCompositionDevice_CreateVisual(IDCompositionDevice* this, IDCompositionVisual** visual);
static inline HRESULT IDCompositionVisual_SetContent(IDCompositionDevice* this, IUnknown* content);
static inline HRESULT IDCompositionTarget_SetRoot(IDCompositionTarget* this, IDCompositionVisual* visual);

static inline HRESULT IDCompositionDevice_Commit(IDCompositionDevice* this) { return ((HRESULT(WINAPI*)(IDCompositionDevice*))this->v->tbl[3])(this); }
static inline HRESULT IDCompositionDevice_CreateTargetForHwnd(IDCompositionDevice* this, HWND hWnd, BOOL topmost, IDCompositionTarget** target) { return ((HRESULT(WINAPI*)(IDCompositionDevice*, HWND, BOOL, IDCompositionTarget**))this->v->tbl[6])(this, hWnd, topmost, target); }
static inline HRESULT IDCompositionDevice_CreateVisual(IDCompositionDevice* this, IDCompositionVisual** visual) { return ((HRESULT(WINAPI*)(IDCompositionDevice*, IDCompositionVisual**))this->v->tbl[7])(this, visual); }
static inline HRESULT IDCompositionVisual_SetContent(IDCompositionDevice* this, IUnknown* content) { return ((HRESULT(WINAPI*)(IDCompositionDevice*, IUnknown*))this->v->tbl[15])(this, content); }
static inline HRESULT IDCompositionTarget_SetRoot(IDCompositionTarget* this, IDCompositionVisual* visual) { return ((HRESULT(WINAPI*)(IDCompositionDevice*, IDCompositionVisual*))this->v->tbl[3])(this, visual); }
static inline HRESULT IDCompositionDevice_Release(IDCompositionDevice* this) { return ((HRESULT(WINAPI*)(IDCompositionDevice*))this->v->tbl[2])(this); }
static inline HRESULT IDCompositionTarget_Release(IDCompositionTarget* this) { return ((HRESULT(WINAPI*)(IDCompositionTarget*))this->v->tbl[2])(this); }
static inline HRESULT IDCompositionVisual_Release(IDCompositionVisual* this) { return ((HRESULT(WINAPI*)(IDCompositionVisual*))this->v->tbl[2])(this); }

static inline HRESULT IDCompositionDesktopDevice_CreateTargetForHwnd(IDCompositionDevice* this, HWND hWnd, BOOL topmost, IDCompositionTarget** target) { return ((HRESULT(WINAPI*)(IDCompositionDevice*, HWND, BOOL, IDCompositionTarget**))this->v->tbl[24])(this, hWnd, topmost, target); }