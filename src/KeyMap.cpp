// KeyMap.cpp
#include "KeyMap.h"

// US_TO_JIS: JIS keyboard on a US-layout Windows.
// Each JIS key is sent as the US-layout VK (+/- Shift) that produces the
// character printed on the JIS key.  Hankaku/Zenkaku (0x29) is not in this
// table; the hook handles it (see Hook.cpp).
static const KeyMapEntry KEYMAP_US_TO_JIS[] = {
    //  JIS key     without Shift                        with Shift
    { 0x03, { '2',           SHIFT_KEEP    }, { VK_OEM_7,      SHIFT_PRESS   } },  // 2  "
    { 0x07, { '6',           SHIFT_KEEP    }, { '7',           SHIFT_PRESS   } },  // 6  &
    { 0x08, { '7',           SHIFT_KEEP    }, { VK_OEM_7,      SHIFT_RELEASE } },  // 7  '
    { 0x09, { '8',           SHIFT_KEEP    }, { '9',           SHIFT_PRESS   } },  // 8  (
    { 0x0A, { '9',           SHIFT_KEEP    }, { '0',           SHIFT_PRESS   } },  // 9  )
    { 0x0B, { '0',           SHIFT_KEEP    }, { KEY_SUPPRESS,  SHIFT_KEEP    } },  // 0  (nothing)
    { 0x0C, { VK_OEM_MINUS,  SHIFT_KEEP    }, { VK_OEM_PLUS,   SHIFT_RELEASE } },  // -  =
    { 0x0D, { '6',           SHIFT_PRESS   }, { VK_OEM_3,      SHIFT_PRESS   } },  // ^  ~
    { 0x1A, { '2',           SHIFT_PRESS   }, { VK_OEM_3,      SHIFT_RELEASE } },  // @  `
    { 0x1B, { VK_OEM_4,      SHIFT_KEEP    }, { VK_OEM_4,      SHIFT_KEEP    } },  // [  {
    { 0x27, { VK_OEM_1,      SHIFT_KEEP    }, { VK_OEM_PLUS,   SHIFT_PRESS   } },  // ;  +
    { 0x28, { VK_OEM_1,      SHIFT_PRESS   }, { '8',           SHIFT_PRESS   } },  // :  *
    { 0x2B, { VK_OEM_6,      SHIFT_KEEP    }, { VK_OEM_6,      SHIFT_KEEP    } },  // ]  }
    { 0x70, { VK_OEM_ATTN,   SHIFT_KEEP    }, { VK_OEM_ATTN,   SHIFT_KEEP    } },  // Katakana/Hiragana
    { 0x73, { VK_OEM_102,    SHIFT_KEEP    }, { VK_OEM_MINUS,  SHIFT_PRESS   } },  // Ro: \  _
    { 0x79, { VK_CONVERT,    SHIFT_KEEP    }, { VK_CONVERT,    SHIFT_KEEP    } },  // Henkan
    { 0x7B, { VK_NONCONVERT, SHIFT_KEEP    }, { VK_NONCONVERT, SHIFT_KEEP    } },  // Muhenkan
    { 0x7D, { VK_OEM_5,      SHIFT_KEEP    }, { VK_OEM_5,      SHIFT_KEEP    } },  // Yen: \  |
};

// JIS_TO_US: US keyboard on a JIS-layout (kbd106) Windows.
// Each US key is sent as the JIS-layout VK (+/- Shift) that produces the
// character printed on the US key.  JIS-layout VKs:
//   VK_OEM_1 = : *    VK_OEM_PLUS = ; +    VK_OEM_3 = @ `    VK_OEM_7 = ^ ~
//   VK_OEM_4 = [ {    VK_OEM_6    = ] }    VK_OEM_5 = \ |    VK_OEM_102 = \ _
//   VK_OEM_MINUS = - =
static const KeyMapEntry KEYMAP_JIS_TO_US[] = {
    //  US key      without Shift                        with Shift
    { 0x03, { KEY_PASS,      SHIFT_KEEP    }, { VK_OEM_3,      SHIFT_RELEASE } },  // 2  @  (@ key)
    { 0x07, { KEY_PASS,      SHIFT_KEEP    }, { VK_OEM_7,      SHIFT_RELEASE } },  // 6  ^  (^ key)
    { 0x08, { KEY_PASS,      SHIFT_KEEP    }, { '6',           SHIFT_KEEP    } },  // 7  &  (Shift+6)
    { 0x09, { KEY_PASS,      SHIFT_KEEP    }, { VK_OEM_1,      SHIFT_KEEP    } },  // 8  *  (Shift+:)
    { 0x0A, { KEY_PASS,      SHIFT_KEEP    }, { '8',           SHIFT_KEEP    } },  // 9  (  (Shift+8)
    { 0x0B, { KEY_PASS,      SHIFT_KEEP    }, { '9',           SHIFT_KEEP    } },  // 0  )  (Shift+9)
    { 0x0C, { KEY_PASS,      SHIFT_KEEP    }, { VK_OEM_102,    SHIFT_KEEP    } },  // -  _  (Shift+Ro)
    { 0x0D, { VK_OEM_MINUS,  SHIFT_PRESS   }, { VK_OEM_PLUS,   SHIFT_KEEP    } },  // =  +  (Shift+-, Shift+;)
    { 0x1A, { VK_OEM_4,      SHIFT_KEEP    }, { VK_OEM_4,      SHIFT_KEEP    } },  // [  {
    { 0x1B, { VK_OEM_6,      SHIFT_KEEP    }, { VK_OEM_6,      SHIFT_KEEP    } },  // ]  }
    { 0x2B, { VK_OEM_5,      SHIFT_KEEP    }, { VK_OEM_5,      SHIFT_KEEP    } },  // \  |  (Yen key)
    { 0x27, { VK_OEM_PLUS,   SHIFT_KEEP    }, { VK_OEM_1,      SHIFT_RELEASE } },  // ;  :  (; key, : key)
    { 0x28, { '7',           SHIFT_PRESS   }, { '2',           SHIFT_KEEP    } },  // '  "  (Shift+7, Shift+2)
    { 0x29, { VK_OEM_3,      SHIFT_PRESS   }, { VK_OEM_7,      SHIFT_KEEP    } },  // `  ~  (Shift+@, Shift+^)
};

// [mode][scan code] -> entry, or nullptr if the key is not remapped.
static const KeyMapEntry* s_index[2][256];

void BuildKeyMapIndex()
{
    for (const KeyMapEntry& e : KEYMAP_US_TO_JIS)
        if (!s_index[LAYOUT_US_TO_JIS][e.scanCode & 0xFF])
            s_index[LAYOUT_US_TO_JIS][e.scanCode & 0xFF] = &e;
    for (const KeyMapEntry& e : KEYMAP_JIS_TO_US)
        if (!s_index[LAYOUT_JIS_TO_US][e.scanCode & 0xFF])
            s_index[LAYOUT_JIS_TO_US][e.scanCode & 0xFF] = &e;
}

const KeyMapEntry* FindKeyMapEntry(WORD scanCode)
{
    const int mode = (g_layoutMode == LAYOUT_US_TO_JIS) ? LAYOUT_US_TO_JIS : LAYOUT_JIS_TO_US;
    return s_index[mode][scanCode & 0xFF];
}

MapResult GetOutput(const KeyMapEntry& entry, bool shift, KeyOutput& out)
{
    out = shift ? entry.shifted : entry.normal;
    if (out.vk == KEY_SUPPRESS) return MAP_SUPPRESS;
    if (out.vk == KEY_PASS)     return MAP_PASS;
    return MAP_REMAP;
}
