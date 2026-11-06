// Main.cpp
// JISKeyMapper: resident tool that remaps keystrokes when the physical
// keyboard layout does not match the Windows keyboard layout.
//
// Modules
//   Common.h      shared IDs, operating mode, Enable switch, logging
//   KeyMap        key maps for both modes
//   Settings      JISKeyMapper.ini
//   Devices       keyboard enumeration and per-keyboard check state
//   Hook          hook thread: low-level hook, Raw Input, key injection
//   Tray          tray icon and menu
//   Main          entry point and main window (this file)
//
// Threads
//   main thread : tray icon, menu, device enumeration, INI file
//   hook thread : WH_KEYBOARD_LL hook and the Raw Input window (WM_INPUT)
//   Slow work (SetupDi enumeration, menus) never runs on the hook thread, so
//   the hook cannot exceed LowLevelHooksTimeout and be silently removed.
#include "Common.h"
#include "KeyMap.h"
#include "Settings.h"
#include "Devices.h"
#include "Hook.h"
#include "Tray.h"

#define TIMER_DEVICE_CHANGE     1       // re-enumerate after a short delay
#define DEVICE_CHANGE_DELAY_MS  500     // merges bursts of device notifications

static const wchar_t MAIN_CLASS_NAME[] = L"JISKeyMapperWnd";
static const wchar_t MUTEX_NAME[]      = L"JISKeyMapper_SingleInstance_v1";

static void SaveSettings()
{
    SaveIni(GetSavedDeviceList());
}

static void ToggleEnabled()
{
    g_enabled = !g_enabled;
    UpdateTrayIcon();
    SaveSettings();
}

static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == TaskbarCreatedMessage() && msg != 0) {
        AddTrayIcon(hWnd);          // Explorer restarted: register the icon again
        return 0;
    }

    switch (msg) {
    case WM_CREATE:
        EnumerateKeyboards();
        AddTrayIcon(hWnd);
        SaveSettings();
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_DEVICE_CHANGE);
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;

    case WM_APP_DEVICE_CHANGE:
        SetTimer(hWnd, TIMER_DEVICE_CHANGE, DEVICE_CHANGE_DELAY_MS, nullptr);   // restarts the delay
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_DEVICE_CHANGE) {
            KillTimer(hWnd, TIMER_DEVICE_CHANGE);
            EnumerateKeyboards();
            UpdateTrayIcon();
        }
        return 0;

    case WM_APP_TRAY:
        switch (LOWORD(lParam)) {
        case WM_LBUTTONUP:          // left click: toggle Enable
            ToggleEnabled();
            break;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowTrayMenu(hWnd);
            break;
        }
        return 0;

    case WM_COMMAND: {
        const UINT id = LOWORD(wParam);
        if (id == IDM_EXIT) {
            DestroyWindow(hWnd);
        } else if (id == IDM_ENABLE) {
            ToggleEnabled();
        } else if (id == IDM_MODE_US_TO_JIS || id == IDM_MODE_JIS_TO_US) {
            g_layoutMode = (id == IDM_MODE_US_TO_JIS) ? LAYOUT_US_TO_JIS : LAYOUT_JIS_TO_US;
            UpdateTrayIcon();
            SaveSettings();
        } else if (id >= IDM_DEVICE_BASE) {
            const size_t index = (size_t)(id - IDM_DEVICE_BASE);
            if (index < GetDevices().size()) {
                ToggleDeviceChecked(index);
                UpdateTrayIcon();
                SaveSettings();
            }
        }
        return 0;
    }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    BuildKeyMapIndex();
    LoadSettings();

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"JISKeyMapper is already running.", L"JISKeyMapper", MB_OK | MB_ICONINFORMATION);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = MAIN_CLASS_NAME;
    RegisterClassExW(&wc);

    HWND hMainWnd = CreateWindowExW(0, MAIN_CLASS_NAME, L"JISKeyMapper", 0, 0, 0, 0, 0,
                                    HWND_MESSAGE, nullptr, hInst, nullptr);
    if (!hMainWnd) {
        MessageBoxW(nullptr, L"CreateWindow failed.", L"JISKeyMapper", MB_OK | MB_ICONERROR);
        CloseHandle(hMutex);
        return 1;
    }

    if (!StartHookThread(hMainWnd)) {
        MessageBoxW(nullptr, L"SetWindowsHookEx / RegisterRawInputDevices failed.\nTry running as Administrator.",
                    L"JISKeyMapper", MB_OK | MB_ICONERROR);
        DestroyWindow(hMainWnd);
        CloseHandle(hMutex);
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    StopHookThread();
    if (hMutex) CloseHandle(hMutex);
    return (int)msg.wParam;
}
