#include "InputHandler.h"
#include "AppSwitcher.h"

void HandleInput() {
    
    //AppSwitcher

    static ULONGLONG capsPressStartTime = 0;
    bool isCapsHeld = (GetAsyncKeyState(VK_CAPITAL) & 0x8000) != 0;

    if (isCapsHeld) {
        if (capsPressStartTime == 0) capsPressStartTime = GetTickCount64();

        if (!AppSwitcher_IsVisible() && (GetTickCount64() - capsPressStartTime > 500)) {
            AppSwitcher_Show();
        }
    }
    else {
        if (AppSwitcher_IsVisible()) {
            AppSwitcher_Hide();
        }
        capsPressStartTime = 0;
    }

    // 
}