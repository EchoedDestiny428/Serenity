#include "AppSwitcher.h"
#include "OverlayEngine.h"

static HWND hAppSwitcherWnd = nullptr;
static bool isVisible = false;


void AppSwitcher_Init(HINSTANCE hInstance)
{
    OverlaySettings settings;
    settings.className = L"AppSwitcherClass";
    settings.width = 400;
    settings.height = 300;
    settings.opacity = 220;

    hAppSwitcherWnd = OverlayEngine_Create(hInstance, settings, DefWindowProcW);
}

bool AppSwitcher_IsVisible()
{
    return isVisible;
}

void AppSwitcher_Show()
{
    if (!isVisible) {
        ShowWindow(hAppSwitcherWnd, SW_SHOWNOACTIVATE);
        isVisible = true;
    }
}

void AppSwitcher_Hide()
{
	if (isVisible) {
		ShowWindow(hAppSwitcherWnd, SW_HIDE);
		isVisible = false;
	}
}