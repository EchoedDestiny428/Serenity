#include "AppSwitcher.h"
#include "OverlayEngine.h"
#include <vector>
#include <string>
#include "SettingsManager.h"
#include <dwmapi.h>
#include <cmath>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "msimg32.lib")

// --- Forward Declared Structs and Enums for Windows Composition ---
enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4
};

struct ACCENT_POLICY {
    int AccentState;
    int AccentFlags;
    int GradientColor;
    int AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    int Attrib;
    PVOID pvData;
    SIZE_T cbData;
};

typedef BOOL(WINAPI* pfnSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

struct RunningApp {
    HWND hwnd;
    std::wstring title;
    HICON hIcon;
};

// --- Global Variables ---
static HWND hAppSwitcherWnd = nullptr;
static bool isVisible = false;
static int g_currentMode = 0; // 0 = Grid, 1 = Left Vertical, 2 = Right Vertical
AppConfig g_config;
int g_selectedIndex = 0;
static int g_mouseScrollDelta = 0;
static int g_lastHoveredIndex = -1;
bool g_isFadingIn = false;

static HWND g_hProxyWnd = nullptr;
static HTHUMBNAIL g_hThumbFade = nullptr;
static ULONGLONG g_bgFadeStart = 0;
static HWND g_pendingBgHwnd = nullptr;
static bool g_bgSwapTriggered = false;

// High-Precision Animation & Smooth Scrolling Configurations
std::vector<RECT> g_cardRects;       
std::vector<RECT> g_targetRects;     
static float g_scrollTargetY = 0.0f; 
static float g_scrollCurrentY = 0.0f;
static LARGE_INTEGER g_animLastTime = { 0 };
static LARGE_INTEGER g_clockFreq = { 0 };

std::vector<RunningApp> g_runningApps;

// --- Localized DWM Blur Engine ---
void UpdateBlurLayout(HWND hWnd, int mode) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    
    auto SetWindowCompositionAttribute = (pfnSetWindowCompositionAttribute)GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute");
        
    if (mode == 0) {
        // Grid Mode: Full Window Acrylic Blur 
        if (SetWindowCompositionAttribute) {
            ACCENT_POLICY policy = { ACCENT_ENABLE_ACRYLICBLURBEHIND, 0, 0x01202020, 0 };
            WINDOWCOMPOSITIONATTRIBDATA data = { 19, &policy, sizeof(policy) };
            SetWindowCompositionAttribute(hWnd, &data);
        }
        // Disable localized region overrides
        DWM_BLURBEHIND bb = { DWM_BB_ENABLE, FALSE, NULL, FALSE };
        DwmEnableBlurBehindWindow(hWnd, &bb);
    } 
    else {
        // Sidebar Mode: Kill global blur state to drop the buggy Pink/Purple tint artifacts
        if (SetWindowCompositionAttribute) {
            ACCENT_POLICY policy = { ACCENT_DISABLED, 0, 0, 0 };
            WINDOWCOMPOSITIONATTRIBDATA data = { 19, &policy, sizeof(policy) };
            SetWindowCompositionAttribute(hWnd, &data);
        }
        
        // Apply Aero Blur strictly to the 250px sidebars. Center remains perfectly clear.
        int stripW = 250;
        HRGN hBlurRgn = (mode == 1) ? CreateRectRgn(0, 0, stripW, screenH) 
                                    : CreateRectRgn(screenW - stripW, 0, screenW, screenH);
        
        DWM_BLURBEHIND bb = { DWM_BB_ENABLE | DWM_BB_BLURREGION, TRUE, hBlurRgn, FALSE };
        DwmEnableBlurBehindWindow(hWnd, &bb);
        DeleteObject(hBlurRgn);
    }
}

void CalculateLayout(int mode) {
    g_targetRects.clear();
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int cardW = 150, cardH = 150, gap = 24;
    int numApps = (int)g_runningApps.size();
    if (numApps == 0) return;

    if (mode == 0) { // GRID MODE 
        int maxCols = 7;
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

            int x = rowStartX + (col * (cardW + gap));
            int y = startY + (row * (cardH + gap));
            g_targetRects.push_back({ x, y, x + cardW, y + cardH });
        }
    } 
    else { // SIDEBAR SLOT MACHINE 
        int totalH = (numApps * cardH) + ((numApps - 1) * gap);
        int startY = (screenH - totalH) / 2;
        int x = (mode == 1) ? 50 : (screenW - cardW - 50);

        for (int i = 0; i < numApps; ++i) {
            int y = startY + (i * (cardH + gap));
            g_targetRects.push_back({ x, y, x + cardW, y + cardH });
        }
    }
}

// --- Get Running Applications ---
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;

    int cloaked = 0;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) return TRUE;

    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

    WCHAR buffer[256];
    if (GetWindowTextW(hwnd, buffer, 256) == 0) return TRUE;
    std::wstring title(buffer);

    if (hwnd == hAppSwitcherWnd) return TRUE;
    if (title == L"Program Manager") return TRUE;

    std::wstring cleanTitle = title;
    size_t lastPos = std::wstring::npos;
    size_t sepLen = 0;
    const wchar_t* separators[] = { L" - ", L" \x2013 ", L" \x2014 ", L" \x2015 ", L" | " };

    for (const wchar_t* sep : separators) {
        size_t pos = cleanTitle.rfind(sep);
        if (pos != std::wstring::npos) {
            if (lastPos == std::wstring::npos || pos > lastPos) { lastPos = pos; sepLen = wcslen(sep); }
        }
    }

    if (lastPos != std::wstring::npos) cleanTitle = cleanTitle.substr(lastPos + sepLen);
    cleanTitle.erase(0, cleanTitle.find_first_not_of(L" \t\r\n"));
    if (!cleanTitle.empty()) cleanTitle.erase(cleanTitle.find_last_not_of(L" \t\r\n") + 1);
    if (cleanTitle.empty()) cleanTitle = title;

    DWORD_PTR dwResult = 0;
    HICON hIcon = nullptr;

    if (SendMessageTimeoutW(hwnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG | SMTO_NORMAL, 20, &dwResult)) hIcon = (HICON)dwResult;
    if (!hIcon && SendMessageTimeoutW(hwnd, WM_GETICON, ICON_SMALL, 0, SMTO_ABORTIFHUNG | SMTO_NORMAL, 20, &dwResult)) hIcon = (HICON)dwResult;
    if (!hIcon) hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICON);
    if (!hIcon) hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICONSM);
    if (!hIcon) hIcon = LoadIcon(NULL, IDI_APPLICATION);

    auto* apps = reinterpret_cast<std::vector<RunningApp>*>(lParam);
    apps->push_back({ hwnd, cleanTitle, hIcon });
    return TRUE;
}

void RefreshRunningApps() {
    g_runningApps.clear();
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&g_runningApps));
}

void __stdcall SetupModernBlur(HWND hWnd) {
    UpdateBlurLayout(hWnd, 0);
}

void AppSwitcher_ResetHoverTimer(HWND hWnd) {
    KillTimer(hWnd, 1); KillTimer(hWnd, 2);
    if (g_hThumbFade) { DwmUnregisterThumbnail(g_hThumbFade); g_hThumbFade = nullptr; }
    g_bgFadeStart = 0;
    ShowWindow(g_hProxyWnd, SW_HIDE);
    SetTimer(hWnd, 1, g_config.appSwitcher.blur.hoverDelay, NULL);
}

// --- Cards Rendering Engine ---
void DrawCards(HWND hWnd, HDC hdc, const AppConfig& config, const std::vector<RunningApp>& apps, int selectedIndex) {
    if (apps.empty() || g_cardRects.size() != apps.size()) return;

    HFONT hFont = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI Variable Display");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    HBRUSH hNormalBrush = CreateSolidBrush(RGB(30, 30, 46));
    HBRUSH hSelectBrush = CreateSolidBrush(RGB(49, 50, 68));
    HPEN hNormalPen = CreatePen(PS_INSIDEFRAME, 2, RGB(45, 45, 65));
    HPEN hAccentPen = CreatePen(PS_INSIDEFRAME, 2, RGB(137, 180, 250));

    SetBkMode(hdc, TRANSPARENT);

    for (size_t i = 0; i < apps.size(); ++i) {
        RECT cardRect = g_cardRects[i];
        if (cardRect.bottom < 0 || cardRect.top > GetSystemMetrics(SM_CYSCREEN)) continue;

        if ((int)i == selectedIndex) { SelectObject(hdc, hSelectBrush); SelectObject(hdc, hAccentPen); }
        else { SelectObject(hdc, hNormalBrush); SelectObject(hdc, hNormalPen); }

        RoundRect(hdc, cardRect.left, cardRect.top, cardRect.right, cardRect.bottom, 16, 16);

        if (apps[i].hIcon) {
            int cardWidth = cardRect.right - cardRect.left;
            int iconSize = 48;
            DrawIconEx(hdc, cardRect.left + (cardWidth - iconSize) / 2, cardRect.top + 26, apps[i].hIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
        }

        if ((int)i == selectedIndex) SetTextColor(hdc, RGB(205, 214, 244));
        else SetTextColor(hdc, RGB(166, 173, 200));

        RECT textRect = cardRect;
        textRect.top += 88; textRect.bottom -= 12; textRect.left += 12; textRect.right -= 12;
        DrawTextW(hdc, apps[i].title.c_str(), -1, &textRect, DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    SelectObject(hdc, hOldFont); DeleteObject(hFont);
    DeleteObject(hNormalBrush); DeleteObject(hSelectBrush); DeleteObject(hNormalPen); DeleteObject(hAccentPen);
}

// --- Window Procedure ---
LRESULT CALLBACK AppSwitcher_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_ERASEBKGND: {
            return 1; 
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            RECT clientRect; GetClientRect(hWnd, &clientRect);
            int screenW = clientRect.right;
            int screenH = clientRect.bottom;

            HDC hMemDC = CreateCompatibleDC(hdc);
            HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, screenW, screenH);
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hMemBmp);

            // Wipe canvas with pure Black (Matches LWA_COLORKEY, making it 100% transparent without pink/purple tints)
            HBRUSH hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hMemDC, &clientRect, hBlackBrush);
            DeleteObject(hBlackBrush);

            DrawCards(hWnd, hMemDC, g_config, g_runningApps, g_selectedIndex);

            BitBlt(hdc, 0, 0, screenW, screenH, hMemDC, 0, 0, SRCCOPY);

            SelectObject(hMemDC, hOldBmp); DeleteObject(hMemBmp); DeleteDC(hMemDC);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            if (isVisible) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (g_currentMode != 0) {
                    g_scrollTargetY += (delta > 0 ? 500.0f : -500.0f);

                    int screenH = GetSystemMetrics(SM_CYSCREEN);
                    int cardH = 150, gap = 24;
                    int numApps = (int)g_runningApps.size();
                    int totalH = (numApps * cardH) + ((numApps - 1) * gap);
                    int startY = (screenH - totalH) / 2;

                    float maxScroll = (screenH / 2.0f - cardH / 2.0f) - startY;
                    float minScroll = (screenH / 2.0f - cardH / 2.0f) - (startY + (numApps - 1) * (cardH + gap));

                    if (g_scrollTargetY > maxScroll) g_scrollTargetY = maxScroll;
                    if (g_scrollTargetY < minScroll) g_scrollTargetY = minScroll;
                }
                else {
                    g_mouseScrollDelta += delta;

                    if (delta > 0) AppSwitcher_PrevApp();
                    else AppSwitcher_NextApp();
                }
            }
            return 0;
        }
        case WM_TIMER: {
            if (wParam == 1) { 
                KillTimer(hWnd, 1);
                if (isVisible && g_selectedIndex >= 0 && g_selectedIndex < (int)g_runningApps.size()) {
                    if (g_lastHoveredIndex == g_selectedIndex) return 0;
                    g_pendingBgHwnd = g_runningApps[g_selectedIndex].hwnd;
                    SetWindowPos(g_hProxyWnd, hWnd, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

                    if (SUCCEEDED(DwmRegisterThumbnail(g_hProxyWnd, g_pendingBgHwnd, &g_hThumbFade))) {
                        DWM_THUMBNAIL_PROPERTIES props = { 0 };
                        props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
                        GetClientRect(g_hProxyWnd, &props.rcDestination);
                        props.fVisible = TRUE; props.opacity = 0;
                        DwmUpdateThumbnailProperties(g_hThumbFade, &props);

                        g_bgFadeStart = GetTickCount64(); g_bgSwapTriggered = false;
                        SetTimer(hWnd, 2, 16, NULL);
                    }
                }
            }
            else if (wParam == 2) {
                if (g_bgFadeStart != 0 && g_hThumbFade) {
                    ULONGLONG elapsed = GetTickCount64() - g_bgFadeStart;
                    float t = (float)elapsed / 150.0f;
                    if (t >= 1.0f && !g_bgSwapTriggered) {
                        SetWindowPos(g_pendingBgHwnd, hWnd, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
                        g_bgSwapTriggered = true;
                        DWM_THUMBNAIL_PROPERTIES props = { 0 }; props.dwFlags = DWM_TNP_OPACITY; props.opacity = 255;
                        DwmUpdateThumbnailProperties(g_hThumbFade, &props);
                    }
                    else if (g_bgSwapTriggered && elapsed > 250) {
                        DwmUnregisterThumbnail(g_hThumbFade); g_hThumbFade = nullptr; g_bgFadeStart = 0;
                        g_lastHoveredIndex = g_selectedIndex; ShowWindow(g_hProxyWnd, SW_HIDE); KillTimer(hWnd, 2);
                    }
                    else if (!g_bgSwapTriggered) {
                        DWM_THUMBNAIL_PROPERTIES props = { 0 }; props.dwFlags = DWM_TNP_OPACITY; props.opacity = (BYTE)(t * 255.0f);
                        DwmUpdateThumbnailProperties(g_hThumbFade, &props);
                    }
                } else KillTimer(hWnd, 2);
            }
            else if (wParam == 3) { // --- HIGH PRECISION ANIMATION ENGINE ---
                if (!isVisible) return 0;

                // 1. Edge Tracking (Globally evaluates position across the full transparent screen)
                POINT pt; GetCursorPos(&pt);
                int screenW = GetSystemMetrics(SM_CXSCREEN);
                int edgeThreshold = (int)(screenW * 0.10); 
                int newMode = 0; 
                if (pt.x < edgeThreshold) newMode = 1; 
                else if (pt.x > (screenW - edgeThreshold)) newMode = 2; 

                bool needsRedraw = false;

                if (newMode != g_currentMode) {
                    g_currentMode = newMode;
                    CalculateLayout(g_currentMode);
                    
                    if (g_currentMode == 0) {
                        g_scrollTargetY = 0.0f; 
                    } else {
                        int screenH = GetSystemMetrics(SM_CYSCREEN);
                        int cardH = 150, gap = 24;
                        int startY = (screenH - ((int)g_runningApps.size() * cardH + ((int)g_runningApps.size() - 1) * gap)) / 2;
                        g_scrollTargetY = (screenH / 2.0f - cardH / 2.0f) - (startY + g_selectedIndex * (cardH + gap));
                    }

                    UpdateBlurLayout(hWnd, g_currentMode);
                    needsRedraw = true;
                }
                
                // 2. Exponential Decay Interpolation (Stops the stutter)
                LARGE_INTEGER now; QueryPerformanceCounter(&now);
                float dt = (float)(now.QuadPart - g_animLastTime.QuadPart) / g_clockFreq.QuadPart;
                g_animLastTime = now;
                if (dt > 0.1f) dt = 0.1f; 

                float lerpFactor = 1.0f - std::exp(-15.0f * dt);
                if (lerpFactor < 0.01f) lerpFactor = 0.01f;
                if (lerpFactor > 1.0f) lerpFactor = 1.0f;

                if (std::abs(g_scrollTargetY - g_scrollCurrentY) > 1.0f) {
                    g_scrollCurrentY += (g_scrollTargetY - g_scrollCurrentY) * lerpFactor;
                    needsRedraw = true;
                } else g_scrollCurrentY = g_scrollTargetY;

                if (g_cardRects.size() != g_runningApps.size()) {
                    g_cardRects.resize(g_runningApps.size());
                    for (size_t i = 0; i < g_targetRects.size(); ++i) g_cardRects[i] = g_targetRects[i];
                }

                for (size_t i = 0; i < g_cardRects.size(); ++i) {
                    float targetX = (float)g_targetRects[i].left;
                    float targetY = (float)g_targetRects[i].top;

                    if (g_currentMode != 0) {
                        targetY += g_scrollCurrentY;
                    }

                    float curX = (float)g_cardRects[i].left;
                    float curY = (float)g_cardRects[i].top;

                    // Absolute deadzone clamping stops perpetual busy loops
                    if (std::abs(targetX - curX) > 1.0f || std::abs(targetY - curY) > 1.0f) {
                        curX += (targetX - curX) * lerpFactor;
                        curY += (targetY - curY) * lerpFactor;
                        needsRedraw = true;
                    } else { curX = targetX; curY = targetY; }

                    g_cardRects[i].left = (int)curX;
                    g_cardRects[i].top = (int)curY;
                    g_cardRects[i].right = (int)curX + 150;
                    g_cardRects[i].bottom = (int)curY + 150;
                }

                // 3. Focal Selection Index Locking
                if (g_currentMode != 0) {
                    int centerY = GetSystemMetrics(SM_CYSCREEN) / 2;
                    int closestIndex = g_selectedIndex;
                    int minDistance = 999999;
                    for (size_t i = 0; i < g_cardRects.size(); ++i) {
                        int cardCenterY = g_cardRects[i].top + 75;
                        int dist = std::abs(cardCenterY - centerY);
                        if (dist < minDistance) { minDistance = dist; closestIndex = (int)i; }
                    }
                    if (closestIndex != g_selectedIndex) { g_selectedIndex = closestIndex; needsRedraw = true; }
                }

                if (needsRedraw) InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_SETCURSOR: {
            SetCursor(LoadCursorW(NULL, IDC_ARROW));
            return TRUE;
        }
        case WM_DESTROY: {
            KillTimer(hWnd, 1); KillTimer(hWnd, 2); KillTimer(hWnd, 3);
            if (g_hThumbFade) DwmUnregisterThumbnail(g_hThumbFade);
            if (g_hProxyWnd) DestroyWindow(g_hProxyWnd);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// --- Init ---
void AppSwitcher_Init(HINSTANCE hInstance)
{
    g_config = SettingsManager::Load();

    OverlaySettings settings = { 0 };
    settings.className = L"AppSwitcherClass";
    settings.width = GetSystemMetrics(SM_CXSCREEN);
    settings.height = GetSystemMetrics(SM_CYSCREEN);
    settings.x = 0; settings.y = 0; settings.opacity = 255;
    settings.styleCallback = SetupModernBlur;

    WNDCLASSEXW proxyWc = { sizeof(WNDCLASSEXW) };
    proxyWc.lpfnWndProc = DefWindowProcW; proxyWc.hInstance = hInstance; proxyWc.lpszClassName = L"AppSwitcherProxyClass";
    proxyWc.hbrBackground = CreateSolidBrush(RGB(1, 1, 1)); RegisterClassExW(&proxyWc);

    g_hProxyWnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        L"AppSwitcherProxyClass", L"", WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), nullptr, nullptr, hInstance, nullptr);
    SetLayeredWindowAttributes(g_hProxyWnd, RGB(1, 1, 1), 0, LWA_COLORKEY);

    hAppSwitcherWnd = OverlayEngine_Create(hInstance, settings, AppSwitcher_WndProc);
}

bool AppSwitcher_IsVisible() { return isVisible; }

// --- Fade Management Engine ---
void FadeWindow(HWND hWnd, bool fadeIn) {
    g_isFadingIn = fadeIn;
    ULONGLONG startTime = GetTickCount64();
    int safeStep = g_config.appSwitcher.blur.fadeStep;
    if (safeStep <= 0) safeStep = 15;
    float duration = (255.0f / (float)safeStep) * 10.0f;

    SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    float t = 0.0f;

    while (t < 1.0f) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        ULONGLONG elapsed = GetTickCount64() - startTime;
        t = (float)elapsed / duration; if (t > 1.0f) t = 1.0f;

        float eased = fadeIn ? (1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t)) : (t * t * t);
        int alpha = fadeIn ? (int)(eased * 255.0f) : (int)((1.0f - eased) * 255.0f);
        if (alpha > 255) alpha = 255; if (alpha < 0) alpha = 0;

        // Ensure Color Key stays mapped correctly during opacity fades
        SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), (BYTE)alpha, LWA_COLORKEY | LWA_ALPHA);
        DwmFlush();
    }

    if (!fadeIn) {
        KillTimer(hWnd, 3); ShowWindow(hWnd, SW_HIDE);
        SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
        isVisible = false;
    }
}

// --- Show Window ---
void AppSwitcher_Show() {
    if (!isVisible) {
        RefreshRunningApps();
        g_selectedIndex = 0; g_lastHoveredIndex = -1; g_currentMode = 0; 
        g_scrollTargetY = 0.0f; g_scrollCurrentY = 0.0f;

        CalculateLayout(g_currentMode);
        g_cardRects.resize(g_runningApps.size());
        for (size_t i = 0; i < g_targetRects.size(); ++i) g_cardRects[i] = g_targetRects[i];
        
        SetWindowRgn(hAppSwitcherWnd, NULL, TRUE);
        UpdateBlurLayout(hAppSwitcherWnd, g_currentMode);

        // Map Black to absolute transparency for pure pass-through
        SetLayeredWindowAttributes(hAppSwitcherWnd, RGB(0, 0, 0), 255, LWA_COLORKEY | LWA_ALPHA);

        ShowWindow(hAppSwitcherWnd, SW_SHOWNA);
        SetWindowLong(hAppSwitcherWnd, GWL_EXSTYLE, GetWindowLong(hAppSwitcherWnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
        isVisible = true;

        QueryPerformanceFrequency(&g_clockFreq);
        QueryPerformanceCounter(&g_animLastTime);
        SetTimer(hAppSwitcherWnd, 3, 10, NULL); 
        
        FadeWindow(hAppSwitcherWnd, true);
    }
}

void AppSwitcher_Hide() {
    if (isVisible) { KillTimer(hAppSwitcherWnd, 1); FadeWindow(hAppSwitcherWnd, false); }
}

int AppSwitcher_GetScrollDelta() {
    int delta = g_mouseScrollDelta; g_mouseScrollDelta = 0; return delta;
}

// --- Selection Navigation ---
void AppSwitcher_NextApp() {
    if (g_runningApps.empty()) return;
    if (g_selectedIndex < (int)g_runningApps.size() - 1) g_selectedIndex++;
    else g_selectedIndex = 0; 

    if (g_currentMode != 0) {
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int cardH = 150, gap = 24;
        int startY = (screenH - ((int)g_runningApps.size() * cardH + ((int)g_runningApps.size() - 1) * gap)) / 2;
        g_scrollTargetY = (screenH / 2.0f - cardH / 2.0f) - (startY + g_selectedIndex * (cardH + gap));
    }
    
    AppSwitcher_ResetHoverTimer(hAppSwitcherWnd);
    InvalidateRect(hAppSwitcherWnd, NULL, FALSE);
}

void AppSwitcher_PrevApp() {
    if (g_runningApps.empty()) return;
    if (g_selectedIndex > 0) g_selectedIndex--;
    else g_selectedIndex = (int)g_runningApps.size() - 1; 

    if (g_currentMode != 0) {
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int cardH = 150, gap = 24;
        int startY = (screenH - ((int)g_runningApps.size() * cardH + ((int)g_runningApps.size() - 1) * gap)) / 2;
        g_scrollTargetY = (screenH / 2.0f - cardH / 2.0f) - (startY + g_selectedIndex * (cardH + gap));
    }
    
    AppSwitcher_ResetHoverTimer(hAppSwitcherWnd);
    InvalidateRect(hAppSwitcherWnd, NULL, FALSE);
}

void AppSwitcher_Commit() {
    if (g_runningApps.empty() || g_selectedIndex < 0 || g_selectedIndex >= (int)g_runningApps.size()) return;
    HWND target = g_runningApps[g_selectedIndex].hwnd;
    if (IsIconic(target)) ShowWindowAsync(target, SW_RESTORE);
    else ShowWindowAsync(target, SW_SHOW);
    SetForegroundWindow(target);

    if (GetKeyState(VK_CAPITAL) & 0x0001) {
        keybd_event(VK_CAPITAL, 0x3a, KEYEVENTF_EXTENDEDKEY | 0, 0);
        keybd_event(VK_CAPITAL, 0x3a, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }
}