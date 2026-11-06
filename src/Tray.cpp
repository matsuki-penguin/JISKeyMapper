// Tray.cpp
#include "Tray.h"
#include "Devices.h"

#include <shellapi.h>

// Icon resource IDs (see JISKeyMapper.rc)
#define IDI_TRAY_JP_ON      2   // US_TO_JIS mode, remapping
#define IDI_TRAY_JP_OFF     3   // US_TO_JIS mode, stopped
#define IDI_TRAY_US_ON      4   // JIS_TO_US mode, remapping
#define IDI_TRAY_US_OFF     5   // JIS_TO_US mode, stopped

#define TRAY_ICON_ID        1

static NOTIFYICONDATA s_trayData         = {};
static bool           s_trayIconIsShared = false;   // s_trayData.hIcon must not be destroyed
static HMENU          s_hMenu            = nullptr;
static bool           s_menuOpen         = false;

UINT TaskbarCreatedMessage()
{
    static const UINT msg = RegisterWindowMessageW(L"TaskbarCreated");
    return msg;
}

static HICON LoadTrayIcon(int resourceId)
{
    const int size = GetSystemMetrics(SM_CXSMICON);
    HICON h = (HICON)LoadImage(GetModuleHandleW(nullptr), MAKEINTRESOURCE(resourceId),
                               IMAGE_ICON, size, size, LR_DEFAULTCOLOR);
    if (h) {
        s_trayIconIsShared = false;
        return h;
    }
    s_trayIconIsShared = true;   // fallback: system icon, owned by the system
    return (HICON)LoadImage(nullptr, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
}

static int TrayIconResourceId(bool active)
{
    if (g_layoutMode == LAYOUT_US_TO_JIS) return active ? IDI_TRAY_JP_ON : IDI_TRAY_JP_OFF;
    else                                  return active ? IDI_TRAY_US_ON : IDI_TRAY_US_OFF;
}

static const wchar_t* TrayTooltip()
{
    if (!g_enabled)          return L"JISKeyMapper - OFF (disabled)";
    if (!AnyDeviceChecked()) return L"JISKeyMapper - OFF (no device checked)";
    return L"JISKeyMapper - ON";
}

static void DestroyTrayIconHandle(HICON h, bool isShared)
{
    if (h && !isShared) DestroyIcon(h);
}

// registerIcon = true : register the icon (startup, or after Explorer restarts)
// registerIcon = false: update the icon and tooltip of the registered icon
static void SetTrayIcon(HWND hWnd, bool registerIcon)
{
    if (!registerIcon && !s_trayData.hWnd) return;   // not registered yet

    const HICON hOld        = s_trayData.hIcon;
    const bool  oldIsShared = s_trayIconIsShared;
    if (registerIcon) {
        ZeroMemory(&s_trayData, sizeof(s_trayData));
        s_trayData.cbSize           = sizeof(NOTIFYICONDATA);
        s_trayData.hWnd             = hWnd;
        s_trayData.uID              = TRAY_ICON_ID;
        s_trayData.uCallbackMessage = WM_APP_TRAY;
    }
    s_trayData.uFlags = NIF_ICON | NIF_TIP | (registerIcon ? NIF_MESSAGE : 0);
    s_trayData.hIcon  = LoadTrayIcon(TrayIconResourceId(IsRemappingActive()));
    wcscpy_s(s_trayData.szTip, TrayTooltip());
    Shell_NotifyIcon(registerIcon ? NIM_ADD : NIM_MODIFY, &s_trayData);
    if (registerIcon) {
        s_trayData.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIcon(NIM_SETVERSION, &s_trayData);
    }
    DestroyTrayIconHandle(hOld, oldIsShared);
}

void AddTrayIcon(HWND hWnd) { SetTrayIcon(hWnd, true); }
void UpdateTrayIcon()       { SetTrayIcon(s_trayData.hWnd, false); }

void RemoveTrayIcon()
{
    Shell_NotifyIcon(NIM_DELETE, &s_trayData);
    DestroyTrayIconHandle(s_trayData.hIcon, s_trayIconIsShared);
    s_trayData.hIcon = nullptr;
    if (s_hMenu) { DestroyMenu(s_hMenu); s_hMenu = nullptr; }
}

void ShowTrayMenu(HWND hWnd)
{
    if (s_menuOpen) return;         // a right-click sends both WM_RBUTTONUP and WM_CONTEXTMENU
    s_menuOpen = true;

    if (s_hMenu) { DestroyMenu(s_hMenu); s_hMenu = nullptr; }
    s_hMenu = CreatePopupMenu();
    AppendMenuW(s_hMenu, MF_STRING | (g_enabled ? MF_CHECKED : 0), IDM_ENABLE, L"Enable");
    AppendMenuW(s_hMenu, MF_SEPARATOR, 0, nullptr);

    HMENU hModeMenu = CreatePopupMenu();
    AppendMenuW(hModeMenu, MF_STRING | (g_layoutMode == LAYOUT_US_TO_JIS ? MF_CHECKED : 0), IDM_MODE_US_TO_JIS, L"JIS keyboard on US-layout Windows");
    AppendMenuW(hModeMenu, MF_STRING | (g_layoutMode == LAYOUT_JIS_TO_US ? MF_CHECKED : 0), IDM_MODE_JIS_TO_US, L"US keyboard on JIS-layout Windows");

    const std::vector<DeviceInfo>& devices = GetDevices();
    HMENU hDeviceMenu = CreatePopupMenu();
    if (devices.empty()) {
        AppendMenuW(hDeviceMenu, MF_STRING | MF_GRAYED, 0, L"(no keyboard found)");
    } else {
        for (size_t i = 0; i < devices.size(); ++i) {
            const UINT flags = MF_STRING | (devices[i].checked ? MF_CHECKED : 0);
            AppendMenuW(hDeviceMenu, flags, (UINT_PTR)(IDM_DEVICE_BASE + i), devices[i].displayName.c_str());
        }
    }

    AppendMenuW(s_hMenu, MF_POPUP, (UINT_PTR)hModeMenu,   L"Mode");
    AppendMenuW(s_hMenu, MF_POPUP, (UINT_PTR)hDeviceMenu, L"Device");
    AppendMenuW(s_hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(s_hMenu, MF_STRING, IDM_EXIT, L"Exit");

    POINT cursor;
    GetCursorPos(&cursor);
    SetForegroundWindow(hWnd);      // required for the menu to close when clicking elsewhere
    TrackPopupMenu(s_hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN, cursor.x, cursor.y, 0, hWnd, nullptr);
    PostMessage(hWnd, WM_NULL, 0, 0);

    s_menuOpen = false;
}
