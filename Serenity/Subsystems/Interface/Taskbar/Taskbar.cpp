#include "Taskbar.h"
#include "Graphics/GraphicsEngine.h"
#include "Windowing/OverlayEngine.h"
#include <d2d1.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <unordered_map>
#include <wincodec.h>

#include "AppSwitcher/State.h"
#include "AppSwitcher/Logic.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "windowscodecs.lib")

static IWICImagingFactory* pWICFactory = nullptr;
static std::unordered_map<HWND, ID2D1Bitmap*> g_iconCache;
static HWND hTaskbarWnd = nullptr;
static int taskbarHeight = 56;
static int taskbarWidth = 600;
static int dockMargin = 32; 
static bool g_isVisible = false;
static bool g_isAnimating = false;

// --- Icon Helpers ---
void ClearIconCache() {
    for (auto& pair : g_iconCache) if (pair.second) pair.second->Release();
    g_iconCache.clear();
}

HICON GetAppIcon(HWND hwnd) {
    HICON hIcon = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0);
    if (!hIcon) hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICON);
    if (!hIcon) hIcon = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0);
    if (!hIcon) hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICONSM);
    return hIcon ? hIcon : LoadIconW(NULL, IDI_APPLICATION);
}

ID2D1Bitmap* CreateD2DBitmapFromIcon(ID2D1HwndRenderTarget* target, HICON hIcon) {
    if (!pWICFactory) {
        CoInitialize(NULL);
        CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pWICFactory));
    }
    if (!pWICFactory || !hIcon) return nullptr;

    IWICBitmap* wic = nullptr;
    if (FAILED(pWICFactory->CreateBitmapFromHICON(hIcon, &wic))) return nullptr;

    IWICFormatConverter* conv = nullptr;
    ID2D1Bitmap* bmp = nullptr;
    if (SUCCEEDED(pWICFactory->CreateFormatConverter(&conv))) {
        if (SUCCEEDED(conv->Initialize(wic, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeMedianCut)))
            target->CreateBitmapFromWicBitmap(conv, NULL, &bmp);
        conv->Release();
    }
    wic->Release();
    return bmp;
}

void SlideTaskbar(HWND hWnd, bool slideUp) {
    g_isAnimating = true;
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int visibleY = screenH - taskbarHeight - dockMargin;
    int hiddenY = screenH + 10;
    ULONGLONG startTime = GetTickCount64();
    RECT rc; GetWindowRect(hWnd, &rc);
    float startY = (float)rc.top;
    float endY = slideUp ? (float)visibleY : (float)hiddenY;
    float t = 0.0f;
    while (t < 1.0f) {
        MSG msg; while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        ULONGLONG elapsed = GetTickCount64() - startTime;
        t = min(1.0f, (float)elapsed / 300.0f);
        float inv = 1.0f - t;
        float eased = slideUp ? (1.0f - (inv * inv * inv)) : (t * t * t);
        SetWindowPos(hWnd, HWND_TOPMOST, rc.left, (int)(startY + (endY - startY) * eased), 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        DwmFlush();
    }
    g_isAnimating = false;
}

LRESULT CALLBACK Taskbar_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_ERASEBKGND: return 1;
        case WM_SETCURSOR: SetCursor(LoadCursorW(NULL, IDC_ARROW)); return TRUE;
        case WM_PAINT: {
            PAINTSTRUCT ps; BeginPaint(hWnd, &ps);
            auto target = Graphics::GetRenderTarget(hWnd);
            if (target) {
                target->BeginDraw();
                target->Clear(D2D1::ColorF(0, 0, 0, 0));
                ID2D1SolidColorBrush *bg, *brd;
                target->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.08f, 0.08f, 0.9f), &bg);
                target->CreateSolidColorBrush(D2D1::ColorF(0.7f, 0.3f, 1.0f, 1.0f), &brd);
                D2D1_SIZE_F s = target->GetSize();
                D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(1, 1, s.width - 1, s.height - 1), 12, 12);
                target->FillRoundedRectangle(&rr, bg);
                target->DrawRoundedRectangle(&rr, brd, 1.0f);
                
                float sz = 32.0f, spc = 20.0f, tw = (g_runningApps.size() * sz) + ((g_runningApps.size() - 1) * spc);
                float x = (s.width - tw) / 2.0f, y = (s.height - sz) / 2.0f;
                for (auto& app : g_runningApps) {

                    if (g_iconCache.find(app.hwnd) == g_iconCache.end()) {
                        g_iconCache[app.hwnd] = CreateD2DBitmapFromIcon(target, app.hIcon);
                    }

                    if (g_iconCache[app.hwnd]) {
                        target->DrawBitmap(g_iconCache[app.hwnd], D2D1::RectF(x, y, x + sz, y + sz));
                    }
                    x += sz + spc;
                }
                bg->Release(); brd->Release();
                target->EndDraw();
            }
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int mx = GET_X_LPARAM(lParam), my = GET_Y_LPARAM(lParam);
            float sz = 32.0f, spc = 20.0f, tw = (g_runningApps.size() * sz) + ((g_runningApps.size() - 1) * spc);
            float x = (taskbarWidth - tw) / 2.0f, y = (taskbarHeight - sz) / 2.0f;
            for (auto& app : g_runningApps) {
                if (mx >= x && mx <= x + sz && my >= y && my <= y + sz) {
                    if (IsIconic(app.hwnd)) ShowWindow(app.hwnd, SW_RESTORE);
                    SetForegroundWindow(app.hwnd);
                    g_isVisible = false; SlideTaskbar(hWnd, false);
                    break;
                }
                x += sz + spc;
            }
            return 0;
        }
        case WM_TIMER: {
            if (wParam == 1 && !g_isAnimating) {
                POINT pt; GetCursorPos(&pt);
                int visibleY = GetSystemMetrics(SM_CYSCREEN) - taskbarHeight - dockMargin;
                bool inZone = (pt.x > (GetSystemMetrics(SM_CXSCREEN) - taskbarWidth)/2 && pt.x < (GetSystemMetrics(SM_CXSCREEN) + taskbarWidth)/2 && pt.y > visibleY - 40);
                if (inZone != g_isVisible) {
                    if (inZone) { RefreshRunningApps(); ClearIconCache(); }
                    g_isVisible = inZone; SlideTaskbar(hWnd, inZone);
                }
            }
            return 0;
        }
        case WM_DESTROY: { KillTimer(hWnd, 1); ClearIconCache(); if(pWICFactory) pWICFactory->Release(); PostQuitMessage(0); return 0; }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void Taskbar_Init(HINSTANCE hInstance) {
    OverlaySettings settings = { L"SerenityTaskbarClass", taskbarWidth, taskbarHeight, (GetSystemMetrics(SM_CXSCREEN)-taskbarWidth)/2, GetSystemMetrics(SM_CYSCREEN)+10, 255 };
    hTaskbarWnd = OverlayEngine_Create(hInstance, settings, Taskbar_WndProc);
    SetWindowLong(hTaskbarWnd, GWL_EXSTYLE, GetWindowLong(hTaskbarWnd, GWL_EXSTYLE) & ~(WS_EX_TRANSPARENT | WS_EX_LAYERED));
    MARGINS m = {-1,-1,-1,-1}; DwmExtendFrameIntoClientArea(hTaskbarWnd, &m);
}

void Taskbar_Show() {
    if (hTaskbarWnd) {
        Graphics::Initialize(hTaskbarWnd);
        ShowWindow(hTaskbarWnd, SW_SHOWNA);
        SetTimer(hTaskbarWnd, 1, 16, NULL);
        InvalidateRect(hTaskbarWnd, NULL, FALSE);
    }
}

void Taskbar_Hide() {
    if (hTaskbarWnd) {
        KillTimer(hTaskbarWnd, 1);
        ShowWindow(hTaskbarWnd, SW_HIDE);
    }
}