#pragma once

#include <windows.h>
#include <d2d1.h>
#include <unordered_map>

#pragma comment(lib, "d2d1.lib")

namespace Graphics {
    bool Initialize(HWND hWnd);
    
    void Cleanup();
    
    ID2D1Factory* GetFactory();

    ID2D1HwndRenderTarget* GetRenderTarget(HWND hWnd);
}