// Devices.cpp
#include "Devices.h"
#include "StringUtil.h"

#include <setupapi.h>
#include <hidsdi.h>
#include <devguid.h>

#include <map>
#include <set>
#include <cwctype>

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
// main thread only
static std::vector<DeviceInfo> s_devices;

// written by the main thread, read by the hook thread
static SRWLOCK           s_checkedLock = SRWLOCK_INIT;
static std::set<HANDLE>  s_checkedHandles;          // guarded by s_checkedLock
static std::atomic<bool> s_anyChecked{ false };     // !s_checkedHandles.empty(); updated under s_checkedLock

bool IsCheckedDevice(HANDLE hDevice)
{
    AcquireSRWLockShared(&s_checkedLock);
    const bool checked = s_checkedHandles.count(hDevice) > 0;
    ReleaseSRWLockShared(&s_checkedLock);
    return checked;
}

bool AnyDeviceChecked()
{
    return s_anyChecked;
}

bool IsRemappingActive()
{
    return g_enabled && AnyDeviceChecked();
}

const std::vector<DeviceInfo>& GetDevices()
{
    return s_devices;
}

std::vector<SavedDevice> GetSavedDeviceList()
{
    std::vector<SavedDevice> list;
    for (size_t i = 0; i < s_devices.size(); ++i) {
        SavedDevice d;
        d.path    = s_devices[i].savedPath;
        d.checked = s_devices[i].checked;
        list.push_back(d);
    }
    return list;
}

void ToggleDeviceChecked(size_t index)
{
    if (index >= s_devices.size()) return;
    DeviceInfo& device = s_devices[index];
    device.checked = !device.checked;
    AcquireSRWLockExclusive(&s_checkedLock);
    for (size_t i = 0; i < device.handles.size(); ++i) {
        if (device.checked) s_checkedHandles.insert(device.handles[i]);
        else                s_checkedHandles.erase(device.handles[i]);
    }
    s_anyChecked = !s_checkedHandles.empty();
    ReleaseSRWLockExclusive(&s_checkedLock);
}

// ---------------------------------------------------------------------------
// Device path helpers
// ---------------------------------------------------------------------------
// Remote Desktop keyboards: all RDP virtual keyboards form one menu entry,
// saved in the INI under this placeholder.
static const wchar_t RDP_KEY[]  = L"__RDP__";
static const wchar_t RDP_NAME[] = L"Remote Desktop keyboard";

// {E0CBF06C-CD8B-4647-BB8A-263B43F0F974}: Bluetooth device setup class
static const GUID GUID_CLASS_BLUETOOTH =
    { 0xe0cbf06c, 0xcd8b, 0x4647, { 0xbb, 0x8a, 0x26, 0x3b, 0x43, 0xf0, 0xf9, 0x74 } };

// "\\?\HID#VID_xxxx&PID_xxxx#7&...#{guid}" -> "HID\VID_XXXX&PID_XXXX\7&..."
static std::wstring PathToInstanceId(const std::wstring& path)
{
    std::wstring s = path;
    if (s.size() > 4 && s[0] == L'\\' && s[1] == L'\\' && (s[2] == L'?' || s[2] == L'.') && s[3] == L'\\')
        s = s.substr(4);
    std::wstring::size_type guid = s.rfind(L'#');
    if (guid != std::wstring::npos && guid + 1 < s.size() && s[guid + 1] == L'{')
        s = s.substr(0, guid);
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == L'#') s[i] = L'\\';
    MakeUpper(s);
    return s;
}

// RDP virtual keyboards report an empty path.
static bool IsRdpKeyboardPath(const std::wstring& upperPath)
{
    return upperPath.empty()
        || Contains(upperPath, L"RDP_KBD")
        || Contains(upperPath, L"TERMINPUT");
}

// 12-hex-digit MAC of a Bluetooth HID path ("..._C243A7D3749E&COL01..."), or empty.
static std::wstring ExtractBtMac(const std::wstring& upperPath)
{
    const std::wstring::size_type col = upperPath.rfind(L"&COL");
    if (col == std::wstring::npos) return L"";
    const std::wstring::size_type sep = upperPath.rfind(L'_', col);
    if (sep == std::wstring::npos) return L"";
    const std::wstring mac = upperPath.substr(sep + 1, col - sep - 1);
    if (mac.size() != 12) return L"";
    for (size_t i = 0; i < mac.size(); ++i)
        if (!iswxdigit(mac[i])) return L"";
    return mac;
}

// "VID_2F24&PID_0135" of a USB HID path, or empty.
static std::wstring ExtractVidPid(const std::wstring& upperPath)
{
    const std::wstring::size_type vid = upperPath.find(L"VID_");
    if (vid == std::wstring::npos || vid + 8 > upperPath.size()) return L"";
    const std::wstring::size_type pid = upperPath.find(L"PID_", vid + 8);
    if (pid == std::wstring::npos || pid + 8 > upperPath.size()) return L"";
    for (int i = 0; i < 4; ++i)
        if (!iswxdigit(upperPath[vid + 4 + i]) || !iswxdigit(upperPath[pid + 4 + i])) return L"";
    return upperPath.substr(vid, 8) + L"&" + upperPath.substr(pid, 8);
}

// Product name reported by the HID device itself (e.g. "USB KEYBOARD").
// Opening with access 0 works even for keyboards that the system holds open.
static std::wstring GetHidProductString(const std::wstring& path)
{
    if (path.empty()) return L"";
    HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return L"";
    wchar_t buf[256] = {};
    std::wstring product;
    if (HidD_GetProductString(h, buf, sizeof(buf) - sizeof(wchar_t))) {
        buf[255] = L'\0';
        product = buf;
    }
    CloseHandle(h);
    Trim(product);
    return product;
}

// ---------------------------------------------------------------------------
// Classification
// ---------------------------------------------------------------------------
// A device node from a SetupDi class list.
struct DevNode {
    std::wstring instanceId;    // upper case
    std::wstring name;          // friendly name, else device description
    bool         hasName;
};

// Data read once per enumeration and shared by all keyboards.
struct EnumContext {
    std::vector<DevNode>         keyboardClass;         // GUID_DEVCLASS_KEYBOARD
    std::vector<DevNode>         bluetoothClass;        // loaded on first use
    bool                         bluetoothLoaded = false;
    std::map<std::wstring, bool> savedDevices;          // from the INI
    std::vector<std::wstring>    excludes;              // from the INI
};

static void LoadDevNodes(const GUID& setupClass, std::vector<DevNode>& out)
{
    HDEVINFO hInfo = SetupDiGetClassDevsW(&setupClass, nullptr, nullptr, DIGCF_PRESENT);
    if (hInfo == INVALID_HANDLE_VALUE) return;
    SP_DEVINFO_DATA data = {};
    data.cbSize = sizeof(data);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(hInfo, i, &data); ++i) {
        wchar_t id[512] = {};
        if (!SetupDiGetDeviceInstanceIdW(hInfo, &data, id, 512, nullptr)) continue;
        DevNode node;
        node.instanceId = ToUpper(id);
        wchar_t name[256] = {};
        node.hasName = SetupDiGetDeviceRegistryPropertyW(hInfo, &data, SPDRP_FRIENDLYNAME, nullptr, (PBYTE)name, sizeof(name) - sizeof(wchar_t), nullptr)
                    || SetupDiGetDeviceRegistryPropertyW(hInfo, &data, SPDRP_DEVICEDESC,   nullptr, (PBYTE)name, sizeof(name) - sizeof(wchar_t), nullptr);
        name[255] = L'\0';
        if (node.hasName) node.name = name;
        out.push_back(node);
    }
    SetupDiDestroyDeviceInfoList(hInfo);
}

// Keyboard-class node whose instance id equals instanceId or is its parent.
static const DevNode* FindKeyboardClassNode(const EnumContext& ctx, const std::wstring& instanceId)
{
    for (size_t i = 0; i < ctx.keyboardClass.size(); ++i) {
        const std::wstring& id = ctx.keyboardClass[i].instanceId;
        if (instanceId == id ||
            (instanceId.size() > id.size() && instanceId.compare(0, id.size(), id) == 0 && instanceId[id.size()] == L'\\'))
            return &ctx.keyboardClass[i];
    }
    return nullptr;
}

// Name of the BTHLE\DEV_<MAC> device node, or empty.
static std::wstring FindBluetoothName(EnumContext& ctx, const std::wstring& mac)
{
    if (!ctx.bluetoothLoaded) {
        LoadDevNodes(GUID_CLASS_BLUETOOTH, ctx.bluetoothClass);
        ctx.bluetoothLoaded = true;
    }
    const std::wstring prefix = L"BTHLE\\DEV_" + mac;
    for (size_t i = 0; i < ctx.bluetoothClass.size(); ++i) {
        const std::wstring& id = ctx.bluetoothClass[i].instanceId;
        if (id.size() >= prefix.size() && id.compare(0, prefix.size(), prefix) == 0 &&
            (id.size() == prefix.size() || id[prefix.size()] == L'\\'))
            return ctx.bluetoothClass[i].name;
    }
    return L"";
}

// How one Raw Input keyboard handle appears in the Device menu.
struct KeyboardEntry {
    std::wstring savedPath;     // key in the INI ("__RDP__" for Remote Desktop)
    std::wstring groupKey;      // handles with the same key form one menu entry
    std::wstring displayName;
};

// Returns false if the handle is not a keyboard that should be listed.
static bool ClassifyKeyboard(EnumContext& ctx, const std::wstring& path, KeyboardEntry& out)
{
    const std::wstring upperPath = ToUpper(path);
    if (IsRdpKeyboardPath(upperPath)) {
        out.savedPath   = RDP_KEY;
        out.groupKey    = RDP_KEY;
        out.displayName = RDP_NAME;
        return true;
    }

    // Mice, pointing devices, game controllers, secondary HID collections
    if (Contains(upperPath, L"MOUSE") || Contains(upperPath, L"M720")  || Contains(upperPath, L"POINT") ||
        Contains(upperPath, L"IG_")   || Contains(upperPath, L"COL02") || Contains(upperPath, L"COL03")) {
        LOGF(L"enum: skip %s", path.c_str());
        return false;
    }

    const std::wstring instanceId = PathToInstanceId(path);
    const std::wstring mac        = ExtractBtMac(upperPath);
    const DevNode*     classNode  = FindKeyboardClassNode(ctx, instanceId);

    // Name: Bluetooth device name, else keyboard-class name, else the path
    std::wstring name;
    if (!mac.empty()) name = FindBluetoothName(ctx, mac);
    if (name.empty()) name = (classNode && classNode->hasName) ? classNode->name : path;

    const std::wstring upperName = ToUpper(name);
    if (Contains(upperName, L"MOUSE") || Contains(upperName, L"\u30DE\u30A6\u30B9") ||   // "mouse" in katakana
        Contains(upperName, L"M720")) {
        LOGF(L"enum: skip %s", path.c_str());
        return false;
    }

    // The class name is often just "HID Keyboard Device"; the product name
    // tells more ("Mouse for Windows" = the keyboard interface of a mouse).
    // Combo receivers such as "Wireless Keyboard and Mouse" are kept.
    const std::wstring product      = GetHidProductString(path);
    const std::wstring upperProduct = ToUpper(product);
    if (Contains(upperProduct, L"MOUSE") &&
        !Contains(upperProduct, L"KEYBOARD") && !Contains(upperProduct, L"KBD")) {
        LOGF(L"enum: skip %s", path.c_str());
        return false;
    }

    // Must be in the keyboard class, or be a Bluetooth HID keyboard
    if (!classNode && mac.empty()) {
        LOGF(L"enum: skip %s", path.c_str());
        return false;
    }

    for (size_t i = 0; i < ctx.excludes.size(); ++i) {
        if (Contains(upperPath, ctx.excludes[i].c_str())) {
            LOGF(L"enum: excluded %s", path.c_str());
            return false;
        }
    }

    out.savedPath   = path;
    out.displayName = name.empty() ? path : name;
    if (mac.empty()) {
        // Generic names like "HID Keyboard Device" are ambiguous: prefer the
        // product name reported by the device and add VID/PID.
        if (!product.empty()) out.displayName = product;
        const std::wstring vidPid = ExtractVidPid(upperPath);
        if (!vidPid.empty()) out.displayName += L"  [" + vidPid + L"]";
    }
    out.groupKey = !mac.empty() ? mac : instanceId;
    if (out.groupKey.empty()) out.groupKey = path;
    return true;
}

// ---------------------------------------------------------------------------
// Enumeration
// ---------------------------------------------------------------------------
static std::vector<RAWINPUTDEVICELIST> GetRawInputDevices()
{
    std::vector<RAWINPUTDEVICELIST> list;
    for (int attempt = 0; attempt < 4; ++attempt) {   // a device may be added between the two calls
        UINT count = 0;
        if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || count == 0) {
            list.clear();
            break;
        }
        list.resize(count);
        const UINT got = GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST));
        if (got != (UINT)-1) {
            list.resize(got);
            break;
        }
        list.clear();
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) break;
    }
    return list;
}

static std::wstring GetRawInputDevicePath(HANDLE hDevice)
{
    UINT length = 0;
    GetRawInputDeviceInfo(hDevice, RIDI_DEVICENAME, nullptr, &length);
    std::wstring path;
    if (length > 0) {
        path.resize(length, L'\0');
        if (GetRawInputDeviceInfo(hDevice, RIDI_DEVICENAME, &path[0], &length) == (UINT)-1) path.clear();
        while (!path.empty() && path.back() == L'\0') path.pop_back();
    }
    return path;
}

void EnumerateKeyboards()
{
    EnumContext ctx;
    LoadDevNodes(GUID_DEVCLASS_KEYBOARD, ctx.keyboardClass);
    if (IniExists()) ctx.savedDevices = LoadSavedDevices();
    ctx.excludes = LoadExcludeList();

    // Current check states, by group, so that a state survives even if the
    // order of a Bluetooth keyboard's collections changes.
    std::map<std::wstring, bool> previousChecked;
    for (size_t i = 0; i < s_devices.size(); ++i)
        previousChecked[s_devices[i].groupKey] = s_devices[i].checked;

    std::map<std::wstring, DeviceInfo>                 groups;        // by groupKey
    std::map<std::wstring, std::vector<std::wstring> > groupPaths;    // groupKey -> savedPath of each handle

    const std::vector<RAWINPUTDEVICELIST> rawDevices = GetRawInputDevices();
    for (size_t i = 0; i < rawDevices.size(); ++i) {
        if (rawDevices[i].dwType != RIM_TYPEKEYBOARD) continue;
        const HANDLE       hDevice = rawDevices[i].hDevice;
        const std::wstring path    = GetRawInputDevicePath(hDevice);

        KeyboardEntry entry;
        if (!ClassifyKeyboard(ctx, path, entry)) continue;

#if JKM_LOG
        {
            RID_DEVICE_INFO info = {};
            info.cbSize = sizeof(info);
            UINT infoSize = sizeof(info);
            GetRawInputDeviceInfo(hDevice, RIDI_DEVICEINFO, &info, &infoSize);
            LOGF(L"enum: use  %s (%s) hDev=%p type=%lu sub=%lu fkeys=%lu ind=%lu keys=%lu",
                 entry.savedPath.c_str(), entry.displayName.c_str(), hDevice,
                 info.keyboard.dwType, info.keyboard.dwSubType,
                 info.keyboard.dwNumberOfFunctionKeys, info.keyboard.dwNumberOfIndicators,
                 info.keyboard.dwNumberOfKeysTotal);
        }
#endif

        DeviceInfo& device = groups[entry.groupKey];
        if (device.handles.empty()) {           // first handle of this group
            device.savedPath   = entry.savedPath;
            device.displayName = entry.displayName;
            device.groupKey    = entry.groupKey;
        }
        device.handles.push_back(hDevice);
        groupPaths[entry.groupKey].push_back(entry.savedPath);
    }

    // Check state: current state > INI (any handle's path or the group key) > checked
    std::vector<DeviceInfo> devices;
    std::set<HANDLE>        checkedHandles;
    for (std::map<std::wstring, DeviceInfo>::iterator it = groups.begin(); it != groups.end(); ++it) {
        DeviceInfo& device = it->second;
        std::map<std::wstring, bool>::const_iterator prev = previousChecked.find(device.groupKey);
        if (prev != previousChecked.end()) {
            device.checked = prev->second;
        } else {
            std::vector<std::wstring> keys = groupPaths[device.groupKey];
            keys.push_back(device.groupKey);
            bool found = false, checked = false;
            for (size_t k = 0; k < keys.size(); ++k) {
                std::map<std::wstring, bool>::const_iterator saved = ctx.savedDevices.find(keys[k]);
                if (saved != ctx.savedDevices.end()) {
                    found   = true;
                    checked = checked || saved->second;
                }
            }
            device.checked = found ? checked : true;   // new keyboards start checked
        }
        if (device.checked)
            checkedHandles.insert(device.handles.begin(), device.handles.end());
        devices.push_back(device);
    }

    s_devices.swap(devices);
    AcquireSRWLockExclusive(&s_checkedLock);
    s_checkedHandles.swap(checkedHandles);
    s_anyChecked = !s_checkedHandles.empty();
    ReleaseSRWLockExclusive(&s_checkedLock);
}
