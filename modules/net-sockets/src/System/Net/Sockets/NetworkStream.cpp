// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Sockets/NetworkStream.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IO/IOException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Net/Sockets/SocketException.hpp"
#include "System/Net/Sockets/detail/ErrnoTranslation.hpp"
#include <exception>

#if defined(_WIN32)
#  include <winsock2.h>
#  if defined(_MSC_VER)
#    pragma comment(lib, "ws2_32.lib")
#  endif
namespace {
    using SockFd = SOCKET;
    // Winsock's recv/send/recvfrom take an int transfer length where POSIX takes size_t.
    // Every call site here passes either an intcs count or a fixed 65536 buffer size, so the
    // conversion is exact; naming the platform's own type keeps it from being an implicit
    // 64-to-32 narrowing (MSVC C4267).
    using SockLen = int;
    inline SockFd toSk(int fd)    { return static_cast<SockFd>(fd); }
    inline bool   validFd(int fd) { return toSk(fd) != INVALID_SOCKET; }
    inline void   closeSk(int fd) { ::closesocket(toSk(fd)); }
    inline int lastErrorCode() { return WSAGetLastError(); }
    inline std::string netErr() {
        char buf[32]; snprintf(buf, sizeof(buf), "WSA error %d", WSAGetLastError()); return buf;
    }
    // Winsock error codes already share SocketError's own numbering (see SocketError.hpp's
    // doc comment) -- no translation needed here.
    inline System::Net::Sockets::SocketError toSocketError(int code) {
        return static_cast<System::Net::Sockets::SocketError>(code);
    }
    // See the POSIX branch below for the full rationale. Winsock has no readable non-blocking
    // state (FIONBIO is write-only; real .NET tracks Socket.Blocking in managed state), so this
    // branch applies three of the four rules rather than four.
    inline void validateSocketDescriptor(int fd) {
        if (!validFd(fd))
            throw System::ArgumentException("The descriptor is not a valid socket handle.", "fd");
        int type = 0;
        int len  = static_cast<int>(sizeof(type));
        if (::getsockopt(toSk(fd), SOL_SOCKET, SO_TYPE,
                         reinterpret_cast<char*>(&type), &len) != 0) {
            const int code = WSAGetLastError();
            if (code == WSAENOTSOCK)
                throw System::ArgumentException("The descriptor is not a socket.", "fd");
            throw System::ArgumentException("The descriptor is not an open handle.", "fd");
        }
        struct sockaddr_storage peer{};
        int plen = static_cast<int>(sizeof(peer));
        if (::getpeername(toSk(fd), reinterpret_cast<struct sockaddr*>(&peer), &plen) != 0)
            throw System::IO::IOException("The operation is not allowed on non-connected sockets.");
        if (type != SOCK_STREAM)
            throw System::IO::IOException("The operation is not allowed on non-stream oriented sockets.");
    }
}
#elif defined(__EMSCRIPTEN__)
#  include "System/PlatformNotSupportedException.hpp"
namespace {
    inline bool validFd([[maybe_unused]] int fd) { return false; }
    inline void closeSk([[maybe_unused]] int fd) {}
    // No check here, deliberately. Every operation on a NetworkStream throws
    // PlatformNotSupportedException on Emscripten, so there is no state a check could protect --
    // and validFd() is unconditionally false on this platform, so a shared check would reject
    // every construction including the ones that only ever go on to throw the platform message.
    inline void validateSocketDescriptor([[maybe_unused]] int fd) {}
}
#else
#  include <sys/socket.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <cerrno>
#  include <cstring>
namespace {
    using SockFd = int;
    // POSIX recv/send/recvfrom take a size_t transfer length; see the Windows branch above.
    using SockLen = std::size_t;
    inline SockFd toSk(int fd)    { return fd; }
    inline bool   validFd(int fd) { return fd >= 0; }
    inline void   closeSk(int fd) { ::close(fd); }
    inline int lastErrorCode()    { return errno; }
    inline std::string netErr()   { return std::strerror(errno); }
    // Verified against SocketErrorPal.Unix.cs's GetSocketErrorForNativeError: real .NET
    // translates POSIX errno into the WSA-numbered SocketError space before constructing a
    // SocketException. This port previously cast the raw POSIX errno straight into
    // SocketException's "errorCode" parameter unchanged, so SocketErrorCode never matched any
    // real SocketError value.
    inline System::Net::Sockets::SocketError toSocketError(int code) {
        return SharpRuntimeDetail::Net::Sockets::TranslateErrno(code);
    }

    // SR-AUD-265 / #2136. Real .NET's NetworkStream constructor takes a Socket and refuses four
    // states: a null socket, a non-blocking one, an unconnected one, and one whose SocketType is
    // not Stream. This port's constructor takes a raw descriptor, so it must additionally
    // establish what .NET gets for free -- that the handle is a socket at all -- and that check
    // comes first because every other question is meaningless without it.
    //
    // Each rule is answered by a query that only READS state; nothing here closes, shuts down or
    // otherwise disturbs the descriptor, because on rejection the caller still owns it. Measured
    // answers for every shape (build-probe/2136_probe1_fdstate.cpp, log 2136_probe1_before.log):
    //
    //   AF_UNIX SOCK_STREAM socketpair   SO_TYPE=SOCK_STREAM, getpeername ok, blocking -> ACCEPT
    //   connected loopback TCP           SO_TYPE=SOCK_STREAM, getpeername ok, blocking -> ACCEPT
    //   socketpair after shutdown(RDWR)  unchanged on all three                        -> ACCEPT
    //   SOCK_DGRAM socketpair            SO_TYPE=SOCK_DGRAM                            -> non-stream
    //   unconnected/listening socket     getpeername ENOTCONN                          -> non-connected
    //   O_NONBLOCK socketpair end        F_GETFL & O_NONBLOCK                          -> non-blocking
    //   pipe end, /dev/null              SO_TYPE ENOTSOCK                              -> not a socket
    //   fd < 0, closed fd, fd = 999999   SO_TYPE EBADF                                 -> not an open handle
    //
    // The exception TYPES are this port's choice and are recorded as such: /rv is absent, so the
    // per-file audit report's list of what .NET rejects is the evidence, not its exact exception
    // shapes. "Not a socket handle" is an argument-domain error about the parameter and carries
    // paramName "fd"; the three socket-state rules carry .NET's own IOException messages.
    inline void validateSocketDescriptor(int fd) {
        if (fd < 0)
            throw System::ArgumentException("The descriptor is not a valid socket handle.", "fd");

        int       type = 0;
        socklen_t len  = sizeof(type);
        if (::getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &len) != 0) {
            const int code = errno;
            if (code == ENOTSOCK)
                throw System::ArgumentException("The descriptor is not a socket.", "fd");
            throw System::ArgumentException("The descriptor is not an open handle.", "fd");
        }

        // .NET's own order for the three socket-state rules: blocking, connected, stream.
        const int flags = ::fcntl(fd, F_GETFL);
        if (flags >= 0 && (flags & O_NONBLOCK) != 0)
            throw System::IO::IOException("The operation is not allowed on a non-blocking Socket.");

        struct sockaddr_storage peer{};
        socklen_t               plen = sizeof(peer);
        if (::getpeername(fd, reinterpret_cast<struct sockaddr*>(&peer), &plen) != 0)
            throw System::IO::IOException("The operation is not allowed on non-connected sockets.");

        if (type != SOCK_STREAM)
            throw System::IO::IOException("The operation is not allowed on non-stream oriented sockets.");
    }
}
#endif

namespace System::Net::Sockets {

// The validation runs in the constructor BODY, and that placement is load-bearing rather than
// stylistic: when a constructor body throws, the object's own destructor never runs, so ~NetworkStream
// -> Close() cannot close a descriptor this instance was never allowed to own. That is what makes
// "/proc/self/fd is unchanged across every rejection" true by construction rather than by care.
NetworkStream::NetworkStream(int fd) : fd_(fd) { validateSocketDescriptor(fd); }

NetworkStream::~NetworkStream() { Close(); }

void NetworkStream::ThrowIfClosed() const {
    // Close() records the closed state and the capability properties have always reported it;
    // until #2136 no data-transfer member consulted it, so a closed stream answered Read with 0
    // (a clean end-of-stream, as far as any caller could tell) and Write with success.
    // "Cannot access a closed Stream." is this repository's established message for the state --
    // MemoryStream.cpp:65, UnmanagedMemoryStream.cpp:46, BufferedStream.cpp:30 -- and is
    // repository-contained evidence rather than a guess at .NET's exact text.
    if (!validFd(fd_))
        throw System::ObjectDisposedException("NetworkStream", "Cannot access a closed Stream.");
}

intcs NetworkStream::Read(bytecs buffer[], intcs offset, intcs count) {
    // Unlike this codebase's other Stream implementations (e.g. FileStream::Read), this method
    // previously had no argument validation at all -- a negative `count` wrapped to a huge
    // size_t once cast for the recv() call below, letting the kernel write past the end of a
    // small destination buffer (confirmed via an ASan repro: stack-buffer-overflow). Matches
    // real .NET's Stream.ValidateBufferArguments behavior.
    if (offset < 0)
        throw System::ArgumentOutOfRangeException("offset", "Non-negative number required.");
    if (count < 0)
        throw System::ArgumentOutOfRangeException("count", "Non-negative number required.");
#if defined(__EMSCRIPTEN__)
    (void)buffer; (void)offset; (void)count;
    throw System::PlatformNotSupportedException("NetworkStream is not supported on Emscripten.");
#else
    // The argument checks stay in FRONT of this one: a caller who passes a negative offset to a
    // closed stream has two defects and should hear about the argument, which is the one under
    // their control. The closed check then precedes the count == 0 shortcut, so a zero-length
    // read on a closed stream throws rather than quietly reporting success.
    ThrowIfClosed();
    if (count == 0) return 0;
    auto n = ::recv(toSk(fd_), reinterpret_cast<char*>(buffer + offset),
                    static_cast<SockLen>(count), 0);
    if (n < 0) {
        auto code = lastErrorCode();
        auto err = netErr();
        auto inner = std::make_exception_ptr(SocketException(toSocketError(code), err));
        throw System::IO::IOException("Unable to read data from the transport connection: " + err + ".", inner);
    }
    return static_cast<intcs>(n);
#endif
}

void NetworkStream::Write(const bytecs buffer[], intcs offset, intcs count) {
    if (offset < 0)
        throw System::ArgumentOutOfRangeException("offset", "Non-negative number required.");
    if (count < 0)
        throw System::ArgumentOutOfRangeException("count", "Non-negative number required.");
#if defined(__EMSCRIPTEN__)
    (void)buffer; (void)offset; (void)count;
    throw System::PlatformNotSupportedException("NetworkStream is not supported on Emscripten.");
#else
    ThrowIfClosed();
    if (count == 0) return;
    auto n = ::send(toSk(fd_), reinterpret_cast<const char*>(buffer + offset),
                    static_cast<SockLen>(count), 0);
    if (n < 0) {
        auto code = lastErrorCode();
        auto err = netErr();
        auto inner = std::make_exception_ptr(SocketException(toSocketError(code), err));
        throw System::IO::IOException("Unable to write data to the transport connection: " + err + ".", inner);
    }
#endif
}

void NetworkStream::Close() {
    if (validFd(fd_)) {
        closeSk(fd_);
        fd_ = -1;
    }
}

intcs NetworkStream::getLengthProperty() const {
    throw System::NotSupportedException("NetworkStream does not support seeking.");
}

} // namespace System::Net::Sockets
