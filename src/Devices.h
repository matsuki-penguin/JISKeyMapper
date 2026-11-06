// Devices.h
// Keyboard enumeration and per-keyboard check state.
//
// The device list itself is main-thread only.  The set of checked device
// handles is shared with the hook thread through IsCheckedDevice() /
// AnyDeviceChecked(), which may be called from any thread.
#pragma once

#include "Common.h"
#include "Settings.h"

#include <string>
#include <vector>

// One entry of the Device menu.  A Bluetooth keyboard shows up as several
// Raw Input handles (HID collections); they are grouped into one entry.
struct DeviceInfo {
    std::wstring        savedPath;      // path of the first handle ("__RDP__" for RDP); key in the INI
    std::wstring        displayName;    // text in the Device menu
    std::wstring        groupKey;       // MAC (Bluetooth) / instance id / "__RDP__"
    bool                checked = false;
    std::vector<HANDLE> handles;        // Raw Input device handles
};

// --- main thread ---
void                           EnumerateKeyboards();         // rebuilds the list; keeps check states
const std::vector<DeviceInfo>& GetDevices();
void                           ToggleDeviceChecked(size_t index);
std::vector<SavedDevice>       GetSavedDeviceList();         // for SaveIni()

// --- any thread ---
bool IsCheckedDevice(HANDLE hDevice);
bool AnyDeviceChecked();
bool IsRemappingActive();       // Enable is on and at least one keyboard is checked
