// Tray.h
// Tray icon and its menu.  Main thread only.
// The icon sends WM_APP_TRAY to the window passed to AddTrayIcon(); menu
// selections arrive there as WM_COMMAND with the IDM_* IDs from Common.h.
#pragma once

#include "Common.h"

void AddTrayIcon(HWND hWnd);    // registers the icon (also after Explorer restarts)
void UpdateTrayIcon();          // refreshes icon and tooltip from the current state
void RemoveTrayIcon();          // removes the icon and frees the menu
void ShowTrayMenu(HWND hWnd);

// Message ID that Explorer broadcasts when the taskbar is re-created.
UINT TaskbarCreatedMessage();
