/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2002 Chris Schoeneman
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

#include "config.h"

#include "arch/unix/ArchNetworkBSD.h"

#include "arch/atari/ArchMultithreadAtari.h"
#include "arch/atari/XArchAtari.h"
#include "arch/Arch.h"
#include "arch/XArch.h"
#include "base/Time.h"

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#if !defined(TCP_NODELAY)
#    include <netinet/tcp.h>
#endif
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <poll.h>

namespace inputleap {

static const int s_family[] = {
    PF_UNSPEC,
    PF_INET,
    PF_INET,  // Map IPv6 to IPv4 for Atari
};
static const int s_type[] = {
    SOCK_DGRAM,
    SOCK_STREAM
};


//
// ArchNetworkBSD (Atari single-threaded implementation)
//

 ArchNetworkBSD::ArchNetworkBSD()
{
}

 ArchNetworkBSD::~ArchNetworkBSD()
{
}

void
 ArchNetworkBSD::init()
{
}

ArchSocket
 ArchNetworkBSD::newSocket(EAddressFamily family, ESocketType type)
{
    // create socket
    int fd = socket(s_family[family], s_type[type], 0);
    if (fd == -1) {
        throwError(errno);
    }
    try {
        setBlockingOnSocket(fd, false);
        // No IPv6 support on Atari
    }
    catch (...) {
        close(fd);
        throw;
    }

    // allocate socket object
    ArchSocketImpl* newSocket = new ArchSocketImpl;
    newSocket->m_fd            = fd;
    newSocket->m_refCount      = 1;
    return newSocket;
}

ArchSocket
 ArchNetworkBSD::copySocket(ArchSocket s)
{
    assert(s != nullptr);

    // ref the socket and return it (no mutex needed for single-threaded)
    ++s->m_refCount;
    return s;
}

void
 ArchNetworkBSD::closeSocket(ArchSocket s)
{
    assert(s != nullptr);

    // unref the socket and note if it should be released (no mutex needed for single-threaded)
    bool doClose = (--s->m_refCount == 0);

    // close the socket if necessary
    if (doClose) {
        if (close(s->m_fd) == -1) {
            // close failed.  restore the last ref and throw.
            int err = errno;
            ++s->m_refCount;
            throwError(err);
        }
        delete s;
    }
}

void
 ArchNetworkBSD::closeSocketForRead(ArchSocket s)
{
    assert(s != nullptr);

    if (shutdown(s->m_fd, 0) == -1) {
        if (errno != ENOTCONN) {
            throwError(errno);
        }
    }
}

void
 ArchNetworkBSD::closeSocketForWrite(ArchSocket s)
{
    assert(s != nullptr);

    if (shutdown(s->m_fd, 1) == -1) {
        if (errno != ENOTCONN) {
            throwError(errno);
        }
    }
}

void
 ArchNetworkBSD::bindSocket(ArchSocket s, ArchNetAddress addr)
{
    assert(s != nullptr);
    assert(addr != nullptr);

    if (bind(s->m_fd, TYPED_ADDR(struct sockaddr, addr), addr->m_len) == -1) {
        throwError(errno);
    }
}

void
 ArchNetworkBSD::listenOnSocket(ArchSocket s)
{
    assert(s != nullptr);

    // hardcoding backlog
    if (listen(s->m_fd, 3) == -1) {
        throwError(errno);
    }
}

ArchSocket
 ArchNetworkBSD::acceptSocket(ArchSocket s, ArchNetAddress* addr)
{
    assert(s != nullptr);

    // if user passed nullptr in addr then use scratch space
    ArchNetAddress dummy;
    if (addr == nullptr) {
        addr = &dummy;
    }

    // create new socket and address
    ArchSocketImpl* newSocket = new ArchSocketImpl;
    *addr                      = new ArchNetAddressImpl;

    // accept on socket
    ACCEPT_TYPE_ARG3 len = static_cast<ACCEPT_TYPE_ARG3>((*addr)->m_len);
    int fd = accept(s->m_fd, TYPED_ADDR(struct sockaddr, (*addr)), &len);
    (*addr)->m_len = static_cast<socklen_t>(len);
    if (fd == -1) {
        int err = errno;
        delete newSocket;
        delete *addr;
        *addr = nullptr;
        if (err == EAGAIN) {
            return nullptr;
        }
        throwError(err);
    }

    try {
        setBlockingOnSocket(fd, false);
    }
    catch (...) {
        close(fd);
        delete newSocket;
        delete *addr;
        *addr = nullptr;
        throw;
    }

    // initialize socket
    newSocket->m_fd       = fd;
    newSocket->m_refCount = 1;

    // discard address if not requested
    if (addr == &dummy) {
        ARCH->closeAddr(dummy);
    }

    return newSocket;
}

bool
 ArchNetworkBSD::connectSocket(ArchSocket s, ArchNetAddress addr)
{
    assert(s != nullptr);
    assert(addr != nullptr);

    if (connect(s->m_fd, TYPED_ADDR(struct sockaddr, addr), addr->m_len) == -1) {
        if (errno == EISCONN) {
            return true;
        }
        if (errno == EINPROGRESS) {
            return false;
        }
        throwError(errno);
    }
    return true;
}


int
 ArchNetworkBSD::pollSocket(PollEntry pe[], int num, double timeout)
{
    assert(pe != nullptr || num == 0);

    // return if nothing to do
    if (num == 0) {
        if (timeout > 0.0) {
            inputleap::this_thread_sleep(timeout);
        }
        return 0;
    }

    // allocate space for translated query
    struct pollfd* pfd = new struct pollfd[num];

    // translate query (no unblock pipe for single-threaded operation)
    for (int i = 0; i < num; ++i) {
        pfd[i].fd     = (pe[i].m_socket == nullptr) ? -1 : pe[i].m_socket->m_fd;
        pfd[i].events = 0;
        if ((pe[i].m_events & kPOLLIN) != 0) {
            pfd[i].events |= POLLIN;
        }
        if ((pe[i].m_events & kPOLLOUT) != 0) {
            pfd[i].events |= POLLOUT;
        }
    }

    // prepare timeout
    int t = (timeout < 0.0) ? -1 : static_cast<int>(1000.0 * timeout);

    // do the poll
    int n = poll(pfd, num, t);

    // handle results
    if (n == -1) {
        if (errno == EINTR) {
            // interrupted system call
            ARCH->testCancelThread();
            delete[] pfd;
            return 0;
        }
        delete[] pfd;
        throwError(errno);
    }

    // translate back
    for (int i = 0; i < num; ++i) {
        pe[i].m_revents = 0;
        if ((pfd[i].revents & POLLIN) != 0) {
            pe[i].m_revents |= kPOLLIN;
        }
        if ((pfd[i].revents & POLLOUT) != 0) {
            pe[i].m_revents |= kPOLLOUT;
        }
        if ((pfd[i].revents & POLLERR) != 0) {
            pe[i].m_revents |= kPOLLERR;
        }
        if ((pfd[i].revents & POLLNVAL) != 0) {
            pe[i].m_revents |= kPOLLNVAL;
        }
    }

    delete[] pfd;
    return n;
}


void
 ArchNetworkBSD::unblockPollSocket(ArchThread thread)
{
    // No-op for single-threaded operation
    (void)thread;
}

size_t
 ArchNetworkBSD::readSocket(ArchSocket s, void* buf, size_t len)
{
    assert(s != nullptr);

    ssize_t n = read(s->m_fd, buf, len);
    if (n == -1) {
        if (errno == EINTR || errno == EAGAIN) {
            return 0;
        }
        throwError(errno);
    }
    return n;
}

size_t
 ArchNetworkBSD::writeSocket(ArchSocket s, const void* buf, size_t len)
{
    assert(s != nullptr);

    ssize_t n = write(s->m_fd, buf, len);
    if (n == -1) {
        if (errno == EINTR || errno == EAGAIN) {
            return 0;
        }
        throwError(errno);
    }
    return n;
}

void
 ArchNetworkBSD::throwErrorOnSocket(ArchSocket s)
{
    assert(s != nullptr);

    // get the error from the socket layer
    int err        = 0;
    socklen_t size = static_cast<socklen_t>(sizeof(err));
    if (getsockopt(s->m_fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<optval_t*>(&err), &size) == -1) {
        err = errno;
    }

    // throw if there's an error
    if (err != 0) {
        throwError(err);
    }
}

void
 ArchNetworkBSD::setBlockingOnSocket(int fd, bool blocking)
{
    assert(fd != -1);

    int mode = fcntl(fd, F_GETFL, 0);
    if (mode == -1) {
        throwError(errno);
    }
    if (blocking) {
        mode &= ~O_NONBLOCK;
    }
    else {
        mode |= O_NONBLOCK;
    }
    if (fcntl(fd, F_SETFL, mode) == -1) {
        throwError(errno);
    }
}

bool
 ArchNetworkBSD::setNoDelayOnSocket(ArchSocket s, bool noDelay)
{
    assert(s != nullptr);

    // get old state
    int oflag;
    socklen_t size = static_cast<socklen_t>(sizeof(oflag));
    if (getsockopt(s->m_fd, IPPROTO_TCP, TCP_NODELAY,
                            reinterpret_cast<optval_t*>(&oflag), &size) == -1) {
        throwError(errno);
    }

    int flag = noDelay ? 1 : 0;
    size = static_cast<socklen_t>(sizeof(flag));
    if (setsockopt(s->m_fd, IPPROTO_TCP, TCP_NODELAY,
                            reinterpret_cast<optval_t*>(&flag), size) == -1) {
        throwError(errno);
    }

    return (oflag != 0);
}

bool
 ArchNetworkBSD::setReuseAddrOnSocket(ArchSocket s, bool reuse)
{
    assert(s != nullptr);

    // get old state
    int oflag;
    socklen_t size = static_cast<socklen_t>(sizeof(oflag));
    if (getsockopt(s->m_fd, SOL_SOCKET, SO_REUSEADDR,
                            reinterpret_cast<optval_t*>(&oflag), &size) == -1) {
        throwError(errno);
    }

    int flag = reuse ? 1 : 0;
    size = static_cast<socklen_t>(sizeof(flag));
    if (setsockopt(s->m_fd, SOL_SOCKET, SO_REUSEADDR,
                            reinterpret_cast<optval_t*>(&flag), size) == -1) {
        throwError(errno);
    }

    return (oflag != 0);
}

std::string
 ArchNetworkBSD::getHostName()
{
    char name[256];
    if (gethostname(name, sizeof(name)) == -1) {
        name[0] = '\0';
    }
    else {
        name[sizeof(name) - 1] = '\0';
    }
    return name;
}

ArchNetAddress
 ArchNetworkBSD::newAnyAddr(EAddressFamily family)
{
    // allocate address
    ArchNetAddressImpl* addr = new ArchNetAddressImpl;

    // fill it in
    switch (family) {
    case kINET:
    case kINET6: {  // Treat IPv6 as IPv4 on Atari
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in, addr);
        memset(ipAddr, 0, sizeof(struct sockaddr_in));
        ipAddr->sin_family         = AF_INET;
        ipAddr->sin_addr.s_addr    = INADDR_ANY;
        addr->m_len = static_cast<socklen_t>(sizeof(struct sockaddr_in));
        break;
    }
    default:
        delete addr;
        assert(0 && "invalid family");
    }

    return addr;
}

ArchNetAddress
 ArchNetworkBSD::copyAddr(ArchNetAddress addr)
{
    assert(addr != nullptr);

    // allocate and copy address
    return new ArchNetAddressImpl(*addr);
}

ArchNetAddress
 ArchNetworkBSD::nameToAddr(const std::string& name)
{
    // allocate address
    ArchNetAddressImpl* addr = new ArchNetAddressImpl;

    struct addrinfo hints;
    struct addrinfo *p;
    int ret;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;

    // No mutex needed for single-threaded operation
    if ((ret = getaddrinfo(name.c_str(), nullptr, &hints, &p)) != 0) {
        delete addr;
        throwNameError(ret);
    }

    // Only IPv4 supported on Atari
    if (p->ai_family == AF_INET) {
        addr->m_len = static_cast<socklen_t>(sizeof(struct sockaddr_in));
    } else {
        // Skip non-IPv4 addresses
        freeaddrinfo(p);
        delete addr;
        throw XArchNetworkNameNoAddress("IPv6 not supported on Atari");
    }

    memcpy(&addr->m_addr, p->ai_addr, addr->m_len);
    freeaddrinfo(p);

    return addr;
}

void
 ArchNetworkBSD::closeAddr(ArchNetAddress addr)
{
    assert(addr != nullptr);

    delete addr;
}

std::string
 ArchNetworkBSD::addrToName(ArchNetAddress addr)
{
    assert(addr != nullptr);

    // No mutex needed for single-threaded operation
    char host[1024];
    char service[20];
    int ret = getnameinfo(TYPED_ADDR(struct sockaddr, addr), addr->m_len, host,
            sizeof(host), service, sizeof(service), 0);
    if (ret != 0) {
        throwNameError(ret);
    }

    return host;
}

std::string
 ArchNetworkBSD::addrToString(ArchNetAddress addr)
{
    assert(addr != nullptr);

    switch (getAddrFamily(addr)) {
    case kINET:
    case kINET6: {  // Treat IPv6 as IPv4 on Atari
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in, addr);

        // No mutex needed for single-threaded operation
        return inet_ntoa(ipAddr->sin_addr);
    }

    default:
        assert(0 && "unknown address family");
        return "";
    }
}

IArchNetwork::EAddressFamily
 ArchNetworkBSD::getAddrFamily(ArchNetAddress addr)
{
    assert(addr != nullptr);

    switch (addr->m_addr.ss_family) {
    case AF_INET:
        return kINET;
    // No IPv6 support on Atari

    default:
        return kUNKNOWN;
    }
}

void
 ArchNetworkBSD::setAddrPort(ArchNetAddress addr, int port)
{
    assert(addr != nullptr);

    switch (getAddrFamily(addr)) {
    case kINET:
    case kINET6: {  // Treat IPv6 as IPv4 on Atari
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in, addr);
        ipAddr->sin_port = htons(port);
        break;
    }

    default:
        assert(0 && "unknown address family");
        break;
    }
}

int
 ArchNetworkBSD::getAddrPort(ArchNetAddress addr)
{
    assert(addr != nullptr);

    switch (getAddrFamily(addr)) {
    case kINET:
    case kINET6: {  // Treat IPv6 as IPv4 on Atari
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in, addr);
        return ntohs(ipAddr->sin_port);
    }

    default:
        assert(0 && "unknown address family");
        return 0;
    }
}

bool
 ArchNetworkBSD::isAnyAddr(ArchNetAddress addr)
{
    assert(addr != nullptr);

    switch (getAddrFamily(addr)) {
    case kINET:
    case kINET6: {  // Treat IPv6 as IPv4 on Atari
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in, addr);
        return (ipAddr->sin_addr.s_addr == INADDR_ANY &&
                addr->m_len == static_cast<socklen_t>(sizeof(struct sockaddr_in)));
    }

    default:
        assert(0 && "unknown address family");
        return true;
    }
}

bool
 ArchNetworkBSD::isEqualAddr(ArchNetAddress a, ArchNetAddress b)
{
    return (a->m_len == b->m_len &&
            memcmp(&a->m_addr, &b->m_addr, a->m_len) == 0);
}

const int*
 ArchNetworkBSD::getUnblockPipe()
{
    // No unblock pipe needed for single-threaded operation
    return nullptr;
}

const int*
 ArchNetworkBSD::getUnblockPipeForThread(ArchThread thread)
{
    // No unblock pipe needed for single-threaded operation
    (void)thread;
    return nullptr;
}

void
 ArchNetworkBSD::throwError(int err)
{
    switch (err) {
    case EINTR:
        ARCH->testCancelThread();
        throw XArchNetworkInterrupted(error_code_to_string_errno(err));

    case EACCES:
    case EPERM:
        throw XArchNetworkAccess(error_code_to_string_errno(err));

    case ENFILE:
    case EMFILE:
    case ENODEV:
    case ENOBUFS:
    case ENOMEM:
    case ENETDOWN:
#if defined(ENOSR)
    case ENOSR:
#endif
        throw XArchNetworkResource(error_code_to_string_errno(err));

    case EPROTOTYPE:
    case EPROTONOSUPPORT:
    case EAFNOSUPPORT:
    case EPFNOSUPPORT:
    case ESOCKTNOSUPPORT:
    case EINVAL:
    case ENOPROTOOPT:
    case EOPNOTSUPP:
    case ESHUTDOWN:
#if defined(ENOPKG)
    case ENOPKG:
#endif
        throw XArchNetworkSupport(error_code_to_string_errno(err));

    case EIO:
        throw XArchNetworkIO(error_code_to_string_errno(err));

    case EADDRNOTAVAIL:
        throw XArchNetworkNoAddress(error_code_to_string_errno(err));

    case EADDRINUSE:
        throw XArchNetworkAddressInUse(error_code_to_string_errno(err));

    case EHOSTUNREACH:
    case ENETUNREACH:
        throw XArchNetworkNoRoute(error_code_to_string_errno(err));

    case ENOTCONN:
        throw XArchNetworkNotConnected(error_code_to_string_errno(err));

    case EPIPE:
        throw XArchNetworkShutdown(error_code_to_string_errno(err));

    case ECONNABORTED:
    case ECONNRESET:
        throw XArchNetworkDisconnected(error_code_to_string_errno(err));

    case ECONNREFUSED:
        throw XArchNetworkConnectionRefused(error_code_to_string_errno(err));

    case EHOSTDOWN:
    case ETIMEDOUT:
        throw XArchNetworkTimedOut(error_code_to_string_errno(err));

    default:
        throw XArchNetwork(error_code_to_string_errno(err));
    }
}

void
 ArchNetworkBSD::throwNameError(int err)
{
    static const char* s_msg[] = {
        "The specified host is unknown",
        "The requested name is valid but does not have an IP address",
        "A non-recoverable name server error occurred",
        "A temporary error occurred on an authoritative name server",
        "An unknown name server error occurred"
    };

    switch (err) {
    case HOST_NOT_FOUND:
        throw XArchNetworkNameUnknown(s_msg[0]);

    case NO_DATA:
        throw XArchNetworkNameNoAddress(s_msg[1]);

    case NO_RECOVERY:
        throw XArchNetworkNameFailure(s_msg[2]);

    case TRY_AGAIN:
        throw XArchNetworkNameUnavailable(s_msg[3]);

    default:
        throw XArchNetworkName(s_msg[4]);
    }
}

} // namespace inputleap