
#include <thread>
#include <windows.h>
#include <shellapi.h>
#include <string>

#include "daemon.h"

#define WM_FDB_NOTIFYMESSAGE (WM_USER + 1)

#define ID_TASK_ICON 100
#define ID_MENU_ITEM_NAME 101
#define ID_MENU_ITEM_SEP 102
#define ID_MENU_ITEM_EXIT 103

static std::string g_appName = "app name";
static std::atomic_bool g_finish = false;
static HMENU g_menu;
static HWND g_wnd;

void createNotifyIcon(HWND hWnd, const std::string& hitText)
{
    NOTIFYICONDATA nid;

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = ID_TASK_ICON;
    nid.uVersion = NOTIFYICON_VERSION;
    nid.uCallbackMessage = WM_FDB_NOTIFYMESSAGE;
    nid.hIcon = (HICON)LoadImage(nullptr, "FreeDistributedBuild.ico", IMAGE_ICON, 0, 0,
                                 LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);
    strcpy_s(nid.szTip, hitText.c_str());
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;

    Shell_NotifyIcon(NIM_ADD, &nid);
}

void deleteNotifyIcon(HWND hWnd)
{
    NOTIFYICONDATA nid;

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = ID_TASK_ICON;

    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void changeNotifyIconHint(HWND hWnd, const char* hitText)
{
    NOTIFYICONDATA nid;

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = ID_TASK_ICON;
    strcpy_s(nid.szTip, hitText);
    nid.uFlags = NIF_TIP;

    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

bool showMenu()
{
    POINT pt;
    GetCursorPos(&pt);

    g_menu = CreatePopupMenu();

    AppendMenu(g_menu, MF_DISABLED , ID_MENU_ITEM_NAME, g_appName.c_str());
    AppendMenu(g_menu, MF_SEPARATOR, ID_MENU_ITEM_SEP, "");
    AppendMenu(g_menu, 0, ID_MENU_ITEM_EXIT, "Exit");

    SetForegroundWindow(g_wnd);

    TrackPopupMenu(g_menu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_wnd, NULL);

    PostMessage(g_wnd, WM_NULL, 0, 0);

    return true;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE: break;

        case WM_FDB_NOTIFYMESSAGE:
            switch(lParam)
            {
                case WM_RBUTTONUP: showMenu(); break;
                default: return DefWindowProc(hWnd, msg, wParam, lParam);
            };
            break;

        case WM_COMMAND:
            if (lParam == 0 && LOWORD(wParam) == ID_MENU_ITEM_EXIT)
            {
                g_finish = true;
            }
            break;

        default: return DefWindowProc(hWnd, msg, wParam, lParam);
    };
    return 0;
}

void createWindow(const std::string& winName)
{
    const char* className = "FDB deamon class";
    WNDCLASS wc = {};

    HINSTANCE hInstance = GetModuleHandle(NULL);

    wc.style = 0;
    wc.hInstance     = hInstance;
    wc.lpfnWndProc   = WindowProc;
    wc.lpszClassName = className;

    if (!RegisterClass(&wc))
    {
        return;
    }

    g_appName = winName;
    g_wnd = CreateWindowEx(0, className, g_appName.c_str(), WS_EX_TOOLWINDOW | WS_CLIPCHILDREN, 10, 10, 200, 200, nullptr, nullptr, hInstance, nullptr);

    if (g_wnd == NULL)
    {
        return;
    }

    ShowWindow(g_wnd, SW_HIDE);
    createNotifyIcon(g_wnd, winName);
    UpdateWindow(g_wnd);

    MSG msg;
    while (GetMessage(&msg, g_wnd, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (g_finish.load())
        {
            break;
        }
    }

    deleteNotifyIcon(g_wnd);
}
