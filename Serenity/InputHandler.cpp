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
        if (AppSwitcherHoldStart != 0) {
            ULONGLONG duration = GetTickCount64() - AppSwitcherHoldStart;

            if (duration > (ULONGLONG)cfg.triggerDelay) {
                if (AppSwitcher_IsVisible()) {
                    AppSwitcher_Hide();

                    if (GetKeyState(VK_CAPITAL) & 0x0001) {
                        keybd_event(VK_CAPITAL, 0x3A, KEYEVENTF_EXTENDEDKEY, 0);
                        keybd_event(VK_CAPITAL, 0x3A, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
                    }
                }
            }
        }
        AppSwitcherHoldStart = 0;
    }
}