/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2024 InputLeap contributors
 */

#include "platform/AtariKeyState.h"
#include "inputleap/key_types.h"
#include "base/Log.h"

#include <mint/osbind.h>
#include <map>

extern "C" {
    extern void call_mousevec(unsigned char *data, void (**mousevec)(void *));
    extern void call_ikbdvec(unsigned char code, _IOREC *iorec, void (**ikbdvec)());
    extern int asm_set_ipl(int level);
    extern unsigned long mousexy(void);
}

extern void (**mousevec)(void *);
extern _IOREC *iorec;
extern void (**ikbdvec)();
extern unsigned char **keytbl;


namespace inputleap {

// Input Leap KeyID to Atari scancode mapping table
// Based on VNC to Atari conversion table, using QWERTY layout
static const std::map<KeyID, std::uint8_t> s_keyMap = {
    // Letters - QWERTY to Atari scancodes (both uppercase and lowercase)
    {'a', 0x1E}, {'A', 0x1E}, {'b', 0x30}, {'B', 0x30},
    {'c', 0x2E}, {'C', 0x2E}, {'d', 0x20}, {'D', 0x20},
    {'e', 0x12}, {'E', 0x12}, {'f', 0x21}, {'F', 0x21},
    {'g', 0x22}, {'G', 0x22}, {'h', 0x23}, {'H', 0x23},
    {'i', 0x17}, {'I', 0x17}, {'j', 0x24}, {'J', 0x24},
    {'k', 0x25}, {'K', 0x25}, {'l', 0x26}, {'L', 0x26},
    {'m', 0x32}, {'M', 0x32}, {'n', 0x31}, {'N', 0x31},
    {'o', 0x18}, {'O', 0x18}, {'p', 0x19}, {'P', 0x19},
    {'q', 0x10}, {'Q', 0x10}, {'r', 0x13}, {'R', 0x13},
    {'s', 0x1F}, {'S', 0x1F}, {'t', 0x14}, {'T', 0x14},
    {'u', 0x16}, {'U', 0x16}, {'v', 0x2F}, {'V', 0x2F},
    {'w', 0x11}, {'W', 0x11}, {'x', 0x2D}, {'X', 0x2D},
    {'y', 0x15}, {'Y', 0x15}, {'z', 0x2C}, {'Z', 0x2C},

    // Numbers and shifted number symbols
    {'1', 0x02}, {'!', 0x02},   // 1 and !
    {'2', 0x03}, {'@', 0x03},   // 2 and @
    {'3', 0x04}, {'#', 0x04},   // 3 and #
    {'4', 0x05}, {'$', 0x05},   // 4 and $
    {'5', 0x06}, {'%', 0x06},   // 5 and %
    {'6', 0x07}, {'^', 0x07},   // 6 and ^
    {'7', 0x08}, {'&', 0x08},   // 7 and &
    {'8', 0x09}, {'*', 0x09},   // 8 and *
    {'9', 0x0A}, {'(', 0x0A},   // 9 and (
    {'0', 0x0B}, {')', 0x0B},   // 0 and )

    // Special keys and their shifted variants
    {kKeyReturn, 0x1C},       // ENTER
    {kKeyEscape, 0x01},       // ESC
    {kKeyBackSpace, 0x0E},    // BACK
    {kKeyTab, 0x0F},          // TAB
    {' ', 0x39},              // SPACE
    {'-', 0x0C}, {'_', 0x0C}, // - and _
    {'=', 0x0D}, {'+', 0x0D}, // = and +
    {'[', 0x1A}, {'{', 0x1A}, // [ and {
    {']', 0x1B}, {'}', 0x1B}, // ] and }
    {'\\', 0x2B}, {'|', 0x2B}, // \ and |
    {';', 0x27}, {':', 0x27}, // ; and :
    {'\'', 0x28}, {'"', 0x28}, // ' and "
    {'`', 0x29}, {'~', 0x29}, // ` and ~
    {',', 0x33}, {'<', 0x33}, // , and <
    {'.', 0x34}, {'>', 0x34}, // . and >
    {'/', 0x35}, {'?', 0x35}, // / and ?
    {kKeyCapsLock, 0x3A},     // CAPS

    // Function keys
    {kKeyF1, 0x3B}, {kKeyF2, 0x3C}, {kKeyF3, 0x3D}, {kKeyF4, 0x3E},
    {kKeyF5, 0x3F}, {kKeyF6, 0x40}, {kKeyF7, 0x41}, {kKeyF8, 0x42},
    {kKeyF9, 0x43}, {kKeyF10, 0x44}, {kKeyF11, 0x62}, {kKeyF12, 0x61},

    // Navigation keys
    {kKeyInsert, 0x52},       // INS
    {kKeyHome, 0x47},         // HOME
    {kKeyPageUp, 0x45},       // PgUp
    {kKeyDelete, 0x53},       // DEL
    {kKeyEnd, 0x55},          // END
    {kKeyPageDown, 0x46},     // PgDn
    {kKeyRight, 0x4D},        // ->
    {kKeyLeft, 0x4B},         // <-
    {kKeyDown, 0x50},         // DOWN
    {kKeyUp, 0x48},           // UP
    {kKeyNumLock, 0x54},      // NuLoc

    // Keypad
    {kKeyKP_Divide, 0x65},    // KP/
    {kKeyKP_Multiply, 0x66},  // KP*
    {kKeyKP_Subtract, 0x4A},  // KP-
    {kKeyKP_Add, 0x4E},       // KP+
    {kKeyKP_Enter, 0x72},     // ENT
    {kKeyKP_1, 0x6D}, {kKeyKP_2, 0x6E}, {kKeyKP_3, 0x6F},
    {kKeyKP_4, 0x6A}, {kKeyKP_5, 0x6B}, {kKeyKP_6, 0x6C},
    {kKeyKP_7, 0x67}, {kKeyKP_8, 0x68}, {kKeyKP_9, 0x69},
    {kKeyKP_0, 0x70}, {kKeyKP_Decimal, 0x71},

    // Modifier keys
    {kKeyControl_L, 0x1D},    // LCTRL
    {kKeyShift_L, 0x2A},      // LSHFT
    {kKeyAlt_L, 0x38},        // LALT
    {kKeyControl_R, 0x1D},    // RCTRL (same as left on Atari)
    {kKeyShift_R, 0x36},      // RSHFT
    {kKeyAlt_R, 0x38},        // RALT (same as left on Atari)
};

AtariKeyState::AtariKeyState(IEventQueue* events) : KeyState(events)
{
}

AtariKeyState::~AtariKeyState()
{
}

bool AtariKeyState::fakeCtrlAltDel()
{
    return false;
}

KeyModifierMask AtariKeyState::pollActiveModifiers() const
{
    KeyModifierMask mask = 0;
    const int state = Kbshift(-1);
    if ((state & (K_RSHIFT | K_LSHIFT)) != 0) {
        mask |= KeyModifierShift;
    }
    if ((state & K_CTRL) != 0) {
        mask |= KeyModifierControl;
    }
    if ((state & K_ALT) != 0) {
        mask |= KeyModifierAlt;
    }
    return mask;
}

std::int32_t AtariKeyState::pollActiveGroup() const
{
    return 0;
}

void AtariKeyState::pollPressedKeys(KeyButtonSet& pressedKeys) const
{
    pressedKeys.clear();
}

void AtariKeyState::getKeyMap(inputleap::KeyMap& keyMap)
{
    // Clear and rebuild the keymap
    inputleap::KeyMap empty;
    keyMap.swap(empty);

    // Add entries for all keys in our mapping table
    inputleap::KeyMap::KeyItem item;
    item.m_group = 0;  // Default group
    item.m_required = 0; // No special modifiers required
    item.m_sensitive = 0; // Not sensitive to modifiers by default
    item.m_generates = 0; // Doesn't generate modifiers by default
    item.m_dead = false; // Not a dead key
    item.m_lock = false; // Not a locking key
    item.m_client = 0; // No client data

    for (const auto& mapping : s_keyMap) {
        item.m_id = mapping.first; // KeyID
        item.m_button = static_cast<KeyButton>(mapping.second); // Atari scancode as KeyButton

        // Special handling for modifier keys
        if (mapping.first == kKeyShift_L || mapping.first == kKeyShift_R) {
            item.m_generates = KeyModifierShift;
        } else if (mapping.first == kKeyControl_L || mapping.first == kKeyControl_R) {
            item.m_generates = KeyModifierControl;
        } else if (mapping.first == kKeyAlt_L || mapping.first == kKeyAlt_R) {
            item.m_generates = KeyModifierAlt;
        } else if (mapping.first == kKeyCapsLock) {
            item.m_generates = KeyModifierCapsLock;
            item.m_lock = true;
        } else if (mapping.first == kKeyNumLock) {
            item.m_generates = KeyModifierNumLock;
            item.m_lock = true;
        } else {
            item.m_generates = 0;
            item.m_lock = false;
        }

        keyMap.addKeyEntry(item);
        LOG_DEBUG2("added key mapping: keyID=0x%08x button=0x%02x", item.m_id, item.m_button);
    }
}

void AtariKeyState::fakeKeyDown(KeyID id, KeyModifierMask mask, KeyButton button)
{
	void *ssp = NULL;
	int level;
    LOG_DEBUG1("AtariKeyStatex::fakeKeyDown() id=0x%08x mask=0x%04x button=0x%04x", id, mask, button);

    // Directly translate KeyID to Atari scancode and send
    const std::uint8_t scancode = keyIDToAtariScancode(id);
    if (scancode != 0) {
        // Track the mapping for proper key release
        m_activeKeys[button] = scancode;

        LOG_DEBUG1("sending key down: keyID=0x%08x button=0x%04x → scancode=0x%02x", id, button, scancode);
//        Ikbdws(1, reinterpret_cast<const char*>(&scancode));
	if(!Super(1L))
		ssp = (void *)Super(0L);
        level = asm_set_ipl(7);
            if((iorec != NULL) && (ikbdvec != NULL))
                call_ikbdvec(scancode, iorec, ikbdvec);
                	asm_set_ipl(level);
	if(ssp != NULL)
		Super(ssp);


    } else {
        LOG_DEBUG1("no scancode mapping for keyID=0x%08x", id);
    }
}

bool AtariKeyState::fakeKeyUp(KeyButton button)
{
	void *ssp = NULL;
	int level;
    LOG_DEBUG1("AtariKeyStatex::fakeKeyUp() button=0x%04x", button);

    // Look up the scancode from our tracking map
    auto it = m_activeKeys.find(button);
    if (it != m_activeKeys.end()) {
        const std::uint8_t scancode = it->second;
        const std::uint8_t release_code = scancode | 0x80u;

        LOG_DEBUG1("sending key up: button=0x%04x → scancode=0x%02x (was 0x%02x)", button, release_code, scancode);
//            Ikbdws(1, reinterpret_cast<const char*>(&release_code));
	if(!Super(1L))
		ssp = (void *)Super(0L);
        level = asm_set_ipl(7);
            if((iorec != NULL) && (ikbdvec != NULL))
                call_ikbdvec(release_code, iorec, ikbdvec);
	asm_set_ipl(level);
	if(ssp != NULL)
		Super(ssp);

        // Remove from active keys
        m_activeKeys.erase(it);
        return true;
    }

    LOG_DEBUG1("no active scancode mapping for button=0x%04x", button);
    return false;
}

void AtariKeyState::fakeKey(const Keystroke& keystroke)
{
    if (keystroke.m_type != Keystroke::kButton) {
        return;
    }

    const std::uint8_t scancode = static_cast<std::uint8_t>(keystroke.m_data.m_button.m_button & 0xff);
    std::uint8_t packet = scancode;
    if (!keystroke.m_data.m_button.m_press) {
        packet |= 0x80u;
    }
    LOG_DEBUG2("AtariKeyState::fakeKey() scancode=0x%02x press=%d", packet & 0x7f, keystroke.m_data.m_button.m_press);
    Ikbdws(1, reinterpret_cast<const char*>(&packet));
}

std::uint8_t AtariKeyState::keyIDToAtariScancode(KeyID keyID)
{
    auto it = s_keyMap.find(keyID);
    if (it != s_keyMap.end()) {
        return it->second;
    }

    // For unmapped keys, log and return 0
    LOG_DEBUG2("unmapped key ID: 0x%08x", keyID);
    return 0;
}
} // namespace inputleap
