#include "InputHandler.h"
#include "Subsystems/Interface/AppSwitcher/AppSwitcher.h"
#include "Core/SettingsManager.h"

void HandleInput() {

    static ULONGLONG AppSwitcherHoldStart = 0;
    static AppConfig cfg = SettingsManager::Load();

    static bool wasTabHeld = false;
    static bool wasUpHeld = false;
    static bool wasDownHeld = false;

    static int tickCount = 0;
    if (tickCount++ % 100 == 0) {
    }

    bool isCapsHeld = (GetAsyncKeyState(VK_CAPITAL) & 0x8000) != 0;

    if (isCapsHeld) {
        if (AppSwitcherHoldStart == 0) AppSwitcherHoldStart = GetTickCount64();


        if (!AppSwitcher_IsVisible() && (GetTickCount64() - AppSwitcherHoldStart > (ULONGLONG)cfg.appSwitcher.blur.triggerDelay)) {
            
            AppSwitcher_Show();
        }

        if (AppSwitcher_IsVisible()) {
            bool isTabHeld = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
            bool isUpHeld = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
            bool isDownHeld = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;

            int scrollDelta = AppSwitcher_GetScrollDelta();

            if (scrollDelta > 0) {
                AppSwitcher_PrevApp();
            }
            else if (scrollDelta < 0) {
                AppSwitcher_NextApp();
            }

            if ((isTabHeld && !wasTabHeld) || (isDownHeld && !wasDownHeld)) {
                AppSwitcher_NextApp();
            }
            if (isUpHeld && !wasUpHeld) {
                AppSwitcher_PrevApp();
            }

            wasTabHeld = isTabHeld;
            wasUpHeld = isUpHeld;
            wasDownHeld = isDownHeld;
        }
    }
    else {
        if (AppSwitcherHoldStart != 0) {
            ULONGLONG duration = GetTickCount64() - AppSwitcherHoldStart;

            if (duration > (ULONGLONG)cfg.appSwitcher.blur.triggerDelay) {
                if (AppSwitcher_IsVisible()) {
                    
                    AppSwitcher_Commit();

                    AppSwitcher_Hide();
                }
            }
        }

        AppSwitcherHoldStart = 0;
        wasTabHeld = false;
        wasUpHeld = false;
        wasDownHeld = false;
    }
}