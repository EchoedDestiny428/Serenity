#include "SettingsManager.h"
#include <string>

AppConfig SettingsManager::Load() {
    AppConfig cfg;
    WCHAR szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    
    std::wstring path = szPath;
    path = path.substr(0, path.find_last_of(L"\\/") + 1) + L"settings.ini";

    cfg.appSwitcher.appBox.width   = GetPrivateProfileIntW(L"AppSwitcher", L"Width", 400, path.c_str());
    cfg.appSwitcher.appBox.height  = GetPrivateProfileIntW(L"AppSwitcher", L"Height", 80, path.c_str());
    cfg.appSwitcher.appBox.padding = GetPrivateProfileIntW(L"AppSwitcher", L"Padding", 20, path.c_str());
    cfg.appSwitcher.appBox.startX  = GetPrivateProfileIntW(L"AppSwitcher", L"StartX", 50, path.c_str());
    cfg.appSwitcher.appBox.startY  = GetPrivateProfileIntW(L"AppSwitcher", L"StartY", 50, path.c_str());

    cfg.appSwitcher.blur.triggerDelay = GetPrivateProfileIntW(L"AppSwitcher", L"TriggerDelay", 100, path.c_str());
    cfg.appSwitcher.blur.fadeStep     = GetPrivateProfileIntW(L"AppSwitcher", L"FadeStep", 15, path.c_str());
	cfg.appSwitcher.blur.hoverDelay = GetPrivateProfileIntW(L"AppSwitcher", L"HoverDelay", 500, path.c_str());

    return cfg;
}