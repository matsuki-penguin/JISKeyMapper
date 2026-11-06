// Settings.cpp
#include "Settings.h"
#include "StringUtil.h"

#include <set>
#include <utility>
#include <cwctype>

std::atomic<int>  g_layoutMode{ LAYOUT_US_TO_JIS };
std::atomic<bool> g_enabled{ true };

static const DWORD EXCLUDE_MAX = 2048;

// [Devices] Exclude as written in the INI.
static std::wstring ReadExclude()
{
    wchar_t buf[EXCLUDE_MAX] = {};
    GetPrivateProfileStringW(L"Devices", L"Exclude", L"", buf, EXCLUDE_MAX, IniPath().c_str());
    return buf;
}

// Ensures [Devices] Exclude exists (empty on first run, so that users can
// find the key) and moves the value from [Settings] used by older versions.
static void PrepareExcludeKey(const wchar_t* ini)
{
    wchar_t inDevices[EXCLUDE_MAX] = {};
    GetPrivateProfileStringW(L"Devices", L"Exclude", L"\x01", inDevices, EXCLUDE_MAX, ini);
    if (inDevices[0] == L'\x01') {
        wchar_t inSettings[EXCLUDE_MAX] = {};
        GetPrivateProfileStringW(L"Settings", L"Exclude", L"", inSettings, EXCLUDE_MAX, ini);
        WritePrivateProfileStringW(L"Devices", L"Exclude", inSettings, ini);
    }
    WritePrivateProfileStringW(L"Settings", L"Exclude", nullptr, ini);   // nullptr = delete the key
}

const std::wstring& IniPath()
{
    static std::wstring path;
    if (path.empty()) {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        path = exePath;
        const std::wstring::size_type slash = path.rfind(L'\\');
        if (slash != std::wstring::npos) path.erase(slash + 1);
        else                             path += L'\\';
        path += L"JISKeyMapper.ini";
    }
    return path;
}

bool IniExists()
{
    return GetFileAttributesW(IniPath().c_str()) != INVALID_FILE_ATTRIBUTES;
}

void LoadSettings()
{
    const wchar_t* ini = IniPath().c_str();
    g_enabled = GetPrivateProfileIntW(L"Settings", L"Enabled", 1, ini) != 0;

    PrepareExcludeKey(ini);

    wchar_t mode[32] = {};
    GetPrivateProfileStringW(L"Settings", L"Mode", L"", mode, 32, ini);
    for (int i = 0; mode[i]; ++i) mode[i] = (wchar_t)::towupper(mode[i]);
    if (wcscmp(mode, L"JIS_TO_US") == 0) {
        g_layoutMode = LAYOUT_JIS_TO_US;
    } else {
        g_layoutMode = LAYOUT_US_TO_JIS;
        if (wcscmp(mode, L"US_TO_JIS") != 0)
            WritePrivateProfileStringW(L"Settings", L"Mode", L"US_TO_JIS", ini);
    }
}

std::vector<std::wstring> LoadExcludeList()
{
    std::vector<std::wstring> tokens;
    const std::wstring all = ReadExclude();
    std::wstring::size_type start = 0;
    while (start <= all.size()) {
        std::wstring::size_type end = all.find(L';', start);
        if (end == std::wstring::npos) end = all.size();
        std::wstring token = all.substr(start, end - start);
        Trim(token);
        if (!token.empty()) tokens.push_back(ToUpper(token));
        start = end + 1;
    }
    return tokens;
}

std::map<std::wstring, bool> LoadSavedDevices()
{
    std::map<std::wstring, bool> saved;
    const wchar_t* ini = IniPath().c_str();
    const int count = (int)GetPrivateProfileIntW(L"Devices", L"Count", 0, ini);
    for (int i = 0; i < count; ++i) {
        wchar_t keyPath[32], keyCheck[32];
        wsprintfW(keyPath,  L"Path%d",  i);
        wsprintfW(keyCheck, L"Check%d", i);
        wchar_t path[1024] = {};
        GetPrivateProfileStringW(L"Devices", keyPath, L"", path, 1024, ini);
        if (path[0] == L'\0') continue;
        bool& checked = saved[path];
        checked = checked || (GetPrivateProfileIntW(L"Devices", keyCheck, 0, ini) != 0);
    }
    return saved;
}

void SaveIni(const std::vector<SavedDevice>& connected)
{
    const wchar_t* ini = IniPath().c_str();

    std::set<std::wstring> connectedPaths;
    for (size_t i = 0; i < connected.size(); ++i) connectedPaths.insert(connected[i].path);

    std::vector<std::pair<std::wstring, bool> > entries;
    const int oldCount = (int)GetPrivateProfileIntW(L"Devices", L"Count", 0, ini);
    for (int i = 0; i < oldCount; ++i) {
        wchar_t keyPath[32], keyCheck[32];
        wsprintfW(keyPath,  L"Path%d",  i);
        wsprintfW(keyCheck, L"Check%d", i);
        wchar_t path[1024] = {};
        GetPrivateProfileStringW(L"Devices", keyPath, L"", path, 1024, ini);
        if (path[0] == L'\0' || connectedPaths.count(path) > 0) continue;
        const bool checked = GetPrivateProfileIntW(L"Devices", keyCheck, 0, ini) != 0;
        entries.push_back(std::make_pair(std::wstring(path), checked));
    }
    for (size_t i = 0; i < connected.size(); ++i)
        entries.push_back(std::make_pair(connected[i].path, connected[i].checked));

    WritePrivateProfileStringW(L"Settings", L"Mode",
        g_layoutMode == LAYOUT_US_TO_JIS ? L"US_TO_JIS" : L"JIS_TO_US", ini);
    WritePrivateProfileStringW(L"Settings", L"Enabled", g_enabled ? L"1" : L"0", ini);

    // [Devices] is rewritten from scratch; Exclude (edited by the user) is kept.
    const std::wstring exclude = ReadExclude();
    WritePrivateProfileSectionW(L"Devices", L"\0", ini);   // double NUL = empty section
    WritePrivateProfileStringW(L"Devices", L"Exclude", exclude.c_str(), ini);

    wchar_t count[32];
    wsprintfW(count, L"%d", (int)entries.size());
    WritePrivateProfileStringW(L"Devices", L"Count", count, ini);
    for (size_t i = 0; i < entries.size(); ++i) {
        wchar_t keyPath[32], keyCheck[32];
        wsprintfW(keyPath,  L"Path%d",  (int)i);
        wsprintfW(keyCheck, L"Check%d", (int)i);
        WritePrivateProfileStringW(L"Devices", keyPath,  entries[i].first.c_str(), ini);
        WritePrivateProfileStringW(L"Devices", keyCheck, entries[i].second ? L"1" : L"0", ini);
    }
}
