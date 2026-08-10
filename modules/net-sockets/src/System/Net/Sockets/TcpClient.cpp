// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Sockets/TcpClient.hpp"
#include "System/InvalidOperationException.hpp"
#include "PortValidation.hpp"
#include "System/Net/Sockets/SocketException.hpp"
#include "System/Net/Sockets/detail/ErrnoTranslation.hpp"
#include <cstdio>
#include <limits>

#if defined(_WIN32)
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  if defined(_MSC_VER)
#    pragma comment(lib, "ws2_32.lib")
#  endif
#  include <mutex>
namespace {
    using SockFd = SOCKET;
    static const SockFd kBad = INVALID_SOCKET;
    inline int    toFd(SockFd s)  { return static_cast<int>(s); }
    inline SockFd toSk(int fd)    { return static_cast<SockFd>(fd); }
    inline void   closeSk(int fd) { ::closesocket(toSk(fd)); }
    inline bool   validFd(int fd) { return toSk(fd) != INVALID_SOCKET; }
    inline int lastErrorCode() { return WSAGetLastError(); }
    inline std::string netErr() {
        char buf[32]; snprintf(buf, sizeof(buf), "WSA error %d", WSAGetLastError()); return buf;
    }
    inline std::string gaErr(int /*rc*/) { return netErr(); }
    // Winsock error codes already share SocketError's own numbering (see SocketError.hpp's
    // doc comment) -- no translation needed here.
    inline System::Net::Sockets::SocketError toSocketError(int code) {
        return static_cast<System::Net::Sockets::SocketError>(code);
    }
    void wsaInit() {
        static std::once_flag f;
        std::call_once(f, []{ WSADATA d; WSAStartup(MAKEWORD(2,2), &d); });
    }
}
#elif defined(__EMSCRIPTEN__)
#  include "System/PlatformNotSupportedException.hpp"
namespace { /* no platform helpers needed on Emscripten */ }
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <sys/ioctl.h>
#  include <cerrno>
#  include <cstring>
namespace {
    using SockFd = int;
    static const SockFd kBad = -1;
    inline int    toFd(SockFd s)  { return s; }
    inline SockFd toSk(int fd)    { return fd; }
    inline void   closeSk(int fd) { ::close(fd); }
    inline bool   validFd(int fd) { return fd >= 0; }
    inline int lastErrorCode()               { return errno; }
    inline std::string netErr()              { return std::strerror(errno); }
    inline std::string gaErr(int rc)         { return ::gai_strerror(rc); }
    inline void wsaInit() {}
    // Verified against SocketErrorPal.Unix.cs's GetSocketErrorForNativeError: real .NET
    // translates POSIX errno into the WSA-numbered SocketError space before constructing a
    // SocketException. This port previously cast the raw POSIX errno straight into
    // SocketException's "errorCode" parameter unchanged (e.g. Linux's ECONNREFUSED=111 doesn't
    // remotely resemble SocketError::ConnectionRefused=10061), so SocketErrorCode never
    // matched any real SocketError value.
    inline System::Net::Sockets::SocketError toSocketError(int code) {
        return SharpRuntimeDetail::Net::Sockets::TranslateErrno(code);
    }
}
#endif

namespace System::Net::Sockets {

// ---------------------------------------------------------------------------
// TcpClient
// ---------------------------------------------------------------------------

TcpClient::TcpClient() = default;

// SR-AUD-266 (endpoint half) / #2137. This body was empty -- literally `{}`, with the parameter
// name commented out -- so a caller who asked to be bound to a specific local endpoint got an
// ephemeral one and no indication that their argument had been dropped. Measured before the
// repair (build-probe/2137_probe1_before.log): a client asked for local port 40601 and the kernel
// reported 40998 after Connect().
//
// Real .NET binds in this constructor (TCPClient.cs: `_clientSocket.Bind(localEP)`), so a port
// already in use fails HERE rather than at some later Connect(). This port does the same, and
// Connect() below then CONNECTS THE SOCKET IT ALREADY OWNS instead of creating a second one --
// creating a second one is what discarded the endpoint in the first place.
//
// The bound-but-not-connected state needs no new member: `fd_ >= 0 && !connected_` was previously
// unreachable (the default constructor leaves fd_ == -1 and the fd-taking constructor sets
// connected_ from the fd), so it is available to mean exactly this. sizeof(TcpClient) is
// unchanged and no member moves.
TcpClient::TcpClient(const IPEndPoint& localEP) {
#if defined(__EMSCRIPTEN__)
    (void)localEP;
    throw System::PlatformNotSupportedException("TcpClient is not supported on Emscripten.");
#else
    wsaInit();
    SockFd sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == kBad)
        throw SocketException(toSocketError(lastErrorCode()), "TcpClient: socket() failed: " + netErr());

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(localEP.getAddressProperty().getAddressProperty());
    addr.sin_port        = htons(static_cast<uint16_t>(localEP.getPortProperty()));

    if (::bind(toSk(sock), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        auto code = lastErrorCode();
        auto err  = netErr();
        closeSk(toFd(sock));
        throw SocketException(toSocketError(code), "TcpClient: bind() failed: " + err);
    }

    fd_        = toFd(sock);
    connected_ = false;
#endif
}

TcpClient::TcpClient(int connectedFd) : fd_(connectedFd), connected_(connectedFd >= 0) {}

TcpClient::~TcpClient() { Close(); }

void TcpClient::Connect(const std::string& hostname, int port) {
    // Measured before #2137: a NEGATIVE port was rejected here, but by getaddrinfo, as "DNS
    // failed: Servname not supported for ai_socktype" -- name resolution blamed for an argument
    // the caller controls -- while 65536 and 70000 were truncated by htons and produced a
    // "Connection refused" against the wrong port. The domain check comes first so both cases
    // name the parameter that is actually wrong.
    detail::ValidatePort(static_cast<SharpRuntime::intcs>(port));
#if defined(__EMSCRIPTEN__)
    (void)hostname;
    throw System::PlatformNotSupportedException("TcpClient is not supported on Emscripten.");
#else
    wsaInit();
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    auto portStr = std::to_string(port);
    int rc = ::getaddrinfo(hostname.c_str(), portStr.c_str(), &hints, &res);
    if (rc != 0)
        throw SocketException(SocketError::HostNotFound, "TcpClient::Connect: DNS failed: " + gaErr(rc));

    // A client constructed from a local endpoint already owns a socket BOUND to it. Creating a
    // fresh one here is exactly how the caller's endpoint used to be discarded, so the bound
    // socket is what gets connected. See the local-endpoint constructor for why `fd_ >= 0 &&
    // !connected_` is the state that means "bound, not yet connected".
    const bool bound = validFd(fd_) && !connected_;
    SockFd     sock  = bound ? toSk(fd_)
                             : ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == kBad) { auto code = lastErrorCode(); ::freeaddrinfo(res); throw SocketException(toSocketError(code), "socket(): " + netErr()); }
    if (::connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen)) < 0) {
        auto code = lastErrorCode();
        auto err = netErr();
        ::freeaddrinfo(res);
        // A bound socket stays owned by this object: closing it here would either leave fd_
        // dangling or silently discard the local endpoint the caller asked for. An unbound socket
        // was created by this call and is closed by it.
        if (!bound) closeSk(toFd(sock));
        throw SocketException(toSocketError(code), "TcpClient::Connect: connect() failed: " + err);
    }
    ::freeaddrinfo(res);
    if (!bound && validFd(fd_)) closeSk(fd_);
    fd_        = toFd(sock);
    connected_ = true;
#endif
}

void TcpClient::Connect(const IPEndPoint& remoteEP) {
#if defined(__EMSCRIPTEN__)
    (void)remoteEP;
    throw System::PlatformNotSupportedException("TcpClient is not supported on Emscripten.");
#else
    wsaInit();
    // Same rule as the hostname overload: a bound client connects the socket it already owns.
    const bool bound = validFd(fd_) && !connected_;
    SockFd     sock  = bound ? toSk(fd_) : ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == kBad)
        throw SocketException(toSocketError(lastErrorCode()), "TcpClient::Connect: socket() failed: " + netErr());

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(remoteEP.getAddressProperty().getAddressProperty());
    addr.sin_port        = htons(static_cast<uint16_t>(remoteEP.getPortProperty()));

    if (::connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        auto code = lastErrorCode();
        auto err = netErr();
        if (!bound) closeSk(toFd(sock));
        throw SocketException(toSocketError(code), "TcpClient::Connect: connect() failed: " + err);
    }
    if (!bound && validFd(fd_)) closeSk(fd_);
    fd_        = toFd(sock);
    connected_ = true;
#endif
}

void TcpClient::Close() {
#if !defined(__EMSCRIPTEN__)
    if (validFd(fd_)) {
        closeSk(fd_);
        fd_        = -1;
        connected_ = false;
    }
    // Drop the cached stream so a subsequent Connect()+GetStream() (this port allows reconnecting
    // a TcpClient after Close(), unlike real .NET where Dispose() is terminal) builds a fresh
    // NetworkStream instead of returning one bound to the now-closed fd.
    stream_.reset();
#endif
}

int TcpClient::Available() const {
#if defined(__EMSCRIPTEN__)
    return 0;
#elif defined(_WIN32)
    if (!validFd(fd_)) return 0;
    u_long n = 0;
    if (::ioctlsocket(toSk(fd_), FIONREAD, &n) < 0)
        throw SocketException(toSocketError(lastErrorCode()), "TcpClient::Available: ioctlsocket() failed: " + netErr());
    return static_cast<int>(n);
#else
    if (!validFd(fd_)) return 0;
    int n = 0;
    if (::ioctl(fd_, FIONREAD, &n) < 0)
        throw SocketException(toSocketError(lastErrorCode()), "TcpClient::Available: ioctl() failed: " + netErr());
    return n;
#endif
}

std::shared_ptr<NetworkStream> TcpClient::GetStream() const {
#if defined(__EMSCRIPTEN__)
    throw System::PlatformNotSupportedException("TcpClient is not supported on Emscripten.");
#else
    if (!connected_ || !validFd(fd_))
        throw System::InvalidOperationException("TcpClient::GetStream: client is not connected.");
    // Verified against TCPClient.cs's GetStream(): `return _dataStream ??= new
    // NetworkStream(Client, true);` -- real .NET caches and returns the SAME NetworkStream
    // instance on every call. Previously this created a brand-new stream on every call (a fresh
    // dup()'d fd on POSIX, or an outright fd-ownership transfer on Windows that left fd_ == -1
    // and connected_ == false after the first call, so a second call threw
    // InvalidOperationException there). Caching matches .NET's contract and incidentally removes
    // that Windows-only second-call bug.
    if (stream_) return stream_;
#  if defined(_WIN32)
    // Winsock has no dup() — transfer ownership to the NetworkStream.
    int transferred = fd_;
    const_cast<TcpClient*>(this)->fd_ = -1;
    const_cast<TcpClient*>(this)->connected_ = false;
    stream_ = std::make_shared<NetworkStream>(transferred);
#  else
    int dupfd = ::dup(fd_);
    if (dupfd < 0)
        throw SocketException(toSocketError(lastErrorCode()), "TcpClient::GetStream: dup() failed: " + netErr());
    // #2136 gave NetworkStream's constructor a validating body, so it can now throw -- and this
    // is the one place in the module that hands it a descriptor it has just created. Without this
    // guard a rejected construction would leak the dup(), which is exactly the accounting defect
    // the validation exists to prevent. A connected, blocking, stream socket passes every check,
    // so this is a path that should never be taken; it is here because "should never" is not an
    // accounting argument.
    try {
        stream_ = std::make_shared<NetworkStream>(dupfd);
    } catch (...) {
        ::close(dupfd);
        throw;
    }
#  endif
    return stream_;
#endif
}

// ---------------------------------------------------------------------------
// TcpListener
// ---------------------------------------------------------------------------

TcpListener::TcpListener(const IPEndPoint& localEP) : local_(localEP) {}

TcpListener::TcpListener(const IPAddress& addr, int port)
    : local_(addr, static_cast<SharpRuntime::intcs>(port)) {}

TcpListener::~TcpListener() { Stop(); }

void TcpListener::Start() {
#if defined(__EMSCRIPTEN__)
    throw System::PlatformNotSupportedException("TcpListener is not supported on Emscripten.");
#else
    wsaInit();
    if (validFd(fd_)) return;

    SockFd sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == kBad)
        throw SocketException(toSocketError(lastErrorCode()), "TcpListener::Start: socket() failed: " + netErr());

#  if defined(_WIN32)
    BOOL opt = TRUE;
    ::setsockopt(toSk(sock), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#  else
    int opt = 1;
    ::setsockopt(toFd(sock), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#  endif

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(local_.getAddressProperty().getAddressProperty());
    addr.sin_port        = htons(static_cast<uint16_t>(local_.getPortProperty()));

    if (::bind(toSk(sock), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        auto code = lastErrorCode();
        auto err = netErr();
        closeSk(toFd(sock));
        throw SocketException(toSocketError(code), "TcpListener::Start: bind() failed: " + err);
    }
    // Verified against TCPListener.cs's parameterless Start(), which delegates to
    // Start((int)SocketOptionName.MaxConnections) -- MaxConnections == 0x7fffffff (INT32_MAX).
    // Real .NET always requests the platform's maximum backlog by default (the OS clamps it
    // to e.g. /proc/sys/net/core/somaxconn on Linux internally); this port previously
    // hardcoded a backlog of 5, which could silently start dropping incoming connections
    // under any real concurrent load far below what the OS would otherwise allow.
    if (::listen(toSk(sock), std::numeric_limits<int>::max()) < 0) {
        auto code = lastErrorCode();
        auto err = netErr();
        closeSk(toFd(sock));
        throw SocketException(toSocketError(code), "TcpListener::Start: listen() failed: " + err);
    }

    fd_ = toFd(sock);

    if (local_.getPortProperty() == 0) {
        struct sockaddr_in actual{};
        socklen_t len = sizeof(actual);
        if (::getsockname(toSk(fd_), reinterpret_cast<struct sockaddr*>(&actual), &len) == 0)
            local_.setPortProperty(static_cast<SharpRuntime::intcs>(ntohs(actual.sin_port)));
    }
#endif
}

void TcpListener::Stop() {
#if !defined(__EMSCRIPTEN__)
    if (validFd(fd_)) {
        closeSk(fd_);
        fd_ = -1;
    }
#endif
}

TcpClient TcpListener::AcceptTcpClient() {
#if defined(__EMSCRIPTEN__)
    throw System::PlatformNotSupportedException("TcpListener is not supported on Emscripten.");
#else
    if (!validFd(fd_))
        throw System::InvalidOperationException("TcpListener::AcceptTcpClient: listener is not started.");
    struct sockaddr_in clientAddr{};
    socklen_t len = sizeof(clientAddr);
    SockFd clientSock = ::accept(toSk(fd_), reinterpret_cast<struct sockaddr*>(&clientAddr), &len);
    if (clientSock == kBad)
        throw SocketException(toSocketError(lastErrorCode()), "TcpListener::AcceptTcpClient: accept() failed: " + netErr());
    return TcpClient(toFd(clientSock));
#endif
}

} // namespace System::Net::Sockets
