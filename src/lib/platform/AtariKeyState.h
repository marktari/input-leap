/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2024 InputLeap contributors
 */

#pragma once

#include "inputleap/KeyState.h"
#include "inputleap/key_types.h"

namespace inputleap {

class AtariKeyState : public KeyState {
public:
    explicit AtariKeyState(IEventQueue* events);
    ~AtariKeyState() override;

    bool fakeCtrlAltDel() override;
    KeyModifierMask pollActiveModifiers() const override;
    std::int32_t pollActiveGroup() const override;
    void pollPressedKeys(KeyButtonSet& pressedKeys) const override;

    // Override KeyState methods for direct Atari scancode translation
    void fakeKeyDown(KeyID id, KeyModifierMask mask, KeyButton button) override;
    bool fakeKeyUp(KeyButton button) override;

protected:
    void getKeyMap(inputleap::KeyMap& keyMap) override;
    void fakeKey(const Keystroke& keystroke) override;

private:
    std::uint8_t keyIDToAtariScancode(KeyID keyID);

    // Track active keys: KeyButton -> Atari scancode mapping
    std::map<KeyButton, std::uint8_t> m_activeKeys;
};

} // namespace inputleap
