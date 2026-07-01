#pragma once
#include <windows.h>

#define DEFAULT_OPACITY 220
#define DEFAULT_TRIGGER_DELAY 500

struct AppConfig {
    int opacity;
    int triggerDelay;
};

class SettingsManager {
public:
    static AppConfig Load();
};