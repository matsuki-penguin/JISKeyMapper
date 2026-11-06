// Settings.h
// JISKeyMapper.ini (next to the EXE).  Main thread only.
//
//   [Settings]  Mode=US_TO_JIS|JIS_TO_US  Enabled=1|0
//   [Devices]   Exclude=token;token...  Count=N  PathN=<device path or __RDP__>  CheckN=1|0
#pragma once

#include "Common.h"

#include <string>
#include <vector>
#include <map>

struct SavedDevice {
    std::wstring path;      // device path, or "__RDP__"
    bool         checked;
};

const std::wstring& IniPath();
bool IniExists();

// Reads Mode and Enabled into g_layoutMode / g_enabled.
// Also moves Exclude from [Settings] (older versions) to [Devices].
void LoadSettings();

// [Devices] Exclude tokens, upper case.
std::vector<std::wstring> LoadExcludeList();

// [Devices] as { path: checked }.  A path listed more than once counts as
// checked if any of its entries is checked.
std::map<std::wstring, bool> LoadSavedDevices();

// Writes Mode, Enabled and [Devices].  Saved entries of keyboards that are not
// in 'connected' are kept, so a sleeping Bluetooth keyboard keeps its state.
void SaveIni(const std::vector<SavedDevice>& connected);
