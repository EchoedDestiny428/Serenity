#include "State.h"

HWND hAppSwitcherWnd = nullptr;
bool isVisible = false;
AppConfig g_config;
int g_selectedIndex = 0;
int g_mouseScrollDelta = 0;
int g_lastHoveredIndex = -1;
bool g_isFadingIn = false;
HWND g_hProxyWnd = nullptr;
HTHUMBNAIL g_hThumbFade = nullptr;
ULONGLONG g_bgFadeStart = 0;
HWND g_pendingBgHwnd = nullptr;
bool g_bgSwapTriggered = false;