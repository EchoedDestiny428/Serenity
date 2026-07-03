#pragma once
#include <windows.h>
#include <vector>
#include <dwmapi.h>
#include "Core/SettingsManager.h"

extern HWND hAppSwitcherWnd;
extern bool isVisible;
extern AppConfig g_config;
extern int g_selectedIndex;
extern int g_mouseScrollDelta;
extern int g_lastHoveredIndex;
extern bool g_isFadingIn;

extern HWND g_hProxyWnd;
extern HTHUMBNAIL g_hThumbFade;
extern ULONGLONG g_bgFadeStart;
extern HWND g_pendingBgHwnd;
extern bool g_bgSwapTriggered;