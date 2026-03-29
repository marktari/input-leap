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

#include "platform/AtariClipboard.h"
#include "inputleap/IClipboard.h"
#include "base/Log.h"

#include <gem.h>

namespace inputleap {

AtariClipboard::AtariClipboard() :
    m_hasData(false)
{
}

AtariClipboard::~AtariClipboard()
{
}

bool AtariClipboard::get(IClipboard* clipboard, ClipboardID id) const
{
    if (!clipboard) {
        return false;
    }

    // Only support primary clipboard
    if (id != kClipboardClipboard) {
        return false;
    }

    // Open clipboard for reading
    clipboard->open(0);

    // Add text data if available
    if (m_hasData && !m_textData.empty()) {
        clipboard->add(IClipboard::kText, m_textData);
        LOG_DEBUG("retrieved clipboard data: %lu bytes", static_cast<unsigned long>(m_textData.size()));
    }

    clipboard->close();
    return true;
}

bool AtariClipboard::set(const IClipboard* clipboard, ClipboardID id)
{
    if (!clipboard) {
        return false;
    }

    // Only support primary clipboard
    if (id != kClipboardClipboard) {
        return false;
    }

    // Clear current data
    m_textData.clear();
    m_hasData = false;

    // Get text data from clipboard
    if (clipboard->has(IClipboard::kText)) {
        m_textData = clipboard->get(IClipboard::kText);
        m_hasData = true;
        LOG_DEBUG("stored clipboard data: %lu bytes", static_cast<unsigned long>(m_textData.size()));
    }

    return true;
}

} // namespace inputleap
