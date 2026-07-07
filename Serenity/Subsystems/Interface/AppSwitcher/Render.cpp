#include "Render.h"
#include "State.h"
#include <dwrite.h>
#include <wincodec.h>
#include <unordered_map>

#pragma comment(lib, "dwrite.lib")

static ID2D1SolidColorBrush* pNormalBrush = nullptr;
static ID2D1SolidColorBrush* pSelectBrush = nullptr;
static ID2D1SolidColorBrush* pNormalBorder = nullptr;
static ID2D1SolidColorBrush* pAccentBorder = nullptr;
static ID2D1SolidColorBrush* pTextNormal = nullptr;
static ID2D1SolidColorBrush* pTextSelect = nullptr;

static IDWriteFactory* pDWriteFactory = nullptr;
static IDWriteTextFormat* pTextFormat = nullptr;

std::unordered_map<HWND, ID2D1Bitmap*> g_appSwitcherIconCache;
IWICImagingFactory* pAppSwitcherWIC = nullptr;

ID2D1Bitmap* GetAppSwitcherBitmap(ID2D1HwndRenderTarget* target, HICON hIcon, HWND hwnd) {
    if (g_appSwitcherIconCache.find(hwnd) != g_appSwitcherIconCache.end()) return g_appSwitcherIconCache[hwnd];

    if (!pAppSwitcherWIC) {
        CoInitialize(NULL);
        CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pAppSwitcherWIC));
    }

    IWICBitmap* wic = nullptr;
    ID2D1Bitmap* bmp = nullptr;
    if (pAppSwitcherWIC && SUCCEEDED(pAppSwitcherWIC->CreateBitmapFromHICON(hIcon, &wic))) {
        IWICFormatConverter* conv = nullptr;
        if (SUCCEEDED(pAppSwitcherWIC->CreateFormatConverter(&conv))) {
            conv->Initialize(wic, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeMedianCut);
            target->CreateBitmapFromWicBitmap(conv, NULL, &bmp);
            conv->Release();
        }
        wic->Release();
    }
    g_appSwitcherIconCache[hwnd] = bmp;
    return bmp;
}

void EnsureResources(ID2D1HwndRenderTarget* target) {
    if (!pNormalBrush)  target->CreateSolidColorBrush(D2D1::ColorF(0x1E1E2E), &pNormalBrush);
    if (!pSelectBrush)  target->CreateSolidColorBrush(D2D1::ColorF(0x313244), &pSelectBrush);
    if (!pNormalBorder) target->CreateSolidColorBrush(D2D1::ColorF(0x2D2D41), &pNormalBorder);
    if (!pAccentBorder) target->CreateSolidColorBrush(D2D1::ColorF(0x89B4FA), &pAccentBorder);
    if (!pTextNormal)   target->CreateSolidColorBrush(D2D1::ColorF(0xA6ADC8), &pTextNormal);
    if (!pTextSelect)   target->CreateSolidColorBrush(D2D1::ColorF(0xCDD6F4), &pTextSelect);

    if (!pDWriteFactory) {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pDWriteFactory));

        HWND hWnd = target->GetHwnd();
        float scale = GetDpiForWindow(hWnd) / 96.0f;

        float baseFontSize = 10.0f;

        pDWriteFactory->CreateTextFormat(
            L"Segoe UI Variable Display", NULL,
            DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            baseFontSize * scale,
            L"en-us", &pTextFormat
        );

        pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        pTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    }
}

// cards
void DrawCards(ID2D1HwndRenderTarget* target, const AppConfig& config, const std::vector<RunningApp>& apps, int selectedIndex) {
    if (apps.empty()) return;

    EnsureResources(target);

    HWND hWnd = target->GetHwnd();
    float scale = GetDpiForWindow(hWnd) / 96.0f;

    D2D1_SIZE_F rtSize = target->GetSize();
    int screenW = (int)rtSize.width;
    int screenH = (int)rtSize.height;

    int cardW = (int)(100 * scale);
    int cardH = (int)(100 * scale);
    int gap = (int)(24 * scale);
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

        D2D1_ROUNDED_RECT rRect = D2D1::RoundedRect(
            D2D1::RectF((float)xPos, (float)yPos, (float)(xPos + cardW), (float)(yPos + cardH)),
            16.0f * scale, 16.0f * scale
        );

        target->FillRoundedRectangle(rRect, (i == selectedIndex) ? pSelectBrush : pNormalBrush);

        target->DrawRoundedRectangle(rRect, (i == selectedIndex) ? pAccentBorder : pNormalBorder, 2.0f * scale);

        if (apps[i].hIcon) {
            ID2D1Bitmap* pIconBmp = GetAppSwitcherBitmap(target, apps[i].hIcon, apps[i].hwnd);
            if (pIconBmp) {
                float iconSize = 32.0f * scale;
                float iconOffsetY = 26.0f * scale;

                float iconX = xPos + (cardW - iconSize) / 2.0f;
                float iconY = yPos + iconOffsetY;

                target->DrawBitmap(pIconBmp, D2D1::RectF(iconX, iconY, iconX + iconSize, iconY + iconSize));
            }
        }

        float topMargin = 65 * scale;
        float bottomMargin = 12 * scale;

        D2D1_RECT_F textRect = D2D1::RectF(
            (float)(xPos + (12 * scale)),
            (float)(yPos + topMargin),
            (float)(xPos + cardW - (12 * scale)),
            (float)(yPos + cardH - bottomMargin)
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