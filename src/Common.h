// Common.h
// Definitions shared by all modules: Windows headers, message and command IDs,
// the operating mode, the Enable switch, and diagnostic logging.
#pragma once

#ifndef UNICODE
#  define UNICODE
#endif
#ifndef _UNICODE
#  define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <atomic>

// ---------------------------------------------------------------------------
// Window messages and menu command IDs
// ---------------------------------------------------------------------------
#define WM_APP_TRAY             (WM_APP + 1)   // tray icon notifications (Tray -> Main)
#define WM_APP_DEVICE_CHANGE    (WM_APP + 2)   // keyboard added/removed (Hook thread -> Main)

#define IDM_DEVICE_BASE         2000    // + index into GetDevices()
#define IDM_MODE_US_TO_JIS      3000
#define IDM_MODE_JIS_TO_US      3001
#define IDM_ENABLE              3100
#define IDM_EXIT                9999

// ---------------------------------------------------------------------------
// Operating mode and master switch (defined in Settings.cpp)
// ---------------------------------------------------------------------------
enum LayoutMode {
    LAYOUT_US_TO_JIS = 0,   // JIS keyboard on a US-layout Windows (default)
    LAYOUT_JIS_TO_US = 1,   // US keyboard on a JIS-layout Windows
};

// Read by the hook thread, written by the main thread.
extern std::atomic<int>  g_layoutMode;  // LayoutMode
extern std::atomic<bool> g_enabled;     // "Enable" in the menu; false = never remap

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------
// 1: write device enumeration results and Hankaku/Zenkaku decisions
//    to JISKeyMapper.log in the EXE folder
#define JKM_LOG 0

#if JKM_LOG
#include <cstdio>
#include <cstdarg>
#include <cwchar>
inline void LogF(const wchar_t* fmt, ...)
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return;
    wcscpy_s(slash + 1, MAX_PATH - (slash - path) - 1, L"JISKeyMapper.log");
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path, L"a") != 0 || !fp) return;
    fwprintf(fp, L"%10lu ", GetTickCount());
    va_list args;
    va_start(args, fmt);
    vfwprintf(fp, fmt, args);
    va_end(args);
    fwprintf(fp, L"\n");
    fclose(fp);
}
#define LOGF(...) LogF(__VA_ARGS__)
#else
#define LOGF(...) ((void)0)
#endif
