#include "OverlayEngine.h"

HWND OverlayEngine_Create(HINSTANCE hInstance, OverlaySettings settings, WNDPROC lpfnWndProc)
{
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };
    wcex.lpfnWndProc = lpfnWndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = settings.className;
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        settings.className,
        nullptr,
        WS_POPUP,
        500, 400, settings.width, settings.height,
        nullptr, nullptr, hInstance, nullptr
    );

    if (hWnd) {
        SetLayeredWindowAttributes(hWnd, 0, settings.opacity, LWA_ALPHA);
    }

    return hWnd;
}