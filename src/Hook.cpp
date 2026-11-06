// Hook.cpp
//
// Identifying the source keyboard
//   The low-level hook does not tell which keyboard a key came from; Raw Input
//   does.  But WM_INPUT is generated only after the hook lets an event through:
//   an event discarded by the hook (return 1) produces no WM_INPUT at all.
//   * Normal keys: the source is guessed from the keyboards seen in WM_INPUT
//     so far (see GuessSourceDevice).
//   * Hankaku/Zenkaku (scan code 0x29, the same as ` on a US keyboard) in
//     US_TO_JIS mode: decided on release.  The keydown is held back, the keyup
//     is let through so that its WM_INPUT reports the real device, and then
//     VK_KANJI (checked keyboard) or ` (unchecked keyboard) is sent.
//
// Everything in this file runs on the hook thread.  The only data shared with
// the main thread is read through Devices.h and the atomics in Common.h.
#include "Hook.h"
#include "Devices.h"
#include "KeyMap.h"

#include <map>

#define TIMER_HANKAKU       1       // Hankaku/Zenkaku keyup WM_INPUT did not arrive
#define HANKAKU_TIMEOUT_MS  300

static const wchar_t HOOK_CLASS_NAME[] = L"JISKeyMapperHookWnd";

// ---------------------------------------------------------------------------
// State (hook thread only)
// ---------------------------------------------------------------------------
static HANDLE s_hThread         = nullptr;
static DWORD  s_threadId        = 0;
static HWND   s_hNotifyWnd      = nullptr;  // main window, receives WM_APP_DEVICE_CHANGE
static HWND   s_hHookWnd        = nullptr;  // receives WM_INPUT
static HHOOK  s_hHook           = nullptr;
static HANDLE s_lastKeyDevice   = NULL;     // device of the last keydown seen in WM_INPUT
static int    s_hankakuPending  = 0;        // Hankaku/Zenkaku keyups waiting for their WM_INPUT

// Keys currently held, per device, as seen in WM_INPUT.
typedef std::map<USHORT, USHORT> HeldKeys;  // (MakeCode | E0 << 8) -> VKey
static std::map<HANDLE, HeldKeys> s_heldKeys;

// Decision for each physical key, made on its first keydown and kept until
// its keyup, so that the keydown and the keyup are always handled the same way.
enum KeyAction : BYTE {
    ACT_NONE,               // key is up
    ACT_PASS,               // original key passed through
    ACT_REMAP,              // sentVk was sent instead
    ACT_SUPPRESS,           // nothing was sent
    ACT_HANKAKU_PENDING,    // Hankaku/Zenkaku held back until its keyup
};
struct KeyState {
    KeyAction action = ACT_NONE;
    WORD      sentVk = 0;
};
static KeyState s_keyStates[512];   // index: scan code | (extended ? 0x100 : 0)

// ---------------------------------------------------------------------------
// Sending keys
// ---------------------------------------------------------------------------
// scanCode = 0: derive the scan code from vk with the current layout.
static INPUT MakeKeyInput(WORD vk, bool keyUp, WORD scanCode = 0)
{
    INPUT input = {};
    input.type       = INPUT_KEYBOARD;
    input.ki.wVk     = vk;
    input.ki.wScan   = scanCode ? scanCode : (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    input.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
    return input;
}

// Sends the keydown of vk, pressing or releasing Shift around it as requested.
// Exactly the Shift keys that are held (left and/or right) are released and restored.
static void SendOutputKeyDown(WORD vk, ShiftOp shiftOp)
{
    const bool leftShift  = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0;
    const bool rightShift = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
    const bool shiftHeld  = leftShift || rightShift;
    const bool release    = (shiftOp == SHIFT_RELEASE) && shiftHeld;
    const bool press      = (shiftOp == SHIFT_PRESS)   && !shiftHeld;

    INPUT inputs[6];
    UINT  count = 0;
    if (release) {
        if (leftShift)  inputs[count++] = MakeKeyInput(VK_LSHIFT, true);
        if (rightShift) inputs[count++] = MakeKeyInput(VK_RSHIFT, true);
    } else if (press) {
        inputs[count++] = MakeKeyInput(VK_LSHIFT, false);
    }
    inputs[count++] = MakeKeyInput(vk, false);
    if (release) {
        if (leftShift)  inputs[count++] = MakeKeyInput(VK_LSHIFT, false);
        if (rightShift) inputs[count++] = MakeKeyInput(VK_RSHIFT, false);
    } else if (press) {
        inputs[count++] = MakeKeyInput(VK_LSHIFT, true);
    }
    SendInput(count, inputs, sizeof(INPUT));
}

static void SendKeyUp(WORD vk)
{
    INPUT input = MakeKeyInput(vk, true);
    SendInput(1, &input, sizeof(INPUT));
}

// Completes a held-back Hankaku/Zenkaku key once its source keyboard is known.
static void FinishHankaku(bool fromCheckedDevice)
{
    INPUT inputs[2];
    if (fromCheckedDevice) {
        inputs[0] = MakeKeyInput(VK_KANJI, false, 1);            // toggle the IME
        inputs[1] = MakeKeyInput(VK_KANJI, true,  1);
    } else {
        inputs[0] = MakeKeyInput(VK_OEM_3, false, SC_HANKAKU);   // `, since its keydown was held back
        inputs[1] = MakeKeyInput(VK_OEM_3, true,  SC_HANKAKU);
    }
    SendInput(2, inputs, sizeof(INPUT));
}

// ---------------------------------------------------------------------------
// Source keyboard guess
// ---------------------------------------------------------------------------
// True if a key went down on this device (per WM_INPUT) and is still held.
// Records whose key is no longer down (e.g. a lost keyup) are dropped.
static bool DeviceHasHeldKeys(HANDLE hDevice)
{
    std::map<HANDLE, HeldKeys>::iterator device = s_heldKeys.find(hDevice);
    if (device == s_heldKeys.end()) return false;
    HeldKeys& keys = device->second;
    for (HeldKeys::iterator key = keys.begin(); key != keys.end(); ) {
        if (GetAsyncKeyState(key->second) & 0x8000) return true;
        key = keys.erase(key);
    }
    return false;
}

// Which keyboard most likely sent the key the hook is handling now:
//   1. the device of the last keydown, if it still has a key held
//   2. otherwise, the only other device that still has a key held
//      (e.g. Shift held on the JIS keyboard after 'a' was typed and released
//       on the US keyboard)
//   3. otherwise, the device of the last keydown
static HANDLE GuessSourceDevice()
{
    const HANDLE last = s_lastKeyDevice;
    if (last == NULL || DeviceHasHeldKeys(last)) return last;

    HANDLE other = NULL;
    int    count = 0;
    for (std::map<HANDLE, HeldKeys>::iterator it = s_heldKeys.begin(); it != s_heldKeys.end(); ++it) {
        if (it->first != last && DeviceHasHeldKeys(it->first)) {
            other = it->first;
            ++count;
        }
    }
    return (count == 1) ? other : last;
}

// ---------------------------------------------------------------------------
// Low-level keyboard hook
// ---------------------------------------------------------------------------
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode != HC_ACTION)
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);

    const KBDLLHOOKSTRUCT* kbd = (const KBDLLHOOKSTRUCT*)lParam;
    if (kbd->flags & LLKHF_INJECTED)         // sent by SendInput (ours or another tool's)
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);

    const bool keyDown  = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    const WORD scanCode = (WORD)(kbd->scanCode & 0xFF);
    const bool extended = (kbd->flags & LLKHF_EXTENDED) != 0;
    KeyState&  state    = s_keyStates[scanCode | (extended ? 0x100 : 0)];

    // ---- keyup: handled the same way as its keydown ----
    if (!keyDown) {
        const KeyState pressed = state;
        state = KeyState();
        switch (pressed.action) {
        case ACT_REMAP:
            SendKeyUp(pressed.sentVk);
            return 1;
        case ACT_SUPPRESS:
            return 1;
        case ACT_HANKAKU_PENDING:
            // Let the keyup through: its WM_INPUT tells which keyboard it came from.
            ++s_hankakuPending;
            SetTimer(s_hHookWnd, TIMER_HANKAKU, HANKAKU_TIMEOUT_MS, nullptr);
            LOGF(L"Hankaku keyup passed, pending=%d", s_hankakuPending);
            return CallNextHookEx(s_hHook, nCode, wParam, lParam);
        default:
            return CallNextHookEx(s_hHook, nCode, wParam, lParam);
        }
    }

    // ---- keydown, including auto-repeat ----
    if (state.action == ACT_PASS)
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);
    if (state.action == ACT_SUPPRESS || state.action == ACT_HANKAKU_PENDING)
        return 1;

    const bool shift   = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
    const bool ctrlAlt = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0
                      || (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;

    if (state.action == ACT_NONE) {
        // First keydown: decide whether this key is remapped.
        bool remap = false;
        if (!extended && !ctrlAlt && IsRemappingActive()) {
            if (scanCode == SC_HANKAKU && g_layoutMode == LAYOUT_US_TO_JIS) {
                if (!shift) {
                    // The source keyboard is decided on the keyup (see the top of this file).
                    state.action = ACT_HANKAKU_PENDING;
                    LOGF(L"Hankaku keydown held back");
                    return 1;
                }
                // Shift+Hankaku/Zenkaku: passed through (~)
            } else if (FindKeyMapEntry(scanCode)) {
                const HANDLE source = GuessSourceDevice();
                remap = (source == NULL) || IsCheckedDevice(source);
            }
        }
        if (!remap) {
            state.action = ACT_PASS;
            return CallNextHookEx(s_hHook, nCode, wParam, lParam);
        }
    }

    // First keydown of a remapped key, or auto-repeat of one.
    // The output is re-evaluated on repeat because Shift may have changed.
    const KeyMapEntry* entry  = FindKeyMapEntry(scanCode);
    KeyOutput          output = { KEY_PASS, SHIFT_KEEP };
    const MapResult    result = entry ? GetOutput(*entry, shift, output) : MAP_PASS;

    if (state.action == ACT_REMAP && (result != MAP_REMAP || output.vk != state.sentVk)) {
        SendKeyUp(state.sentVk);    // release the previous output first
        state = KeyState();
    }
    if (result == MAP_PASS) {
        state.action = ACT_PASS;
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);
    }
    if (result == MAP_SUPPRESS) {
        state.action = ACT_SUPPRESS;
        return 1;
    }

    SendOutputKeyDown(output.vk, output.shift);
    state.action = ACT_REMAP;
    state.sentVk = output.vk;
    return 1;
}

// ---------------------------------------------------------------------------
// Hook thread window: Raw Input
// ---------------------------------------------------------------------------
static LRESULT CALLBACK HookWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INPUT: {
        // Only keyboards are registered, so one RAWINPUT on the stack is enough.
        RAWINPUT raw;
        UINT     size = sizeof(raw);
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
            raw.header.dwType == RIM_TYPEKEYBOARD)
        {
            const HANDLE       hDevice = raw.header.hDevice;    // NULL for SendInput events
            const RAWKEYBOARD& kb      = raw.data.keyboard;
            const bool         keyUp   = (kb.Flags & RI_KEY_BREAK) != 0;
            const bool         e0      = (kb.Flags & RI_KEY_E0)    != 0;
            if (hDevice != NULL) {
                const USHORT key = (USHORT)(kb.MakeCode | (e0 ? 0x100 : 0));
                if (!keyUp) {
                    s_lastKeyDevice = hDevice;
                    if (kb.VKey != 0 && kb.VKey != 0xFF)    // 0xFF: fake Shift and similar
                        s_heldKeys[hDevice][key] = kb.VKey;
                } else {
                    std::map<HANDLE, HeldKeys>::iterator device = s_heldKeys.find(hDevice);
                    if (device != s_heldKeys.end()) device->second.erase(key);
                }

                if (keyUp && s_hankakuPending > 0 && kb.MakeCode == SC_HANKAKU && !e0) {
                    if (--s_hankakuPending == 0) KillTimer(hWnd, TIMER_HANKAKU);
                    s_lastKeyDevice = hDevice;
                    const bool checked = IsCheckedDevice(hDevice);
                    LOGF(L"Hankaku keyup WM_INPUT hDev=%p checked=%d", hDevice, checked ? 1 : 0);
                    FinishHankaku(checked);
                }
            }
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    case WM_TIMER:
        if (wParam == TIMER_HANKAKU) {
            KillTimer(hWnd, TIMER_HANKAKU);
            // The keyup's WM_INPUT did not arrive: fall back to the guess.
            const HANDLE source  = GuessSourceDevice();
            const bool   checked = (source == NULL) || IsCheckedDevice(source);
            while (s_hankakuPending > 0) {
                --s_hankakuPending;
                LOGF(L"Hankaku timeout, checked=%d", checked ? 1 : 0);
                FinishHankaku(checked);
            }
        }
        return 0;

    case WM_INPUT_DEVICE_CHANGE:
        if (s_hNotifyWnd) PostMessage(s_hNotifyWnd, WM_APP_DEVICE_CHANGE, 0, 0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Thread
// ---------------------------------------------------------------------------
// Passed to the hook thread; valid only until readyEvent is signaled.
struct HookThreadStartup {
    HANDLE readyEvent;
    bool   succeeded;
};

static DWORD WINAPI HookThreadProc(LPVOID param)
{
    HookThreadStartup* startup = (HookThreadStartup*)param;
    HINSTANCE          hInst   = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = HookWndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = HOOK_CLASS_NAME;
    RegisterClassExW(&wc);

    s_hHookWnd = CreateWindowExW(0, HOOK_CLASS_NAME, L"", 0, 0, 0, 0, 0,
                                 HWND_MESSAGE, nullptr, hInst, nullptr);
    bool succeeded = false;
    if (s_hHookWnd) {
        RAWINPUTDEVICE rid = {};
        rid.usUsagePage = 0x01;     // generic desktop
        rid.usUsage     = 0x06;     // keyboard
        rid.dwFlags     = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
        rid.hwndTarget  = s_hHookWnd;
        if (RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
            s_hHook   = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInst, 0);
            succeeded = (s_hHook != nullptr);
        }
    }

    startup->succeeded = succeeded;
    SetEvent(startup->readyEvent);  // 'startup' must not be used after this
    if (!succeeded) {
        if (s_hHookWnd) { DestroyWindow(s_hHookWnd); s_hHookWnd = nullptr; }
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWindowsHookEx(s_hHook);
    s_hHook = nullptr;
    DestroyWindow(s_hHookWnd);
    s_hHookWnd = nullptr;
    return 0;
}

bool StartHookThread(HWND notifyWnd)
{
    s_hNotifyWnd = notifyWnd;

    HookThreadStartup startup = {};
    startup.readyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    s_hThread = CreateThread(nullptr, 0, HookThreadProc, &startup, 0, &s_threadId);
    if (s_hThread) WaitForSingleObject(startup.readyEvent, INFINITE);
    CloseHandle(startup.readyEvent);

    if (!s_hThread || !startup.succeeded) {
        if (s_hThread) {
            WaitForSingleObject(s_hThread, 1000);
            CloseHandle(s_hThread);
            s_hThread = nullptr;
        }
        return false;
    }
    SetThreadPriority(s_hThread, THREAD_PRIORITY_HIGHEST);
    return true;
}

void StopHookThread()
{
    if (!s_hThread) return;
    PostThreadMessageW(s_threadId, WM_QUIT, 0, 0);
    WaitForSingleObject(s_hThread, 3000);
    CloseHandle(s_hThread);
    s_hThread = nullptr;
}
