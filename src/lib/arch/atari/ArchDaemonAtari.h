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

#include "arch/IArchDaemon.h"
#include "arch/XArch.h"

#define ARCH_DAEMON ArchDaemonAtari

namespace inputleap {

class ArchDaemonAtari : public IArchDaemon {
public:
    void installDaemon(const char*, const char*, const char*, const char*, const char*) override {
        throw XArchDaemon("daemon installation not supported on Atari");
    }

    void uninstallDaemon(const char*) override {
        throw XArchDaemon("daemon installation not supported on Atari");
    }

    void installDaemon() override {
        throw XArchDaemon("daemon installation not supported on Atari");
    }

    void uninstallDaemon() override {
        throw XArchDaemon("daemon installation not supported on Atari");
    }

    int daemonize(const char* name, DaemonFunc func) override {
        (void)name;
        // Just call the function directly - no daemonization on Atari
        return func(0, nullptr);
    }

    bool canInstallDaemon(const char*) override {
        return false;
    }

    bool isDaemonInstalled(const char*) override {
        return false;
    }

    std::string commandLine() const override {
        return "";
    }
};

} // namespace inputleap
