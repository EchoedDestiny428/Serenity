#include "Logic.h"
#include <dwmapi.h>
#include "State.h"

std::vector<RunningApp> g_runningApps;


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

    std::wstring cleanTitle = title;
    size_t lastPos = std::wstring::npos;
    size_t sepLen = 0;

    const wchar_t* separators[] = { L" - ", L" \x2013 ", L" \x2014 ", L" \x2015 ", L" | " };

    for (const wchar_t* sep : separators) {
        size_t pos = cleanTitle.rfind(sep);
        if (pos != std::wstring::npos) {
            if (lastPos == std::wstring::npos || pos > lastPos) {
                lastPos = pos;
                sepLen = wcslen(sep);
            }
        }
    }

    if (lastPos != std::wstring::npos) {
        cleanTitle = cleanTitle.substr(lastPos + sepLen);
    }

    cleanTitle.erase(0, cleanTitle.find_first_not_of(L" \t\r\n"));
    if (!cleanTitle.empty()) {
        cleanTitle.erase(cleanTitle.find_last_not_of(L" \t\r\n") + 1);
    }

    if (cleanTitle.empty()) cleanTitle = title;

    DWORD_PTR dwResult = 0;
    HICON hIcon = nullptr;

    if (SendMessageTimeoutW(hwnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG | SMTO_NORMAL, 20, &dwResult)) {
        hIcon = (HICON)dwResult;
    }
    if (!hIcon && SendMessageTimeoutW(hwnd, WM_GETICON, ICON_SMALL, 0, SMTO_ABORTIFHUNG | SMTO_NORMAL, 20, &dwResult)) {
        hIcon = (HICON)dwResult;
    }
    if (!hIcon) hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICON);
    if (!hIcon) hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICONSM);
    if (!hIcon) hIcon = LoadIcon(NULL, IDI_APPLICATION);

    auto* apps = reinterpret_cast<std::vector<RunningApp>*>(lParam);
    apps->push_back({ hwnd, cleanTitle, hIcon });

    return TRUE;
}

// Wrapper
void RefreshRunningApps() {
    g_runningApps.clear();
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&g_runningApps));
}

// Visibility
bool AppSwitcher_IsVisible()
{
    return isVisible;
}

// Background Delay Engine
void AppSwitcher_ResetHoverTimer(HWND hWnd) {
    KillTimer(hWnd, 1);
    KillTimer(hWnd, 2);

    if (g_hThumbFade) {
        DwmUnregisterThumbnail(g_hThumbFade);
        g_hThumbFade = nullptr;
    }
    g_bgFadeStart = 0;
    ShowWindow(g_hProxyWnd, SW_HIDE);

    int delay = g_config.appSwitcher.blur.hoverDelay;

    SetTimer(hWnd, 1, delay, NULL);
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