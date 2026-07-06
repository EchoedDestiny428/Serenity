#include "AppSwitcher.h"
#include "Render.h"
#include "Logic.h"
#include "State.h"
#include "Graphics/GraphicsEngine.h"

#include "Subsystems/Windowing/OverlayEngine.h"
#include <vector>
#include <string>
#include "Core/SettingsManager.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "msimg32.lib")



enum ACCENT_STATE {
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4
};

struct ACCENT_POLICY {
    int AccentState;
    int AccentFlags;
    int GradientColor;
    int AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    int Attrib;
    PVOID pvData;
    SIZE_T cbData;
};

typedef BOOL(WINAPI* pfnSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

// Setup
void __stdcall SetupModernBlur(HWND hWnd) {
    auto SetWindowCompositionAttribute = (pfnSetWindowCompositionAttribute)GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute");

    if (SetWindowCompositionAttribute) {
        ACCENT_POLICY policy = {};
        policy.AccentState = 4;

        WINDOWCOMPOSITIONATTRIBDATA data = {};
        data.Attrib = 19;
        data.pvData = &policy;
        data.cbData = sizeof(policy);

        SetWindowCompositionAttribute(hWnd, &data);
    }
}


// Window Procedure
LRESULT CALLBACK AppSwitcher_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);

            auto target = Graphics::GetRenderTarget(hWnd);
            if (target) {
                target->BeginDraw();
                target->Clear(D2D1::ColorF(0, 0, 0, 0));
                DrawCards(target, g_config, g_runningApps, g_selectedIndex);
                target->EndDraw();
            }

            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_TIMER: {
            if (wParam == 1) { // --- HOVER DELAY TRIGGERED ---
                KillTimer(hWnd, 1);

                if (isVisible && g_selectedIndex >= 0 && g_selectedIndex < (int)g_runningApps.size()) {
                    if (g_lastHoveredIndex == g_selectedIndex) return 0;

                    if (g_hThumbFade) {
                        DwmUnregisterThumbnail(g_hThumbFade);
                        g_hThumbFade = nullptr;
                    }

                    g_pendingBgHwnd = g_runningApps[g_selectedIndex].hwnd;

                    SetWindowPos(g_hProxyWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

                    if (SUCCEEDED(DwmRegisterThumbnail(g_hProxyWnd, g_pendingBgHwnd, &g_hThumbFade))) {
                        DWM_THUMBNAIL_PROPERTIES props = { 0 };
                        props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
                        GetClientRect(g_hProxyWnd, &props.rcDestination);
                        props.fVisible = TRUE;
                        props.opacity = 0;
                        DwmUpdateThumbnailProperties(g_hThumbFade, &props);

                        g_bgFadeStart = GetTickCount64();
                        g_bgSwapTriggered = false;

                        SetTimer(hWnd, 2, 16, NULL);
                    }
                }
            }
            else if (wParam == 2) { // --- FADE ANIMATION ---
                if (g_bgFadeStart != 0 && g_hThumbFade) {
                    ULONGLONG elapsed = GetTickCount64() - g_bgFadeStart;
                    float t = (float)elapsed / 150.0f;

                    if (t >= 1.0f && !g_bgSwapTriggered) {
                        SetWindowPos(g_pendingBgHwnd, g_hProxyWnd, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
                        g_bgSwapTriggered = true;

                        DWM_THUMBNAIL_PROPERTIES props = { 0 };
                        props.dwFlags = DWM_TNP_OPACITY;
                        props.opacity = 255;
                        DwmUpdateThumbnailProperties(g_hThumbFade, &props);
                    }
                    else if (g_bgSwapTriggered && elapsed > 250) {
                        DwmUnregisterThumbnail(g_hThumbFade);
                        g_hThumbFade = nullptr;
                        g_bgFadeStart = 0;
                        g_lastHoveredIndex = g_selectedIndex;
                        ShowWindow(g_hProxyWnd, SW_HIDE);

                        KillTimer(hWnd, 2);
                    }
                    else if (!g_bgSwapTriggered) {
                        DWM_THUMBNAIL_PROPERTIES props = { 0 };
                        props.dwFlags = DWM_TNP_OPACITY;
                        props.opacity = (BYTE)(t * 255.0f);
                        DwmUpdateThumbnailProperties(g_hThumbFade, &props);
                    }
                }
                else {
                    KillTimer(hWnd, 2);
                }
            }
            return 0;
        }
        case WM_DESTROY: {
            KillTimer(hWnd, 1);
            if (g_hThumbFade) DwmUnregisterThumbnail(g_hThumbFade);
            if (g_hProxyWnd) DestroyWindow(g_hProxyWnd);

            PostQuitMessage(0);
            return 0;
        }
        case WM_SETCURSOR: {
            SetCursor(LoadCursorW(NULL, IDC_ARROW));
            return TRUE;
        }
        case WM_MOUSEWHEEL: {
            if (isVisible) {
                g_mouseScrollDelta += GET_WHEEL_DELTA_WPARAM(wParam);
            }
            return 0;
        }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// Init
void AppSwitcher_Init(HINSTANCE hInstance)
{
    g_config = SettingsManager::Load();

    OverlaySettings settings = { 0 };
    settings.className = L"AppSwitcherClass";

    settings.width = GetSystemMetrics(SM_CXSCREEN);
    settings.height = GetSystemMetrics(SM_CYSCREEN);
    settings.x = 0;
    settings.y = 0;
    settings.opacity = 255;

    settings.styleCallback = SetupModernBlur;

    WNDCLASSEXW proxyWc = { sizeof(WNDCLASSEXW) };
    proxyWc.lpfnWndProc = DefWindowProcW;
    proxyWc.hInstance = hInstance;
    proxyWc.lpszClassName = L"AppSwitcherProxyClass";
    proxyWc.hbrBackground = CreateSolidBrush(RGB(1, 1, 1));
    RegisterClassExW(&proxyWc);

    g_hProxyWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        L"AppSwitcherProxyClass", L"", WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hInstance, nullptr
    );
    SetLayeredWindowAttributes(g_hProxyWnd, RGB(1, 1, 1), 0, LWA_COLORKEY);

    hAppSwitcherWnd = OverlayEngine_Create(hInstance, settings, AppSwitcher_WndProc);
}

// Blur
void FadeWindow(HWND hWnd, bool fadeIn) {
    g_isFadingIn = fadeIn;
    ULONGLONG startTime = GetTickCount64();

    int safeStep = g_config.appSwitcher.blur.fadeStep;
    if (safeStep <= 0) safeStep = 15;
    float duration = (255.0f / (float)safeStep) * 10.0f;

    SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);

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
        if (fadeIn) {
            float inv = 1.0f - t;
            eased = 1.0f - (inv * inv * inv);
        }
        else {
            eased = t * t * t;
        }

        int alpha = fadeIn ? (int)(eased * 255.0f) : (int)((1.0f - eased) * 255.0f);
        if (alpha > 255) alpha = 255;
        if (alpha < 0) alpha = 0;

        SetLayeredWindowAttributes(hWnd, 0, (BYTE)alpha, LWA_ALPHA);

        DwmFlush();
    }

    if (!fadeIn) {
        ShowWindow(hWnd, SW_HIDE);
        SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
        isVisible = false;
    }
}

// Show Window
void AppSwitcher_Show() {
    if (!isVisible) {
        RefreshRunningApps();
        g_selectedIndex = 0;
        g_lastHoveredIndex = -1;

        Graphics::Initialize(hAppSwitcherWnd);

        SetupModernBlur(hAppSwitcherWnd);

        SetLayeredWindowAttributes(hAppSwitcherWnd, 0, 0, LWA_ALPHA);
        ShowWindow(hAppSwitcherWnd, SW_SHOWNA);

        SetWindowLong(hAppSwitcherWnd, GWL_EXSTYLE,
            GetWindowLong(hAppSwitcherWnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);

        isVisible = true;
        FadeWindow(hAppSwitcherWnd, true);
    }
}

// Hide Window
void AppSwitcher_Hide() {
    if (isVisible) {
        KillTimer(hAppSwitcherWnd, 1);
        KillTimer(hAppSwitcherWnd, 2);

        if (g_hThumbFade) {
            DwmUnregisterThumbnail(g_hThumbFade);
            g_hThumbFade = nullptr;
        }
        if (g_hProxyWnd) {
            ShowWindow(g_hProxyWnd, SW_HIDE);
        }

        FadeWindow(hAppSwitcherWnd, false);
    }
}

// Interactivity
int AppSwitcher_GetScrollDelta() {
    int delta = g_mouseScrollDelta;
    g_mouseScrollDelta = 0;
    return delta;
}

