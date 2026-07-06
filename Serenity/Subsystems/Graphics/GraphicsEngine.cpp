#include "GraphicsEngine.h"

static ID2D1Factory* pFactory = nullptr;
static std::unordered_map<HWND, ID2D1HwndRenderTarget*> renderTargets;

namespace Graphics {

    bool Initialize(HWND hWnd) {
        if (!pFactory) {
            HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
            if (FAILED(hr)) return false;
        }

        // Only create a new target if this specific window doesn't have one yet
        if (renderTargets.find(hWnd) == renderTargets.end()) {
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

            ID2D1HwndRenderTarget* pTarget = nullptr;
            HRESULT hr = pFactory->CreateHwndRenderTarget(props, hwndProps, &pTarget);
            if (FAILED(hr)) return false;

            renderTargets[hWnd] = pTarget;
        }
        return true;
    }

    void Cleanup() {
        for (auto& pair : renderTargets) {
            if (pair.second) {
                pair.second->Release();
            }
        }
        renderTargets.clear();

        if (pFactory) {
            pFactory->Release();
            pFactory = nullptr;
        }
    }

    ID2D1Factory* GetFactory() { return pFactory; }
    
    ID2D1HwndRenderTarget* GetRenderTarget(HWND hWnd) { 
        auto it = renderTargets.find(hWnd);
        if (it != renderTargets.end()) {
            return it->second;
        }
        return nullptr;
    }
}