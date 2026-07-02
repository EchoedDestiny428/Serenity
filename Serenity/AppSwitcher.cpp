#include "AppSwitcher.h"
#include "OverlayEngine.h"
#include <vector>
#include <string>
#include "SettingsManager.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// Global Variables

static HWND hAppSwitcherWnd = nullptr;
static bool isVisible = false;
AppConfig g_config;
int g_selectedIndex = 0;

bool g_isFadingIn = false;

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

// wrapper
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

// Cards

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

        if (i == selectedIndex) {
            SelectObject(hdc, hSelectBrush);
            SelectObject(hdc, hAccentPen);
        }
        else {
            SelectObject(hdc, hNormalBrush);
            SelectObject(hdc, hNullPen);
        }

        RoundRect(hdc, cardRect.left, cardRect.top, cardRect.right, cardRect.bottom, 12, 12);

        // Draw Icon
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
            DrawCards(hWnd, hdc, g_config, g_runningApps, g_selectedIndex);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
        case WM_SETCURSOR: {
            SetCursor(LoadCursorW(NULL, IDC_ARROW));
            return TRUE;
        }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}


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

    hAppSwitcherWnd = OverlayEngine_Create(hInstance, settings, AppSwitcher_WndProc);
}

bool AppSwitcher_IsVisible()
{
    return isVisible;
}


// Blur
void FadeWindow(HWND hWnd, bool fadeIn) {
    g_isFadingIn = fadeIn;
    ULONGLONG startTime = GetTickCount64();

    // Calculate duration based on your INI settings
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

void AppSwitcher_Show() {
    if (!isVisible) {
        RefreshRunningApps();
        g_selectedIndex = 0;

        SetupModernBlur(hAppSwitcherWnd);

        SetLayeredWindowAttributes(hAppSwitcherWnd, 0, 0, LWA_ALPHA);
        ShowWindow(hAppSwitcherWnd, SW_SHOWNA);

        SetWindowLong(hAppSwitcherWnd, GWL_EXSTYLE,
            GetWindowLong(hAppSwitcherWnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);

        isVisible = true;
        FadeWindow(hAppSwitcherWnd, true);
    }
}

void AppSwitcher_Hide() {
    if (isVisible) {
        FadeWindow(hAppSwitcherWnd, false);
    }
}