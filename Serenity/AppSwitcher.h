#pragma once
#include <windows.h>

void AppSwitcher_Init(HINSTANCE hInstance);
void AppSwitcher_Show();
void AppSwitcher_Hide();
bool AppSwitcher_IsVisible();
void AppSwitcher_NextApp();
void AppSwitcher_PrevApp();
void AppSwitcher_Commit();
int AppSwitcher_GetScrollDelta();