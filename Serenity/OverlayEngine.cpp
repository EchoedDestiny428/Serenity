#include "OverlayEngine.h"

HWND OverlayEngine_Create(HINSTANCE hInstance, OverlaySettings settings, WNDPROC lpfnWndProc)
{
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };
    wcex.lpfnWndProc = lpfnWndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = settings.className;
    wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        settings.className,
        nullptr,
        WS_POPUP,
        settings.x, settings.y, settings.width, settings.height,
        nullptr, nullptr, hInstance, nullptr
    );

    if (hWnd) {
        SetLayeredWindowAttributes(hWnd, 0, settings.opacity, LWA_ALPHA);
    }

    if (settings.styleCallback != nullptr) {
        settings.styleCallback(hWnd);
    }

    return hWnd;
}