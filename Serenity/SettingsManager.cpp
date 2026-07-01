#include "SettingsManager.h"
#include <string>

AppConfig SettingsManager::Load() {
    AppConfig cfg;
    WCHAR szPath[MAX_PATH];
    
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    std::wstring path = szPath;
    size_t lastSlash = path.find_last_of(L"\\/");
    path = path.substr(0, lastSlash + 1);
    
    path += L"settings.ini";

    cfg.opacity = GetPrivateProfileIntW(L"Overlay", L"Opacity", DEFAULT_OPACITY, path.c_str());
    cfg.triggerDelay = GetPrivateProfileIntW(L"Overlay", L"TriggerDelay", DEFAULT_TRIGGER_DELAY, path.c_str());

    return cfg;
}