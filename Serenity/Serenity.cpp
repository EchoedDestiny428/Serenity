#include "framework.h"
#include "Serenity.h"
#include <shellapi.h> 

#define WM_TRAYICON (WM_USER + 1)

// Global Variables
HINSTANCE hInst;                                
WCHAR szWindowClass[100];            

// Forward declarations
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

// The main entry point for the Windows application. It initializes the app and runs the message loop.
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDC_SERENITY, szWindowClass, 100);
    MyRegisterClass(hInstance);

    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}

// Registers the window class structure to define the core behavior and style of the application.
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = nullptr;
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName   = nullptr;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = nullptr; 

    return RegisterClassExW(&wcex);
}

// Creates the window as hidden, then registers a default application icon into the Windows system tray.
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; 

   HWND hWnd = CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TOPMOST,
      szWindowClass,                      
      L"Serenity",                            
      WS_POPUP,                           
      100, 100,                           
      400, 250,                           
      nullptr, nullptr, hInstance, nullptr
   );

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, SW_HIDE); 

   NOTIFYICONDATAW nid = {};
   nid.cbSize = sizeof(NOTIFYICONDATAW);
   nid.hWnd = hWnd;
   nid.uID = 1;
   nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
   nid.uCallbackMessage = WM_TRAYICON;

   nid.hIcon = nid.hIcon = (HICON)LoadImageW(
       nullptr, 
       L"Assets\\Icons\\serenity.ico", 
       IMAGE_ICON, 
       0, 0, 
       LR_LOADFROMFILE | LR_DEFAULTSIZE
   );(nullptr, IDI_APPLICATION);

   wcscpy_s(nid.szTip, L"Serenity Widget Manager");

   Shell_NotifyIconW(NIM_ADD, &nid);

   return TRUE;
}

// Processes events, listening for right-clicks on the tray icon to close the app, and cleans up the icon on exit.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP)
        {
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, 1001, L"Exit Serenity");
            SetForegroundWindow(hWnd);
            POINT pt;
            GetCursorPos(&pt);
            TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, nullptr);
            DestroyMenu(hMenu);
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1001)
        {
            DestroyWindow(hWnd);
        }
        break;

    case WM_DESTROY:
    {
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(NOTIFYICONDATAW);
        nid.hWnd = hWnd;
        nid.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        PostQuitMessage(0);
    }
    break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}