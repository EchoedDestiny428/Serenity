#pragma once
#include <windows.h>
#include <d2d1.h>
#include <vector>
#include "Logic.h"
#include "Core/SettingsManager.h"
#include <unordered_map>
#include <wincodec.h>

extern std::unordered_map<HWND, ID2D1Bitmap*> g_appSwitcherIconCache;
extern IWICImagingFactory* pAppSwitcherWIC;

void DrawCards(ID2D1HwndRenderTarget* target, const AppConfig& config, const std::vector<RunningApp>& apps, int selectedIndex);