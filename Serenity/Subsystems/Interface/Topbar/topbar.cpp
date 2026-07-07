#include "TopBar.h"
#include "Graphics/GraphicsEngine.h"
#include "Windowing/OverlayEngine.h"
#include <d2d1.h>
#include <shellapi.h>
#include <dwmapi.h>

#pragma comment(lib, "shell32.lib")

static HWND hTopBarWnd = nullptr;
static int topBarHeight = 60;
static int marginX = 8.0f;
static int marginY = 6.0f;
static int cornerRadius = 6.0f;

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

                target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

                ID2D1SolidColorBrush* bgBrush = nullptr;
                ID2D1SolidColorBrush* borderBrush = nullptr;

                target->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), &bgBrush);
                target->CreateSolidColorBrush(D2D1::ColorF(0.7f, 0.3f, 1.0f, 1.0f), &borderBrush);

                if (bgBrush && borderBrush) {
                    D2D1_SIZE_F size = target->GetSize();

                    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
                        D2D1::RectF(marginX, marginY, size.width - marginX, size.height - marginY),
						cornerRadius, cornerRadius
                    );

                    target->FillRoundedRectangle(&rr, bgBrush);
                    target->DrawRoundedRectangle(&rr, borderBrush, 1.0f);

                    bgBrush->Release();
                    borderBrush->Release();
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
    
	MARGINS margins = {-1, -1, -1, -1};
	DwmExtendFrameIntoClientArea(hTopBarWnd, &margins);
    RegisterAppBar(hTopBarWnd);
}

void TopBar_Show() {
    if (hTopBarWnd) {
        Graphics::Initialize(hTopBarWnd);
        ShowWindow(hTopBarWnd, SW_SHOWNA);
        InvalidateRect(hTopBarWnd, NULL, FALSE);
    }
}