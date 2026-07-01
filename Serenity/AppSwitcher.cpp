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

struct RunningApp {
    HWND hwnd;
    std::wstring title;
};
std::vector<RunningApp> g_runningApps;

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

void DrawCards(HWND hWnd, HDC hdc, const AppConfig& config, const std::vector<RunningApp>& apps, int selectedIndex) {
    HBRUSH hDefaultBrush = CreateSolidBrush(RGB(40, 40, 40));
    HBRUSH hSelectBrush = CreateSolidBrush(RGB(80, 80, 255));

    for (size_t i = 0; i < apps.size(); ++i) {
        int yPos = config.appSwitcher.appBox.startY +
            (i * (config.appSwitcher.appBox.height + config.appSwitcher.appBox.padding));

        RECT cardRect = {
            config.appSwitcher.appBox.startX,
            yPos,
            config.appSwitcher.appBox.startX + config.appSwitcher.appBox.width,
            yPos + config.appSwitcher.appBox.height
        };

        FillRect(hdc, &cardRect, (i == selectedIndex) ? hSelectBrush : hDefaultBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, apps[i].title.c_str(), -1, &cardRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    }

    DeleteObject(hDefaultBrush);
    DeleteObject(hSelectBrush);
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
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}


void AppSwitcher_Init(HINSTANCE hInstance)
{
    g_config = SettingsManager::Load();

    if (g_runningApps.empty()) {
        g_runningApps.push_back({ nullptr, L"Mozilla Firefox" });
        g_runningApps.push_back({ nullptr, L"Microsoft Visual Studio" });
        g_runningApps.push_back({ nullptr, L"Discord" });
    }

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
    int alpha = fadeIn ? 0 : 255;

    int step = g_config.appSwitcher.blur.fadeStep;

    SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);

    while (fadeIn ? (alpha < 255) : (alpha > 0)) {
        alpha += fadeIn ? step : -step;
        if (alpha > 255) alpha = 255;
        if (alpha < 0) alpha = 0;

        SetLayeredWindowAttributes(hWnd, 0, (BYTE)alpha, LWA_ALPHA);
        Sleep(5);
    }
}
void AppSwitcher_Show() {
    if (!isVisible) {
        SetupModernBlur(hAppSwitcherWnd);

        SetLayeredWindowAttributes(hAppSwitcherWnd, 0, 0, LWA_ALPHA);
        ShowWindow(hAppSwitcherWnd, SW_SHOWNA);

        SetWindowLong(hAppSwitcherWnd, GWL_EXSTYLE,
            GetWindowLong(hAppSwitcherWnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);

        FadeWindow(hAppSwitcherWnd, true);
        isVisible = true;
    }
}

void AppSwitcher_Hide() {
    if (isVisible) {
        FadeWindow(hAppSwitcherWnd, false);

        ShowWindow(hAppSwitcherWnd, SW_HIDE);
        SetWindowLong(hAppSwitcherWnd, GWL_EXSTYLE,
            GetWindowLong(hAppSwitcherWnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT);

        isVisible = false;
    }
}