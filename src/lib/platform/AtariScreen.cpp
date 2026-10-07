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

#include "platform/AtariScreen.h"
#include "platform/AtariKeyState.h"
#include "platform/AtariClipboard.h"
#include "inputleap/Clipboard.h"
#include "base/Log.h"
#include "base/IEventQueue.h"
#include "inputleap/ClipboardChunk.h"
#include "inputleap/IPlatformScreen.h"

#include <mint/osbind.h>
#include <mint/cookie.h>
#include <mint/ostruct.h>
#include <gem.h>

#include <algorithm>
#include <cstdio>

extern "C" {
    extern void call_mousevec(unsigned char *data, void (**mousevec)(void *));
    extern void call_ikbdvec(unsigned char code, _IOREC *iorec, void (**ikbdvec)());
    extern int asm_set_ipl(int level);
    extern unsigned long mousexy(void);
}

void (**mousevec)(void *) = NULL;
_IOREC *iorec = NULL;
void (**ikbdvec)() = NULL;
unsigned char **keytbl;
#ifdef COLDFIRE
void *linea000 = NULL;
#endif


namespace inputleap {

AtariScreen::AtariScreen(bool isPrimary, IEventQueue* events) :
    PlatformScreen(),
    m_isPrimary(isPrimary),
    m_events(events),
    m_keyState(nullptr),
    m_clipboard(nullptr),
    m_x(0), m_y(0),
    m_width(640), m_height(480),
    m_mouseX(0), m_mouseY(0),
    m_mouseButtons(0),
    m_hasMiNT(false),
    m_hasIKBD(false),
    m_fakeInput(true),
    m_sequenceNumber(0)
{
    LOG_DEBUG("creating Atari screen");

    // Check for FreeMiNT and IKBD
    m_hasMiNT = checkMiNT();
    m_hasIKBD = true;
    // fixme do an actual check
    //m_hasIKBD = checkIKBD();

    if (!m_hasMiNT) {
        LOG_WARN("FreeMiNT not detected - some features may not work");
    }

    if (!m_hasIKBD) {
        LOG_WARN("IKBD not detected - keyboard/mouse input may not work");
    }

    // Initialize key state and clipboard
    m_keyState = new AtariKeyState(events);
    m_clipboard = new AtariClipboard();

    // Get screen dimensions
    updateScreenShape();

    // Initialize GEM if available
    init();
}

AtariScreen::~AtariScreen()
{
    deinit();
    delete m_keyState;
    delete m_clipboard;
}

void AtariScreen::init()
{
    // Initialize GEM application
    if (appl_init() >= 0) {
        LOG_DEBUG("GEM application initialized");
    } else {
        LOG_WARN("Failed to initialize GEM application");
    }

    _KBDVECS *kbdvecs = (_KBDVECS *)Kbdvbase();
	void **kbdvecs2 = (void **)kbdvecs;
	mousevec = &kbdvecs->mousevec;
	ikbdvec = (void (**)())&kbdvecs2[-1]; /* undocumented */
	iorec = (_IOREC *)Iorec(1);
	keytbl = (unsigned char **)Keytbl(-1, -1, -1);
#ifdef COLDFIRE
	{
		long val;
		if (Getcookie('A000', &val) == C_FOUND && val)
			linea000 = (void *)val;
	}
#endif
}

void AtariScreen::deinit()
{
    // Clean up GEM
    appl_exit();
}

bool AtariScreen::checkMiNT()
{
    long cookie_value;
    if (Getcookie(C_MiNT, &cookie_value) == C_FOUND) {
        LOG_INFO("FreeMiNT detected, version: %ld", cookie_value);
        return true;
    }
    return false;
}

bool AtariScreen::checkIKBD()
{
    return true;
}

void AtariScreen::updateScreenShape()
{
    short work_in[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2};
    short work_out[57] = {0};
    short width = 0;
    short height = 0;
    short unused1 = 0;
    short unused2 = 0;
    short vdi_handle = graf_handle(&width, &height, &unused1, &unused2);

    if (vdi_handle > 0) {
        v_opnvwk(work_in, &vdi_handle, work_out);
        if (vdi_handle > 0) {
            m_width = work_out[0] + 1;
            m_height = work_out[1] + 1;
            v_clsvwk(vdi_handle);
        }
    }

    if (m_width <= 0 || m_height <= 0) {
        m_width = 640;
        m_height = 480;
    }

    LOG_INFO("Screen resolution: %d x %d", static_cast<int>(m_width), static_cast<int>(m_height));
}

void AtariScreen::sendIKBDCommand(const std::uint8_t* command, std::size_t length)
{
    LOG_INFO("sendikbd %d %ld", *command, length);
    if (!m_hasIKBD || !command || length == 0) {
        return;
    }

    // Send command through IKBD
    for (std::size_t i = 0; i < length; i++) {
        Ikbdws(1, (char*)&command[i]);
    }
}

void AtariScreen::sendMousePacket(std::uint8_t buttons, std::int32_t dx, std::int32_t dy)
{
	static std::uint8_t old_buttons = 0, old_x, old_y;
    int delta_x, delta_y;
    void *ssp = NULL;
    int level, i = 0;

    if (!m_fakeInput) {
        return;
    }

    if(!Super(1L))
        ssp = (void *)Super(0L);

    // first move the mouse
    if (dx>=0 && dy>=0)
    {
    do
    {
        int old_x = (int)(mousexy() >> 16);
        int old_y = (int)(mousexy() & 0xffff);
        delta_x = dx - old_x;
        delta_y = dy - old_y;
        if(delta_x < -128)
            delta_x = -128;
        else if(delta_x > 127)
            delta_x = 127;
        if(delta_y < -128)
            delta_y = -128;
        else if(delta_y > 127)
            delta_y = 127;
        if(delta_x || delta_y)
        {
            static unsigned char frame[4];
            level = asm_set_ipl(7); /* mask interrupts */
            /* IKBD: B1: left, B0: right */
            frame[0] = ((old_buttons & 1) << 1) + ((old_buttons & 4) >> 2) + 0xF8;
            frame[1] = (unsigned char)delta_x;
            frame[2] = (unsigned char)delta_y;

            if(mousevec != NULL)
                call_mousevec(frame, mousevec);
            asm_set_ipl(level);
        }
        else
            break;
        i++;
    }
    while(i < 32);
    }

    if((buttons ^ old_buttons) & 7)
    {
        static unsigned char frame[4];
        level = asm_set_ipl(7); /* mask interrupts */
        /* IKBD: B1: left, B0: right */
        frame[0] = ((buttons & 1) << 1) + ((buttons & 4) >> 2) + 0xF8;
        frame[1] = frame[2] = 0;
        if(mousevec != NULL)
            call_mousevec(frame, mousevec);
        asm_set_ipl(level);
    }
    //if((buttonMask ^ old_buttons) & 2) /* 3rd button: middle */
   // {
    //    if(buttonMask & 2)
     //   {
     //       level = asm_set_ipl(7); /* mask interrupts */
     //       vnc_kbd_send_code(0x72); /* ENTER */
     //       vnc_kbd_send_code(0xF2);
     //       asm_set_ipl(level);
      //  }
    //}
    old_buttons = buttons;
    if(ssp != NULL)
        Super(ssp);






/*
            // IKBD mouse packet format:
            // Byte 0: %11111ABC where A=right button, B=left button, C=middle button (inverted)
            std::uint8_t packet[4];
            packet[0] = 0xF8 | ((buttons & 0x01) ? 0x02 : 0x00) |  // Left button
                            ((buttons & 0x02) ? 0x01 : 0x00) |  // Right button
                            ((buttons & 0x04) ? 0x00 : 0x04);   // Middle button (inverted)
            packet[1] = dx;
            packet[2] = dy;
*/


    //sendIKBDCommand(packet, 3);
}

void AtariScreen::sendKeycode(std::uint8_t keycode, bool press)
{
    if (!m_fakeInput) {
        return;
    }

    // IKBD key packet: 0x00-0x7F for key press, add 0x80 for key release
    std::uint8_t packet = press ? keycode : (keycode | 0x80);
    sendIKBDCommand(&packet, 1);
}

// IScreen overrides
const EventTarget* AtariScreen::get_event_target() const
{
    return this;
}

bool AtariScreen::getClipboard(ClipboardID id, IClipboard* clipboard) const
{
    if (m_clipboard && clipboard) {
        return m_clipboard->get(clipboard, id);
    }
    return false;
}

void AtariScreen::getShape(std::int32_t& x, std::int32_t& y,
                           std::int32_t& width, std::int32_t& height) const
{
    x = m_x;
    y = m_y;
    width = m_width;
    height = m_height;
}

void AtariScreen::getCursorPos(std::int32_t& x, std::int32_t& y) const
{
    x = m_mouseX;
    y = m_mouseY;
}

// IPrimaryScreen overrides
void AtariScreen::reconfigure(std::uint32_t activeSides)
{
    // Nothing special to do for Atari
    (void)activeSides;
}

void AtariScreen::warpCursor(std::int32_t x, std::int32_t y)
{
    // Move mouse cursor to specified position
    graf_mouse(M_ON, nullptr);
    // Note: GEM doesn't have direct cursor positioning,
    // so we'll track logical position
    m_mouseX = x;
    m_mouseY = y;
}

std::uint32_t AtariScreen::registerHotKey(KeyID key, KeyModifierMask mask)
{
    // Hot keys not implemented for Atari
    (void)key;
    (void)mask;
    return 0;
}

void AtariScreen::unregisterHotKey(std::uint32_t id)
{
    // Hot keys not implemented
    (void)id;
}

void AtariScreen::fakeInputBegin()
{
    m_fakeInput = true;
}

void AtariScreen::fakeInputEnd()
{
    m_fakeInput = false;
}

std::int32_t AtariScreen::getJumpZoneSize() const
{
    return 1;
}

bool AtariScreen::isAnyMouseButtonDown(std::uint32_t& buttonID) const
{
    if ((m_mouseButtons & 0x01u) != 0) {
        buttonID = static_cast<std::uint32_t>(kButtonLeft);
        return true;
    }
    if ((m_mouseButtons & 0x02u) != 0) {
        buttonID = static_cast<std::uint32_t>(kButtonRight);
        return true;
    }
    if ((m_mouseButtons & 0x04u) != 0) {
        buttonID = static_cast<std::uint32_t>(kButtonMiddle);
        return true;
    }

    buttonID = 0;
    return false;
}

void AtariScreen::getCursorCenter(std::int32_t& x, std::int32_t& y) const
{
    x = m_width / 2;
    y = m_height / 2;
}

// ISecondaryScreen overrides
void AtariScreen::fakeMouseButton(ButtonID id, bool press)
{
    if (!m_fakeInput) {
        return;
    }

    std::uint8_t buttonMask = 0;
    switch (id) {
        case kButtonLeft:   buttonMask = 0x01; break;
        case kButtonRight:  buttonMask = 0x02; break;
        case kButtonMiddle: buttonMask = 0x04; break;
        default: return;
    }

    if (press) {
        m_mouseButtons |= buttonMask;
    } else {
        m_mouseButtons &= ~buttonMask;
    }

    sendMousePacket(m_mouseButtons, -1, -1);
}

void AtariScreen::fakeMouseMove(std::int32_t x, std::int32_t y)
{
    int delta_x, delta_y;
    void *ssp = NULL;

    if (!m_fakeInput) {
      return;
    }

    m_mouseX = x;
    m_mouseY = y;


    AtariScreen* self = const_cast<AtariScreen*>(this);

    self->sendMousePacket(self->m_mouseButtons, x, y);
}

void AtariScreen::fakeMouseRelativeMove(std::int32_t dx, std::int32_t dy) const
{
    if (!m_fakeInput) {
        return;
    }

    AtariScreen* self = const_cast<AtariScreen*>(this);

    // Clamp delta values to signed 8-bit range
    std::int8_t deltaX = static_cast<std::int8_t>(std::max(-127, std::min(127, static_cast<int>(dx))));
    std::int8_t deltaY = static_cast<std::int8_t>(std::max(-127, std::min(127, static_cast<int>(dy))));

    self->m_mouseX += deltaX;
    self->m_mouseY += deltaY;

    self->sendMousePacket(self->m_mouseButtons, deltaX, deltaY);
}

void AtariScreen::fakeMouseWheel(std::int32_t xDelta, std::int32_t yDelta) const
{
    // Mouse wheel not supported on Atari IKBD
    (void)xDelta;
    (void)yDelta;
}

// IPlatformScreen overrides
void AtariScreen::enable()
{
    // Enable input monitoring
}

void AtariScreen::disable()
{
    // Disable input monitoring
}

void AtariScreen::enter()
{
    // Screen entered
}

bool AtariScreen::canLeave()
{
    return true;
}

void AtariScreen::leave()
{
    // Screen left
}

bool AtariScreen::setClipboard(ClipboardID id, const IClipboard* clipboard)
{
    if (m_clipboard && clipboard) {
        return m_clipboard->set(clipboard, id);
    }
    return false;
}

void AtariScreen::checkClipboards()
{
    if (m_clipboard) {
        // Check if clipboard has changed and post events
    }
}

void AtariScreen::openScreensaver(bool notify)
{
    // Screen saver not implemented
    (void)notify;
}

void AtariScreen::closeScreensaver()
{
    // Screen saver not implemented
}

void AtariScreen::screensaver(bool activate)
{
    // Screen saver not implemented
    (void)activate;
}

void AtariScreen::resetOptions()
{
    // Reset all options to defaults
}

void AtariScreen::setOptions(const OptionsList& options)
{
    // Set options
    (void)options;
}

void AtariScreen::setSequenceNumber(std::uint32_t seqNum)
{
    m_sequenceNumber = seqNum;
}

bool AtariScreen::isPrimary() const
{
    return m_isPrimary;
}

void AtariScreen::handle_system_event(const Event& event)
{
    // Handle system events
    (void)event;
}

void AtariScreen::updateButtons()
{
    // Update button mapping - Atari has 2-3 mouse buttons typically
}

IKeyState* AtariScreen::getKeyState() const
{
    return m_keyState;
}

} // namespace inputleap
