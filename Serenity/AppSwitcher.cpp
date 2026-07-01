#include "AppSwitcher.h"
#include "OverlayEngine.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

static HWND hAppSwitcherWnd = nullptr;
static bool isVisible = false;



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


void AppSwitcher_Init(HINSTANCE hInstance)
{
    OverlaySettings settings = { 0 };
    settings.className = L"AppSwitcherClass";

    settings.width = GetSystemMetrics(SM_CXSCREEN);
    settings.height = GetSystemMetrics(SM_CYSCREEN);
    settings.x = 0;
    settings.y = 0;
    settings.opacity = 255;

    settings.styleCallback = SetupModernBlur;

    hAppSwitcherWnd = OverlayEngine_Create(hInstance, settings, DefWindowProcW);
}

bool AppSwitcher_IsVisible()
{
    return isVisible;
}

void FadeWindow(HWND hWnd, bool fadeIn) {
    int alpha = fadeIn ? 0 : 255;
    int step = 15;

    SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);

    while (fadeIn ? (alpha < 255) : (alpha > 0)) {
        alpha += fadeIn ? step : -step;
        if (alpha > 255) alpha = 255;
        if (alpha < 0) alpha = 0;

        SetLayeredWindowAttributes(hWnd, 0, (BYTE)alpha, LWA_ALPHA);
        Sleep(10);
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