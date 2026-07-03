#include "Render.h"
#include "State.h"
#include <dwrite.h>

#pragma comment(lib, "dwrite.lib")

static ID2D1SolidColorBrush* pNormalBrush = nullptr;
static ID2D1SolidColorBrush* pSelectBrush = nullptr;
static ID2D1SolidColorBrush* pNormalBorder = nullptr;
static ID2D1SolidColorBrush* pAccentBorder = nullptr;
static ID2D1SolidColorBrush* pTextNormal = nullptr;
static ID2D1SolidColorBrush* pTextSelect = nullptr;

static IDWriteFactory* pDWriteFactory = nullptr;
static IDWriteTextFormat* pTextFormat = nullptr;

void EnsureResources(ID2D1HwndRenderTarget* target) {
    if (!pNormalBrush)  target->CreateSolidColorBrush(D2D1::ColorF(0x1E1E2E), &pNormalBrush);
    if (!pSelectBrush)  target->CreateSolidColorBrush(D2D1::ColorF(0x313244), &pSelectBrush);
    if (!pNormalBorder) target->CreateSolidColorBrush(D2D1::ColorF(0x2D2D41), &pNormalBorder);
    if (!pAccentBorder) target->CreateSolidColorBrush(D2D1::ColorF(0x89B4FA), &pAccentBorder);
    if (!pTextNormal)   target->CreateSolidColorBrush(D2D1::ColorF(0xA6ADC8), &pTextNormal);
    if (!pTextSelect)   target->CreateSolidColorBrush(D2D1::ColorF(0xCDD6F4), &pTextSelect);

    if (!pDWriteFactory) {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pDWriteFactory));
        
        pDWriteFactory->CreateTextFormat(
            L"Segoe UI Variable Display", NULL, 
            DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 
            16.0f, L"en-us", &pTextFormat
        );
        pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        pTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    }
}

// cards
void DrawCards(ID2D1HwndRenderTarget* target, const AppConfig& config, const std::vector<RunningApp>& apps, int selectedIndex) {
    if (apps.empty()) return;

    EnsureResources(target);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    int cardW = 150;
    int cardH = 150;
    int gap = 24;
    int maxCols = 7;

    int numApps = (int)apps.size();
    int cols = (numApps < maxCols) ? numApps : maxCols;
    int rows = (numApps + cols - 1) / cols;

    int totalH = (rows * cardH) + ((rows - 1) * gap);
    int startY = (screenH - totalH) / 2;

    for (int i = 0; i < numApps; ++i) {
        int row = i / cols;
        int col = i % cols;

        int appsInThisRow = (row == rows - 1) ? (numApps - (row * cols)) : cols;
        int rowWidth = (appsInThisRow * cardW) + ((appsInThisRow - 1) * gap);
        int rowStartX = (screenW - rowWidth) / 2;

        int xPos = rowStartX + (col * (cardW + gap));
        int yPos = startY + (row * (cardH + gap));

        // Create the Rounded Rect geometry
        D2D1_ROUNDED_RECT rRect = D2D1::RoundedRect(
            D2D1::RectF((float)xPos, (float)yPos, (float)(xPos + cardW), (float)(yPos + cardH)),
            16.0f, 16.0f
        );

        // 1. Draw Background
        target->FillRoundedRectangle(rRect, (i == selectedIndex) ? pSelectBrush : pNormalBrush);
        
        // 2. Draw Border (2px width)
        target->DrawRoundedRectangle(rRect, (i == selectedIndex) ? pAccentBorder : pNormalBorder, 2.0f);

        // 3. Draw Icon (GDI Interop Bridge)
        if (apps[i].hIcon) {
            ID2D1GdiInteropRenderTarget* pGdiInterop = nullptr;
            // Ask D2D for permission to draw using GDI temporarily
            if (SUCCEEDED(target->QueryInterface(__uuidof(ID2D1GdiInteropRenderTarget), (void**)&pGdiInterop))) {
                HDC hdc = nullptr;
                if (SUCCEEDED(pGdiInterop->GetDC(D2D1_DC_INITIALIZE_MODE_COPY, &hdc))) {
                    int iconSize = 48;
                    DrawIconEx(hdc, xPos + (cardW - iconSize) / 2, yPos + 26, apps[i].hIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
                    pGdiInterop->ReleaseDC(nullptr);
                }
                pGdiInterop->Release();
            }
        }

        // 4. Draw Text
        D2D1_RECT_F textRect = D2D1::RectF(
            (float)(xPos + 12), 
            (float)(yPos + 88), 
            (float)(xPos + cardW - 12), 
            (float)(yPos + cardH - 12)
        );

        target->DrawText(
            apps[i].title.c_str(), 
            (UINT32)apps[i].title.length(), 
            pTextFormat, 
            textRect, 
            (i == selectedIndex) ? pTextSelect : pTextNormal
        );
    }
}