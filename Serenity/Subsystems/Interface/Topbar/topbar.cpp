#include "TopBar.h"
#include "Graphics/GraphicsEngine.h"
#include "Windowing/OverlayEngine.h"
#include <d2d1.h>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")

static HWND hTopBarWnd = nullptr;
static int topBarHeight = 32;

void RegisterAppBar(HWND hWnd) {
    APPBARDATA abd = { sizeof(APPBARDATA) };
    abd.hWnd = hWnd;
    
    SHAppBarMessage(ABM_NEW, &abd);

    abd.uEdge = ABE_TOP;
    abd.rc.top = 0;
    abd.rc.left = 0;
    abd.rc.right = GetSystemMetrics(SM_CXSCREEN);
    abd.rc.bottom = topBarHeight;

    SHAppBarMessage(ABM_QUERYPOS, &abd);
    SHAppBarMessage(ABM_SETPOS, &abd);
    
    MoveWindow(hWnd, abd.rc.left, abd.rc.top, abd.rc.right - abd.rc.left, abd.rc.bottom - abd.rc.top, TRUE);
}

void UnregisterAppBar(HWND hWnd) {
    APPBARDATA abd = { sizeof(APPBARDATA) };
    abd.hWnd = hWnd;
    SHAppBarMessage(ABM_REMOVE, &abd);
}

LRESULT CALLBACK TopBar_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_ERASEBKGND: return 1;
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            
            auto target = Graphics::GetRenderTarget(hWnd);
            if (target) {
                target->BeginDraw();
                
                ID2D1SolidColorBrush* bgBrush = nullptr;
                target->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.14f, 1.0f), &bgBrush);
                
                if (bgBrush) {
                    D2D1_SIZE_F size = target->GetSize();
                    D2D1_RECT_F rect = D2D1::RectF(0.0f, 0.0f, size.width, size.height);
                    
                    target->FillRectangle(&rect, bgBrush);
                    
                    ID2D1SolidColorBrush* borderBrush = nullptr;
                    target->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.2f, 0.25f, 1.0f), &borderBrush);
                    if (borderBrush) {
                        target->DrawLine(
                            D2D1::Point2F(0.0f, size.height - 0.5f), 
                            D2D1::Point2F(size.width, size.height - 0.5f), 
                            borderBrush, 1.0f
                        );
                        borderBrush->Release();
                    }
                    bgBrush->Release();
                }
                
                // TODO: Draw clock, workspaces, and system info here!
                
                target->EndDraw();
            }
            EndPaint(hWnd, &ps);
            return 0;
        }
        
        case WM_DESTROY: {
            UnregisterAppBar(hWnd);
            return 0;
        }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void TopBar_Init(HINSTANCE hInstance) {
    OverlaySettings settings = { 
        L"SerenityTopBarClass", 
        GetSystemMetrics(SM_CXSCREEN), topBarHeight, 
        0, 0, 255 
    };
    
    hTopBarWnd = OverlayEngine_Create(hInstance, settings, TopBar_WndProc);
    
    SetWindowLong(hTopBarWnd, GWL_EXSTYLE, GetWindowLong(hTopBarWnd, GWL_EXSTYLE) & ~(WS_EX_TRANSPARENT | WS_EX_LAYERED));
    
    RegisterAppBar(hTopBarWnd);
}

void TopBar_Show() {
    if (hTopBarWnd) {
        Graphics::Initialize(hTopBarWnd);
        ShowWindow(hTopBarWnd, SW_SHOWNA);
        InvalidateRect(hTopBarWnd, NULL, FALSE);
    }
}