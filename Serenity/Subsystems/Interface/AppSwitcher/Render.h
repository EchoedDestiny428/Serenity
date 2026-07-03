#pragma once
#include <windows.h>
#include <d2d1.h>
#include <vector>
#include "Logic.h"
#include "Core/SettingsManager.h"

void DrawCards(ID2D1HwndRenderTarget* target, const AppConfig& config, const std::vector<RunningApp>& apps, int selectedIndex);