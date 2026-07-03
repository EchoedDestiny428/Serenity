#include "Render.h"
#include "State.h"

// Cards 
void DrawCards(HWND hWnd, HDC hdc, const AppConfig& config, const std::vector<RunningApp>& apps, int selectedIndex) {
    if (apps.empty()) return;

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

    HFONT hFont = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI Variable Display");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    HBRUSH hNormalBrush = CreateSolidBrush(RGB(30, 30, 46));
    HBRUSH hSelectBrush = CreateSolidBrush(RGB(49, 50, 68));

    HPEN hNormalPen = CreatePen(PS_INSIDEFRAME, 2, RGB(45, 45, 65));
    HPEN hAccentPen = CreatePen(PS_INSIDEFRAME, 2, RGB(137, 180, 250));

    SetBkMode(hdc, TRANSPARENT);

    for (int i = 0; i < numApps; ++i) {
        int row = i / cols;
        int col = i % cols;

        int appsInThisRow = (row == rows - 1) ? (numApps - (row * cols)) : cols;
        int rowWidth = (appsInThisRow * cardW) + ((appsInThisRow - 1) * gap);
        int rowStartX = (screenW - rowWidth) / 2;

        int xPos = rowStartX + (col * (cardW + gap));
        int yPos = startY + (row * (cardH + gap));
        RECT cardRect = { xPos, yPos, xPos + cardW, yPos + cardH };

        if (i == selectedIndex) {
            SelectObject(hdc, hSelectBrush);
            SelectObject(hdc, hAccentPen);
        }
        else {
            SelectObject(hdc, hNormalBrush);
            SelectObject(hdc, hNormalPen);
        }

        RoundRect(hdc, cardRect.left, cardRect.top, cardRect.right, cardRect.bottom, 16, 16);

        if (apps[i].hIcon) {
            int iconSize = 48;
            DrawIconEx(hdc, cardRect.left + (cardW - iconSize) / 2, cardRect.top + 26, apps[i].hIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
        }

        if (i == selectedIndex) SetTextColor(hdc, RGB(205, 214, 244));
        else SetTextColor(hdc, RGB(166, 173, 200));

        RECT textRect = cardRect;
        textRect.top += 88; textRect.bottom -= 12; textRect.left += 12; textRect.right -= 12;
        DrawTextW(hdc, apps[i].title.c_str(), -1, &textRect, DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteObject(hNormalBrush); DeleteObject(hSelectBrush);
    DeleteObject(hNormalPen); DeleteObject(hAccentPen);
}