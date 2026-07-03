#pragma once
#include <windows.h>

struct AppBox {
    int width, height, padding, startX, startY;
};

struct Blur {
	int triggerDelay;
	int fadeStep;
	int hoverDelay;
};

struct AppSwitcher {
    AppBox appBox;
	Blur blur;
};

struct AppConfig {
	AppSwitcher appSwitcher;
};

class SettingsManager {
public:
    static AppConfig Load();
};