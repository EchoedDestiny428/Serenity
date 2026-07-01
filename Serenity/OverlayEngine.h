#pragma once
#include <windows.h>

struct OverlaySettings {
    LPCWSTR className;
    int width;
    int height;
    BYTE opacity;
};

HWND OverlayEngine_Create(HINSTANCE hInstance, OverlaySettings settings, WNDPROC lpfnWndProc);