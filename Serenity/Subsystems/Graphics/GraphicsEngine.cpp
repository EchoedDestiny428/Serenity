#include "GraphicsEngine.h"

static ID2D1Factory* pFactory = nullptr;
static ID2D1HwndRenderTarget* pRenderTarget = nullptr;

namespace Graphics {

    bool Initialize(HWND hWnd) {
        if (!pFactory) {
            HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
            if (FAILED(hr)) return false;
        }

        if (!pRenderTarget) {
            RECT rc;
            GetClientRect(hWnd, &rc);

            HRESULT hr = pFactory->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(hWnd, D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)),
                &pRenderTarget
            );
            if (FAILED(hr)) return false;
        }
        return true;
    }

    void Cleanup() {
        if (pRenderTarget) {
            pRenderTarget->Release();
            pRenderTarget = nullptr;
        }
        if (pFactory) {
            pFactory->Release();
            pFactory = nullptr;
        }
    }

    ID2D1Factory* GetFactory() { return pFactory; }
    ID2D1HwndRenderTarget* GetRenderTarget() { return pRenderTarget; }
}