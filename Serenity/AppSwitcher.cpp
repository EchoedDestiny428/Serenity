#include "AppSwitcher.h"
#include "OverlayEngine.h"
#include <vector>
#include <string>
#include "SettingsManager.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "msimg32.lib")

// Global Variables

static HWND hAppSwitcherWnd = nullptr;
static bool isVisible = false;
AppConfig g_config;
int g_selectedIndex = 0;
static int g_mouseScrollDelta = 0;
static int g_lastHoveredIndex = -1;
bool g_isFadingIn = false;

static HWND g_hProxyWnd = nullptr;
static HTHUMBNAIL g_hThumbFade = nullptr;
static ULONGLONG g_bgFadeStart = 0;
static HWND g_pendingBgHwnd = nullptr;

struct RunningApp {
    HWND hwnd;
    std::wstring title;
    HICON hIcon;
};
std::vector<RunningApp> g_runningApps;

// Get Running Applications
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;

    int cloaked = 0;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) return TRUE;

    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

    WCHAR buffer[256];
    if (GetWindowTextW(hwnd, buffer, 256) == 0) return TRUE;
    std::wstring title(buffer);

    if (hwnd == hAppSwitcherWnd) return TRUE;
    if (title == L"Program Manager") return TRUE;

    DWORD_PTR dwResult = 0;
    HICON hIcon = nullptr;

    if (SendMessageTimeoutW(hwnd, WM_GETICON, ICON_BIG, 0,
        SMTO_ABORTIFHUNG | SMTO_NORMAL, 20, &dwResult)) {
        hIcon = (HICON)dwResult;
    }

    if (!hIcon && SendMessageTimeoutW(hwnd, WM_GETICON, ICON_SMALL, 0,
        SMTO_ABORTIFHUNG | SMTO_NORMAL, 20, &dwResult)) {
        hIcon = (HICON)dwResult;
    }

    if (!hIcon) hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICON);
    if (!hIcon) hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICONSM);
    if (!hIcon) hIcon = LoadIcon(NULL, IDI_APPLICATION);

    auto* apps = reinterpret_cast<std::vector<RunningApp>*>(lParam);
    apps->push_back({ hwnd, title, hIcon });

    return TRUE;
}

// Wrapper
void RefreshRunningApps() {
    g_runningApps.clear();
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&g_runningApps));
}

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

// Background Delay Engine
void AppSwitcher_ResetHoverTimer(HWND hWnd) {
    KillTimer(hWnd, 1);

    int delay = g_config.appSwitcher.blur.hoverDelay;

    SetTimer(hWnd, 1, delay, NULL);
}

// Cards
void DrawCards(HWND hWnd, HDC hdc, const AppConfig& config, const std::vector<RunningApp>& apps, int selectedIndex) {
    HFONT hFont = CreateFontW(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    HBRUSH hNormalBrush = CreateSolidBrush(RGB(35, 35, 40));
    HBRUSH hSelectBrush = CreateSolidBrush(RGB(50, 50, 60));
    HPEN   hNullPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
    HPEN   hAccentPen = CreatePen(PS_SOLID, 2, RGB(0, 120, 215));

    for (size_t i = 0; i < apps.size(); ++i) {
        int yPos = config.appSwitcher.appBox.startY +
            (i * (config.appSwitcher.appBox.height + config.appSwitcher.appBox.padding));

        RECT cardRect = { config.appSwitcher.appBox.startX, yPos,
                          config.appSwitcher.appBox.startX + config.appSwitcher.appBox.width,
                          yPos + config.appSwitcher.appBox.height };

        if ((int)i == selectedIndex) {
            SelectObject(hdc, hSelectBrush);
            SelectObject(hdc, hAccentPen);
        }
        else {
            SelectObject(hdc, hNormalBrush);
            SelectObject(hdc, hNullPen);
        }

        RoundRect(hdc, cardRect.left, cardRect.top, cardRect.right, cardRect.bottom, 12, 12);

        if (apps[i].hIcon) {
            int iconSize = 32;
            int iconY = cardRect.top + ((config.appSwitcher.appBox.height - iconSize) / 2);
            DrawIconEx(hdc, cardRect.left + 16, iconY, apps[i].hIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
        }

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(240, 240, 240));

        RECT textRect = cardRect;
        textRect.left += 64;
        textRect.right -= 16;

        DrawTextW(hdc, apps[i].title.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    }

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteObject(hNormalBrush);
    DeleteObject(hSelectBrush);
    DeleteObject(hNullPen);
    DeleteObject(hAccentPen);
}

// Window Procedure
LRESULT CALLBACK AppSwitcher_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        if (g_bgFadeStart != 0 && g_hThumbFade) {
            float t = (float)(GetTickCount64() - g_bgFadeStart) / 150.0f;

            if (g_bgFadeStart != 0 && g_hThumbFade) {
                float t = (float)(GetTickCount64() - g_bgFadeStart) / 150.0f;

                if (t >= 1.0f) {
                    SetWindowPos(g_pendingBgHwnd, hWnd, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);

                    DwmUnregisterThumbnail(g_hThumbFade);
                    g_hThumbFade = nullptr;
                    g_bgFadeStart = 0;
                    g_lastHoveredIndex = g_selectedIndex;

                    ShowWindow(g_hProxyWnd, SW_HIDE);
                }
                else {
                    DWM_THUMBNAIL_PROPERTIES props = { 0 };
                    props.dwFlags = DWM_TNP_OPACITY;
                    props.opacity = (BYTE)(t * 255.0f);
                    DwmUpdateThumbnailProperties(g_hThumbFade, &props);
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
        }

        DrawCards(hWnd, hdc, g_config, g_runningApps, g_selectedIndex);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_TIMER: {
        if (wParam == 1) {
            KillTimer(hWnd, 1);

            if (isVisible && g_selectedIndex >= 0 && g_selectedIndex < (int)g_runningApps.size()) {
                if (g_lastHoveredIndex == g_selectedIndex) return 0;

                g_pendingBgHwnd = g_runningApps[g_selectedIndex].hwnd;

                if (g_hThumbFade) {
                    DwmUnregisterThumbnail(g_hThumbFade);
                    g_hThumbFade = nullptr;
                }

                SetWindowPos(g_hProxyWnd, hWnd, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

                if (SUCCEEDED(DwmRegisterThumbnail(g_hProxyWnd, g_pendingBgHwnd, &g_hThumbFade))) {
                    DWM_THUMBNAIL_PROPERTIES props = { 0 };
                    props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
                    GetClientRect(g_hProxyWnd, &props.rcDestination);
                    props.fVisible = TRUE;
                    props.opacity = 0;
                    DwmUpdateThumbnailProperties(g_hThumbFade, &props);

                    g_bgFadeStart = GetTickCount64();
                    InvalidateRect(hWnd, NULL, FALSE);
                }
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

// Visibility
bool AppSwitcher_IsVisible()
{
    return isVisible;
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
        KillTimer(hAppSwitcherWnd, 1); // Safety cancel
        FadeWindow(hAppSwitcherWnd, false);
    }
}

// Interactivity
int AppSwitcher_GetScrollDelta() {
    int delta = g_mouseScrollDelta;
    g_mouseScrollDelta = 0;
    return delta;
}

// Selection Nav
void AppSwitcher_NextApp() {
    if (g_runningApps.empty()) return;
    g_selectedIndex++;
    if (g_selectedIndex >= (int)g_runningApps.size()) g_selectedIndex = 0;

    AppSwitcher_ResetHoverTimer(hAppSwitcherWnd);
    InvalidateRect(hAppSwitcherWnd, NULL, FALSE);
}

void AppSwitcher_PrevApp() {
    if (g_runningApps.empty()) return;
    g_selectedIndex--;
    if (g_selectedIndex < 0) g_selectedIndex = (int)g_runningApps.size() - 1;

    AppSwitcher_ResetHoverTimer(hAppSwitcherWnd);
    InvalidateRect(hAppSwitcherWnd, NULL, FALSE);
}

// Context Actions
void AppSwitcher_Commit() {
    if (g_runningApps.empty() || g_selectedIndex < 0 || g_selectedIndex >= (int)g_runningApps.size()) return;

    HWND target = g_runningApps[g_selectedIndex].hwnd;
    if (IsIconic(target)) {
        ShowWindowAsync(target, SW_RESTORE);
    }
    else {
        ShowWindowAsync(target, SW_SHOW);
    }
    SetForegroundWindow(target);

    if (GetKeyState(VK_CAPITAL) & 0x0001) {
        keybd_event(VK_CAPITAL, 0x3a, KEYEVENTF_EXTENDEDKEY | 0, 0);
        keybd_event(VK_CAPITAL, 0x3a, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }
}