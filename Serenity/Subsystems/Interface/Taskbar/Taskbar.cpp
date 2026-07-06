#include "Taskbar.h"
#include "../../Graphics/GraphicsEngine.h"
#include "../../Windowing/OverlayEngine.h"
#include <d2d1.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

static HWND hTaskbarWnd = nullptr;
static int taskbarHeight = 56;
static int taskbarWidth = 600;

static int dockMargin = 32; 

static bool g_isVisible = false;
static bool g_isAnimating = false;

void SlideTaskbar(HWND hWnd, bool slideUp) {
    g_isAnimating = true;

    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int visibleY = screenH - taskbarHeight - dockMargin;
    int hiddenY = screenH + 10;

    ULONGLONG startTime = GetTickCount64();
    float duration = 300.0f;

    RECT rc;
    GetWindowRect(hWnd, &rc);
    float startY = (float)rc.top;
    float endY = slideUp ? (float)visibleY : (float)hiddenY;

    float t = 0.0f;
    while (t < 1.0f) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        ULONGLONG elapsed = GetTickCount64() - startTime;
        t = (float)elapsed / duration;
        if (t > 1.0f) t = 1.0f;

        float eased = 0.0f;
        if (slideUp) {
            float inv = 1.0f - t;
            eased = 1.0f - (inv * inv * inv);
        } else {
            eased = t * t * t;
        }

        int currentY = (int)(startY + (endY - startY) * eased);
        
        SetWindowPos(hWnd, HWND_TOPMOST, rc.left, currentY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);

        DwmFlush();
    }

    g_isAnimating = false;
}

LRESULT CALLBACK Taskbar_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_ERASEBKGND: {
            return 1; 
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);

            auto target = Graphics::GetRenderTarget();
            if (target) {
                target->BeginDraw();

                target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

                ID2D1SolidColorBrush* bgBrush = nullptr;
                ID2D1SolidColorBrush* borderBrush = nullptr;

                target->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.08f, 0.08f, 0.8f), &bgBrush);
                target->CreateSolidColorBrush(D2D1::ColorF(0.7f, 0.3f, 1.0f, 1.0f), &borderBrush);

                if (bgBrush && borderBrush) {
                    D2D1_SIZE_F size = target->GetSize();

                    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
                        D2D1::RectF(1.0f, 1.0f, size.width - 1.0f, size.height - 1.0f),
                        12.0f, 12.0f
                    );

                    target->FillRoundedRectangle(&rr, bgBrush);
                    target->DrawRoundedRectangle(&rr, borderBrush, 1.0f);
                    bgBrush->Release();
                    borderBrush->Release();
                }

                // TODO: DrawTaskbarIcons() goes here

                target->EndDraw();
            }

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                if (g_isAnimating) return 0;

                POINT pt;
                GetCursorPos(&pt);

                int screenW = GetSystemMetrics(SM_CXSCREEN);
                int screenH = GetSystemMetrics(SM_CYSCREEN);

                int left = (screenW - taskbarWidth) / 2;
                int right = left + taskbarWidth;
                int visibleY = screenH - taskbarHeight - dockMargin;
                
                int triggerTop = visibleY - 40; 

                bool inZone = (pt.x >= left && pt.x <= right && pt.y >= triggerTop);

                if (inZone && !g_isVisible) {
                    g_isVisible = true;
                    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                    SlideTaskbar(hWnd, true);
                }
                else if (!inZone && g_isVisible) {
                    g_isVisible = false;
                    SlideTaskbar(hWnd, false);
                }
            }
            return 0;
        }

        case WM_NCHITTEST: {
            return HTCLIENT;
        }

        case WM_DESTROY: {
            KillTimer(hWnd, 1);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void Taskbar_Init(HINSTANCE hInstance) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    int startY = screenH + 10;
    g_isVisible = false;

    OverlaySettings settings = { 0 };
    settings.className = L"SerenityTaskbarClass";
    settings.width = taskbarWidth;
    settings.height = taskbarHeight;
    settings.x = (screenW - taskbarWidth) / 2;
    settings.y = startY;
    settings.opacity = 255;

    hTaskbarWnd = OverlayEngine_Create(hInstance, settings, Taskbar_WndProc);

    SetWindowLong(hTaskbarWnd, GWL_EXSTYLE, GetWindowLong(hTaskbarWnd, GWL_EXSTYLE) & ~(WS_EX_TRANSPARENT | WS_EX_LAYERED));

    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hTaskbarWnd, &margins);
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