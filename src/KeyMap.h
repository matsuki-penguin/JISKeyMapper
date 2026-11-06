// KeyMap.h
// Key maps for both modes: which virtual key (and Shift state) to send for
// each physical key.
#pragma once

#include "Common.h"

#define SC_HANKAKU  0x29    // Hankaku/Zenkaku (JIS) = ` (US); handled by the hook, not by the key maps

// What to do with Shift while sending the output key.
// PRESS is applied only if Shift is not held, RELEASE only if it is held.
enum ShiftOp : BYTE {
    SHIFT_KEEP,         // leave Shift as the user holds it
    SHIFT_PRESS,        // press Shift around the output key
    SHIFT_RELEASE,      // release the held Shift around the output key
};

static const WORD KEY_PASS     = 0;        // send the original key unchanged
static const WORD KEY_SUPPRESS = 0xFFFF;   // send nothing

struct KeyOutput {
    WORD    vk;         // virtual-key code to send, or KEY_PASS / KEY_SUPPRESS
    ShiftOp shift;
};

struct KeyMapEntry {
    WORD      scanCode; // physical key
    KeyOutput normal;   // without Shift
    KeyOutput shifted;  // with Shift
};

enum MapResult { MAP_PASS, MAP_SUPPRESS, MAP_REMAP };

// Call once at startup.
void BuildKeyMapIndex();

// Entry for scanCode in the current mode, or nullptr if the key is not remapped.
const KeyMapEntry* FindKeyMapEntry(WORD scanCode);

// Output for the current Shift state.
MapResult GetOutput(const KeyMapEntry& entry, bool shift, KeyOutput& out);
