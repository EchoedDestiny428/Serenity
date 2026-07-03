#pragma once
#include <windows.h>
#include <vector>
#include "Core/SettingsManager.h"
#include "Logic.h"

void DrawCards(HWND hWnd, HDC hdc, const AppConfig& config, const std::vector<RunningApp>& apps, int selectedIndex);