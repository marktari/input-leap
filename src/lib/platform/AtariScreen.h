/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2024 InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "config.h"
#include "inputleap/PlatformScreen.h"
#include "inputleap/KeyMap.h"

#include <mint/osbind.h>
#include <mint/cookie.h>
#include <mint/ostruct.h>

#include <set>
#include <vector>

namespace inputleap {

class AtariKeyState;
class AtariClipboard;

//! Implementation of IPlatformScreen for Atari FreeMiNT
class AtariScreen : public PlatformScreen {
public:
    AtariScreen(bool isPrimary, IEventQueue* events);
    ~AtariScreen() override;

    //! @name manipulators
    //@{

    //@}

    // IScreen overrides
    const EventTarget* get_event_target() const override;
    bool getClipboard(ClipboardID id, IClipboard*) const override;
    void getShape(std::int32_t& x, std::int32_t& y,
                  std::int32_t& width, std::int32_t& height) const override;
    void getCursorPos(std::int32_t& x, std::int32_t& y) const override;

    // IPrimaryScreen overrides
    void reconfigure(std::uint32_t activeSides) override;
    void warpCursor(std::int32_t x, std::int32_t y) override;
    std::uint32_t registerHotKey(KeyID key, KeyModifierMask mask) override;
    void unregisterHotKey(std::uint32_t id) override;
    void fakeInputBegin() override;
    void fakeInputEnd() override;
    std::int32_t getJumpZoneSize() const override;
    bool isAnyMouseButtonDown(std::uint32_t& buttonID) const override;
    void getCursorCenter(std::int32_t& x, std::int32_t& y) const override;

    // ISecondaryScreen overrides
    void fakeMouseButton(ButtonID id, bool press) override;
    void fakeMouseMove(std::int32_t x, std::int32_t y) override;
    void fakeMouseRelativeMove(std::int32_t dx, std::int32_t dy) const override;
    void fakeMouseWheel(std::int32_t xDelta, std::int32_t yDelta) const override;

    // IPlatformScreen overrides
    void enable() override;
    void disable() override;
    void enter() override;
    bool canLeave() override;
    void leave() override;
    bool setClipboard(ClipboardID, const IClipboard*) override;
    void checkClipboards() override;
    void openScreensaver(bool notify) override;
    void closeScreensaver() override;
    void screensaver(bool activate) override;
    void resetOptions() override;
    void setOptions(const OptionsList& options) override;
    void setSequenceNumber(std::uint32_t) override;
    bool isPrimary() const override;

    void handle_system_event(const Event& event) override;

protected:
    void updateButtons() override;
    IKeyState* getKeyState() const override;

private:
    void init();
    void deinit();
    void updateScreenShape();
    bool checkMiNT();
    bool checkIKBD();

    // IKBD communication
    void sendIKBDCommand(const std::uint8_t* command, std::size_t length);
    void sendMousePacket(std::uint8_t buttons, std::int32_t dx, std::int32_t dy);
    void sendKeycode(std::uint8_t keycode, bool press);

    // Event handling
    void handleKeyEvent(bool down, std::uint8_t scancode);
    void handleMouseEvent();
    void pollEvents();

private:
    bool m_isPrimary;
    IEventQueue* m_events;
    AtariKeyState* m_keyState;
    AtariClipboard* m_clipboard;

    // Screen properties
    std::int32_t m_x, m_y;
    std::int32_t m_width, m_height;

    // Mouse state
    std::int32_t m_mouseX, m_mouseY;
    std::uint8_t m_mouseButtons;

    // System checks
    bool m_hasMiNT;
    bool m_hasIKBD;

    // Input state
    bool m_fakeInput;
    std::uint32_t m_sequenceNumber;
};

} // namespace inputleap
