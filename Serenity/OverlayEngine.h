#pragma once
#include <windows.h>

typedef void(*WindowStyleSetup)(HWND);

struct OverlaySettings {
    LPCWSTR className;
    int width;
    int height;
    int x;
    int y;
    BYTE opacity;
    WindowStyleSetup styleCallback;
};

HWND OverlayEngine_Create(HINSTANCE hInstance, OverlaySettings settings, WNDPROC lpfnWndProc);