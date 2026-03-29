#pragma once

#include "arch/unix/ArchNetworkBSD.h"

#define ARCH_NETWORK ArchNetworkBSD

namespace inputleap {

// Atari FreeMiNT uses BSD-compatible sockets, so we can reuse the BSD implementation
using ArchNetworkAtari = ArchNetworkBSD;

} // namespace inputleap
