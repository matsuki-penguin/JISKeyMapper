// Hook.h
// The hook thread: low-level keyboard hook, Raw Input, source-keyboard
// identification and key injection.
#pragma once

#include "Common.h"

// Starts the hook thread and waits until the hook and Raw Input are ready.
// notifyWnd receives WM_APP_DEVICE_CHANGE when a keyboard is added or removed.
// Returns false if SetWindowsHookEx or RegisterRawInputDevices failed.
bool StartHookThread(HWND notifyWnd);

// Removes the hook and ends the hook thread.
void StopHookThread();
