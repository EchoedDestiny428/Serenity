#pragma once
#include <windows.h>
#include <vector>
#include <string>

struct RunningApp {
    HWND hwnd;
    std::wstring title;
    HICON hIcon;
};

extern std::vector<RunningApp> g_runningApps;

void RefreshRunningApps();
void AppSwitcher_ResetHoverTimer(HWND hWnd);
void AppSwitcher_NextApp();
void AppSwitcher_PrevApp();
void AppSwitcher_Commit();
bool AppSwitcher_IsVisible();
