# JISKeyMapper

A resident tool for Windows that remaps keystrokes to the correct characters when the physical keyboard layout does not match the Windows keyboard layout.

[日本語 README](README.md)

---

## Overview

Windows does not adjust key input to match the physical keyboard that is connected. For example, on a Windows PC set to the English (US) layout, pressing `Shift+2` on a JIS keyboard produces `@` instead of `"`. JISKeyMapper intercepts keystrokes system-wide and remaps them so that each key produces the character printed on it.

| Mode | Use case |
|---|---|
| **US_TO_JIS** (default) | Physical JIS keyboard on a US-layout Windows |
| **JIS_TO_US** | Physical US keyboard on a JIS-layout Windows |

When several keyboards are connected at the same time, you can choose which ones are remapped. For example, you can remap only the JIS keyboard and use a US keyboard unchanged.

---

## Features

- Remaps symbol keys, shifted number-row keys, and JIS-specific keys (Henkan, Muhenkan, Katakana/Hiragana, Ro, Yen)
- Hankaku/Zenkaku key toggles the IME on a US-layout Windows
- Per-keyboard enable/disable (Device menu)
- Global on/off switch (left-click the tray icon, or Enable in the menu)
- IME compatible: input is injected as virtual-key codes, never as Unicode characters
- Shortcuts with Ctrl/Alt are passed through unchanged
- Bluetooth keyboards are listed by device name
- USB devices are listed by product name and VID/PID
- Keyboard collections of mice and tablets are excluded automatically; any device can also be excluded manually in the INI file
- Works inside Remote Desktop sessions
- Detects keyboard connection and disconnection automatically
- Settings are saved to `JISKeyMapper.ini` automatically
- The tray icon is restored automatically after Explorer restarts
- Prevents multiple instances; Hi-DPI aware (Per-Monitor V2)

---

## Requirements

- Windows 10 or later (x64)
- Administrator privileges (a UAC prompt appears at startup)
- Visual Studio 2019 or 2022 for building

The tool runs elevated so that remapping also works in applications running as administrator (such as an elevated Command Prompt).

---

## Build

Open **x64 Native Tools Command Prompt for VS 2019/2022** and run:

```bat
cd C:\path\to\JISKeyMapper
nmake
```

| Command | Description |
|---|---|
| `nmake` | Release build |
| `nmake DEBUG=1` | Debug build |
| `nmake clean` | Remove generated files |
| `nmake rebuild` | Clean, then build |

`JISKeyMapper.exe` is created in the project root. No DLLs or other files are required.

---

## Usage

Run `JISKeyMapper.exe`. An icon appears in the notification area (system tray).

### Tray icon

| Action | Result |
|---|---|
| Left-click | Toggle Enable on/off |
| Right-click | Open the menu |

The badge at the bottom right of the icon shows the state.

| Color | State |
|---|---|
| Green | Enable is on and at least one device is checked (remapping) |
| Gray | Enable is off, or no device is checked (not remapping) |

Hovering over the icon shows `ON`, `OFF (disabled)`, or `OFF (no device checked)`. The icon label (JP/US) shows the current mode.

### Menu

| Item | Description |
|---|---|
| **Enable** | When unchecked, remapping stops for all keyboards |
| **Mode** | Selects the remapping mode |
| **Device** | Lists the connected keyboards. Only checked keyboards are remapped |
| **Exit** | Quits the program |

The Mode menu offers two choices:

| Menu item | When to use |
|---|---|
| JIS keyboard on US-layout Windows | JIS keyboard, Windows set to English (US) layout (US_TO_JIS) |
| US keyboard on JIS-layout Windows | US keyboard, Windows set to Japanese layout (JIS_TO_US) |

Devices are named as follows:

- Bluetooth keyboards: device name (e.g. `Keychron K1 Max`)
- USB keyboards: product name reported by the device, plus VID/PID (e.g. `USB KEYBOARD  [VID_258A&PID_000C]`)
- Remote Desktop: `Remote Desktop keyboard`

A newly connected keyboard is added with its check mark on.

---

## Key map (US_TO_JIS)

Characters produced by the keys of a JIS keyboard. Keys not listed (letters, etc.) are passed through unchanged.

| JIS key | Output | With Shift |
|---|---|---|
| `2` | `2` | `"` |
| `6` | `6` | `&` |
| `7` | `7` | `'` |
| `8` | `8` | `(` |
| `9` | `9` | `)` |
| `0` | `0` | (nothing) |
| `-` | `-` | `=` |
| `^` | `^` | `~` |
| `@` | `@` | `` ` `` |
| `[` | `[` | `{` |
| `;` | `;` | `+` |
| `:` | `:` | `*` |
| `]` | `]` | `}` |
| `\` (Ro) | `\` | `_` |
| `¥` | `\` | `\|` |
| Hankaku/Zenkaku | IME on/off | `~` |
| Henkan / Muhenkan / Katakana-Hiragana | Sent as the corresponding IME keys | Same |

---

## Key map (JIS_TO_US)

Characters produced by the keys of a US keyboard; each key produces the character printed on it. Keys not listed (letters, `1` `3` `4` `5` `,` `.` `/`, etc.) produce the same character on both layouts and are passed through unchanged.

| US key | Output | With Shift |
|---|---|---|
| `2` | `2` | `@` |
| `6` | `6` | `^` |
| `7` | `7` | `&` |
| `8` | `8` | `*` |
| `9` | `9` | `(` |
| `0` | `0` | `)` |
| `-` | `-` | `_` |
| `=` | `=` | `+` |
| `[` | `[` | `{` |
| `]` | `]` | `}` |
| `\` | `\` | `\|` |
| `;` | `;` | `:` |
| `'` | `'` | `"` |
| `` ` `` | `` ` `` | `~` |

Toggle the IME with `` Alt+` `` (the same as on a US-layout Windows). The `` ` `` key sits where the Hankaku/Zenkaku key is on the JIS layout, and with Alt it is passed through unchanged.

---

## Settings file (JISKeyMapper.ini)

The file is created automatically in the same folder as the EXE. Normally the menu is all you need; only `Exclude` is edited by hand.

```ini
[Settings]
Mode=US_TO_JIS
Enabled=1

[Devices]
Exclude=VID_2F24&PID_0135;VID_1234&PID_5678
Count=2
Path0=\\?\HID#VID_258A&PID_000C&MI_00#...
Check0=0
Path1=__RDP__
Check1=1
```

| Section / key | Description |
|---|---|
| `[Settings]` `Mode` | `US_TO_JIS` or `JIS_TO_US` |
| `[Settings]` `Enabled` | `1` = enabled, `0` = disabled |
| `[Devices]` `Exclude` | Devices to hide from the Device menu. Semicolon-separated substrings of the device path (such as `VID_xxxx&PID_xxxx`), case-insensitive |
| `[Devices]` `PathN` / `CheckN` | Path and check state of each keyboard. The Remote Desktop keyboard is saved as `__RDP__` |

Restart JISKeyMapper after changing `Exclude`. An `Exclude` written under `[Settings]` by an earlier version is moved to `[Devices]` automatically at startup. Settings of devices that are not currently connected are kept, so a Bluetooth keyboard keeps its check state when it reconnects.

---

## How it works

### Architecture

JISKeyMapper runs on two threads.

| Thread | Responsibilities |
|---|---|
| Main thread | Tray icon, menu, device enumeration, INI file |
| Hook thread | Low-level keyboard hook (`WH_KEYBOARD_LL`) and Raw Input (`WM_INPUT`) |

Windows may silently remove a low-level hook that does not return within LowLevelHooksTimeout. Slow work such as device enumeration (SetupDi API) and menus therefore runs on a separate thread from the hook. The only data shared between the threads is the set of checked device handles, which is protected by an SRW lock. This data is kept inside Devices.cpp; the hook thread (Hook.cpp) reaches it only through functions such as `IsCheckedDevice`.

### Intercepting and injecting keys

A `WH_KEYBOARD_LL` hook receives every physical keystroke. For keys that need remapping, the hook discards the original event (`return 1`) and injects a different virtual-key code with `SendInput`.

- **Virtual-key injection:** Keys are sent as virtual-key codes rather than characters (`KEYEVENTF_UNICODE`), so the IME treats them as ordinary key input, even during composition.
- **Temporary Shift changes:** For example, JIS `Shift+7` (`'`) is the unshifted `'` key on the US layout. In this case the Shift keys that are held (left and right handled separately) are released briefly, the key is sent, and Shift is restored. Some keys need Shift pressed briefly instead.
- **Ignoring its own input:** Events sent with `SendInput` carry the `LLKHF_INJECTED` flag and are ignored by the hook, which prevents remapping loops.
- **Matching press and release:** The decision made at keydown (remap, pass through, or suppress) is recorded per key, and the keyup is always handled the same way. Releasing Shift, toggling Enable, or switching modes while a key is held never leaves a key stuck down.
- **Shortcut protection:** Nothing is remapped while Ctrl or Alt is held.

### Identifying the keyboard

The low-level hook does not report which keyboard a keystroke came from. JISKeyMapper therefore also uses Raw Input (`RegisterRawInputDevices` with `RIDEV_INPUTSINK`) and identifies the keyboard by the device handle (`hDevice`) in `WM_INPUT`.

There is a catch: `WM_INPUT` is generated only after the hook lets an event through. A key discarded by the hook never produces `WM_INPUT`, and when the hook makes its decision, the source of that very key is not yet known. JISKeyMapper estimates the source as follows.

1. **Previous input:** From the `WM_INPUT` of keys that passed the hook, the device of the most recent keydown is recorded. The next key is normally assumed to come from the same keyboard.
2. **Held keys:** The keys held on each device are tracked. If every key on the last-used device has been released while another device still has a key such as Shift held, that other device is taken as the source. Each record is checked with `GetAsyncKeyState` when it is used, and stale records (whose release was missed) are discarded.
3. **Hankaku/Zenkaku decided on release:** The Hankaku/Zenkaku key (scan code `0x29`) has the same scan code as the `` ` `` key on a US keyboard. A wrong guess would toggle the IME unexpectedly, so this key is identified exactly.
   - The keydown is discarded and held back.
   - The keyup is let through, so its own `WM_INPUT` arrives with the correct `hDevice`.
   - If it came from a checked keyboard, `VK_KANJI` is injected to toggle the IME; otherwise the held-back `` ` `` is injected.
   - If no `WM_INPUT` arrives within 0.3 seconds, methods 1 and 2 are used instead.

### Device enumeration

- **Listing:** Keyboards are enumerated with `GetRawInputDeviceList`.
- **Real keyboards:** The SetupDi API checks whether each device is registered in the keyboard class (`GUID_DEVCLASS_KEYBOARD`).
- **Excluding mice and similar devices:** Keyboard collections of mice, tablets, game controllers, and similar devices are excluded based on the path, the device name, and the HID product string (`HidD_GetProductString`). Devices whose product name contains both Mouse and Keyboard (such as keyboard-and-mouse combo receivers) are kept.
- **Bluetooth keyboards:** One keyboard appears as several HID collections, so they are grouped into one entry by the MAC address in the path. The name is read from the `BTHLE\DEV_<MAC>` device entry.
- **Remote Desktop:** Virtual keyboards report an empty path and are grouped into one entry as `__RDP__`.
- **Connect/disconnect detection:** `WM_INPUT_DEVICE_CHANGE` (via `RIDEV_DEVNOTIFY`) triggers a re-enumeration after 0.5 seconds, which merges bursts of notifications.

### Diagnostic log

Set `#define JKM_LOG 0` in `src\Common.h` to `1` and rebuild. The following information is then written to `JISKeyMapper.log` in the EXE folder:

- Device enumeration results (accepted or excluded, path, keyboard information)
- How each Hankaku/Zenkaku keystroke was decided

---

## Limitations

### First key after switching keyboards

That single key is not remapped when all of the following apply:

- The previous input came from the US keyboard
- No key such as Shift is being held
- The first key pressed on the JIS keyboard is a remapped key (`@`, `[`, `:`, etc.)

This is because the source of that key is not yet known when the hook decides (see "Identifying the keyboard"). Input starting with Shift held, or starting with a letter key, is remapped correctly.

### Shift held on another keyboard

If you hold Shift on the JIS keyboard and press a remapped key on the US keyboard, the key is treated as JIS keyboard input and remapped.

### Hankaku/Zenkaku key

- The IME toggles when the key is released.
- The `` ` `` key on a US keyboard is also typed when released, and does not auto-repeat when held.
- `Shift+Hankaku/Zenkaku` produces `~` instead of cycling the IME input mode.

### Shortcuts with Ctrl/Alt

Input containing Ctrl or Alt is passed through unchanged, so shortcuts that use a key whose virtual-key code differs between JIS and US may not work as expected. For example, in US_TO_JIS mode the `:` key sends the `;` virtual-key code, so VSCode's `Shift+Ctrl+:` (zoom in) arrives as `Shift+Ctrl+;`.

In VSCode, add the following to `keybindings.json` as a workaround:

```json
{ "key": "ctrl+shift+oem_1", "command": "editor.action.fontZoomIn" },
{ "key": "ctrl+oem_minus",   "command": "editor.action.fontZoomOut" }
```

### Remote Desktop

On the remote side of a Remote Desktop session, all local keyboards arrive as a single virtual keyboard. JISKeyMapper running on the remote side cannot tell the local keyboards apart.

### Screens where remapping does not work

On the secure desktop (UAC prompts, the lock screen, etc.) low-level hooks are not called, so keys are not remapped.

### JIS_TO_US mode

- The CapsLock key keeps its JIS-layout behavior (Eisu key; CapsLock is Shift+CapsLock).
- When a JIS keyboard and a US keyboard are used together, the JIS keyboard's Hankaku/Zenkaku key pressed right after switching keyboards may be mistaken for the US keyboard's `` ` `` key. The release-time decision used in US_TO_JIS mode is not applied in this mode.

---

## Directory structure

```
JISKeyMapper\
+-- Makefile
+-- README.md                     Japanese README
+-- README.en.md                  English README (this file)
+-- src\
|   +-- Common.h                  Shared constants (message/menu IDs), mode, Enable, diagnostic log
|   +-- Main.cpp                  Entry point and main window
|   +-- KeyMap.h / .cpp           Key maps (US_TO_JIS / JIS_TO_US) and lookup
|   +-- Settings.h / .cpp         JISKeyMapper.ini
|   +-- Devices.h / .cpp          Keyboard enumeration, classification and check state
|   +-- Hook.h / .cpp             Hook thread (low-level hook, Raw Input, keyboard identification, key injection)
|   +-- Tray.h / .cpp             Tray icon and menu
|   +-- StringUtil.h / .cpp       String helpers
|   +-- JISKeyMapper.rc           Resource script
|   \-- JISKeyMapper.exe.manifest Manifest (requireAdministrator, DPI awareness)
+-- res\
|   +-- app.ico                   Application icon
|   +-- on_jp.ico / off_jp.ico    Tray icons: US_TO_JIS mode (remapping / stopped)
|   \-- on_us.ico / off_us.ico    Tray icons: JIS_TO_US mode (remapping / stopped)
\-- build\                        Intermediate files (created automatically)
```

At runtime, `JISKeyMapper.ini` (settings) is created in the EXE folder. If `JKM_LOG` is enabled, `JISKeyMapper.log` is created as well.
