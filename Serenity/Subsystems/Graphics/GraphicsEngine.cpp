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

            D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                0.0f, 0.0f,
                D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE,
                D2D1_FEATURE_LEVEL_DEFAULT
            );

            D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
                hWnd, 
                D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)
            );

            HRESULT hr = pFactory->CreateHwndRenderTarget(props, hwndProps, &pRenderTarget);
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