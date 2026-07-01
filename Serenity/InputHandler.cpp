#include "InputHandler.h"
#include "AppSwitcher.h"
#include "SettingsManager.h"

void HandleInput() {

    // AppSwitcher

    static ULONGLONG AppSwitcherHoldStart = 0;
    static AppConfig cfg = SettingsManager::Load();

    bool isCapsHeld = (GetAsyncKeyState(VK_CAPITAL) & 0x8000) != 0;

    if (isCapsHeld) {
        if (AppSwitcherHoldStart == 0) AppSwitcherHoldStart = GetTickCount64();

        if (!AppSwitcher_IsVisible() && (GetTickCount64() - AppSwitcherHoldStart > (ULONGLONG)cfg.triggerDelay)) {
            AppSwitcher_Show();
        }
    }
    else {
        if (AppSwitcher_IsVisible()) {
            AppSwitcher_Hide();
        }
        AppSwitcherHoldStart = 0;
    }
}