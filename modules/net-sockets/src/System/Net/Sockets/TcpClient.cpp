// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Sockets/TcpClient.hpp"
#include "System/InvalidOperationException.hpp"
#include "AddressFamilyValidation.hpp"
#include "IPEndPointNative.hpp"
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
//
// #2363: the family is the ENDPOINT'S, not a constant. .NET's counterpart is
// `_family = localEP.AddressFamily;` immediately followed by `InitializeClientSocket()`
// (TCPClient.cs:50-51), with the comment "set before calling CreateSocket" -- so a socket door
// that takes an endpoint never has a family to disagree about, which is why #2138's refusal is
// gone from here rather than merely narrowed.
TcpClient::TcpClient(const IPEndPoint& localEP) {
#if defined(__EMSCRIPTEN__)
    (void)localEP;
    throw System::PlatformNotSupportedException("TcpClient is not supported on Emscripten.");
#else
    wsaInit();
    isIPv6_ = localEP.getAddressProperty().getIsIPv6Property();
    SockFd sock = ::socket(detail::NativeFamilyOf(getAddressFamilyProperty()), SOCK_STREAM, 0);
    if (sock == kBad)
        throw SocketException(toSocketError(lastErrorCode()), "TcpClient: socket() failed: " + netErr());

    sockaddr_storage addr{};
    const socklen_t  addrLen = detail::BuildIPSockAddr(localEP, addr);

    if (::bind(sock, reinterpret_cast<struct sockaddr*>(&addr), addrLen) < 0) {
        auto code = lastErrorCode();
        auto err  = netErr();
        closeSk(toFd(sock));
        throw SocketException(toSocketError(code), "TcpClient: bind() failed: " + err);
    }

    fd_        = toFd(sock);
    connected_ = false;
#endif
}

TcpClient::TcpClient(int connectedFd, AddressFamily family)
    : fd_(connectedFd), connected_(connectedFd >= 0),
      isIPv6_(family == AddressFamily::InterNetworkV6) {}

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
    // #2363: AF_UNSPEC, and every result is tried in turn.
    //
    // Two things this fixes at once. `hints.ai_family = AF_INET` meant `getaddrinfo` never
    // returned an IPv6 result, so an IPv6-only host -- and an IPv6 LITERAL, which needs no DNS at
    // all -- was reported as `SocketError::HostNotFound`, "DNS failed", about a name that
    // resolves perfectly. And taking only `res` (the first result) meant a host with several
    // addresses failed outright if the first was unreachable.
    //
    // .NET reaches the same two behaviours by a different route, and the route is worth naming
    // because it is what settles the question #2138 deferred: `Socket.Connect(string host, int
    // port)` calls `IPAddress.TryParse(host)` FIRST (Socket.cs:919-923) and only falls back to
    // `Dns.GetHostAddresses` + `Connect(IPAddress[], port)`. So .NET treats a literal arriving at
    // a hostname parameter as a literal, deliberately. #2138 left this door alone because that
    // decision looked like #2359's (`System::Uri`'s host grammar); measured against the
    // reference, it is not -- a URI authority has bracket syntax and a socket hostname parameter
    // does not, and .NET answers the socket question in the socket code. `getaddrinfo` with
    // AF_UNSPEC parses a literal of either family natively, so this port reaches .NET's answer
    // without a second address parser.
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
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
    // A BOUND socket has a family already, and only results it can carry may be tried -- .NET's
    // `CanTryAddressFamily` (Socket.cs:780-783). An unbound client has no family yet and takes
    // the first result that connects, whichever family that is.
    const int  boundFamily = bound ? detail::NativeFamilyOf(getAddressFamilyProperty()) : AF_UNSPEC;
    const bool boundDual   = bound && detail::SocketIsDualMode(fd_, getAddressFamilyProperty());

    int         lastCode = 0;
    std::string lastErr  = "no address returned for the host";
    bool        anyTried = false;

    for (struct addrinfo* it = res; it != nullptr; it = it->ai_next) {
        if (bound && it->ai_family != boundFamily
            && !(it->ai_family == AF_INET && boundDual))
            continue;
        anyTried = true;
        SockFd sock = bound ? toSk(fd_)
                            : ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (sock == kBad) { lastCode = lastErrorCode(); lastErr = "socket(): " + netErr(); continue; }
        if (::connect(sock, it->ai_addr, static_cast<int>(it->ai_addrlen)) < 0) {
            lastCode = lastErrorCode();
            lastErr  = netErr();
            // A bound socket stays owned by this object: closing it here would either leave fd_
            // dangling or silently discard the local endpoint the caller asked for. An unbound
            // socket was created by this call and is closed by it.
            if (!bound) closeSk(toFd(sock));
            continue;
        }
        const int connectedFamily = it->ai_family;
        ::freeaddrinfo(res);
        if (!bound && validFd(fd_)) closeSk(fd_);
        fd_        = toFd(sock);
        connected_ = true;
        if (!bound)
            isIPv6_ = connectedFamily == AF_INET6;
        return;
    }

    ::freeaddrinfo(res);
    if (!anyTried)
        throw SocketException(SocketError::AddressFamilyNotSupported,
                              "TcpClient::Connect: the host resolved only to addresses this "
                              "client's bound local endpoint cannot reach.");
    throw SocketException(toSocketError(lastCode), "TcpClient::Connect: connect() failed: " + lastErr);
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
    // #2363: only a socket that ALREADY EXISTS can disagree with the endpoint's family, and this
    // is the one `TcpClient` door where that is possible -- a client bound to an IPv4 local
    // endpoint cannot connect to an IPv6 peer. An unbound client creates the socket FROM the
    // endpoint, so there is nothing to check.
    if (bound)
        detail::ValidateEndPointFamilyForSocket(
            remoteEP.getAddressProperty().getAddressFamilyProperty(), getAddressFamilyProperty(),
            detail::SocketIsDualMode(fd_, getAddressFamilyProperty()), "remoteEP");

    const AddressFamily targetFamily =
        bound ? getAddressFamilyProperty() : remoteEP.getAddressProperty().getAddressFamilyProperty();
    SockFd sock = bound ? toSk(fd_) : ::socket(detail::NativeFamilyOf(targetFamily), SOCK_STREAM, 0);
    if (sock == kBad)
        throw SocketException(toSocketError(lastErrorCode()), "TcpClient::Connect: socket() failed: " + netErr());

    sockaddr_storage addr{};
    const socklen_t  addrLen =
        detail::BuildIPSockAddr(detail::AdaptEndPointForSocket(remoteEP, targetFamily), addr);

    if (::connect(sock, reinterpret_cast<struct sockaddr*>(&addr), addrLen) < 0) {
        auto code = lastErrorCode();
        auto err = netErr();
        if (!bound) closeSk(toFd(sock));
        throw SocketException(toSocketError(code), "TcpClient::Connect: connect() failed: " + err);
    }
    if (!bound && validFd(fd_)) closeSk(fd_);
    fd_        = toFd(sock);
    connected_ = true;
    isIPv6_    = targetFamily == AddressFamily::InterNetworkV6;
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

// #2138 made both constructors reject an IPv6 endpoint at CONSTRUCTION, which was the earliest
// honest point while every listener socket was AF_INET. #2363 removes the limitation, and with it
// the refusal: .NET's own constructors are `_serverSocket = new Socket(_serverSocketEP.
// AddressFamily, SocketType.Stream, ProtocolType.Tcp)` (TCPListener.cs:29,45) -- the endpoint
// DECIDES the family, so there is nothing left for a constructor to disagree with.
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

    // #2363: the family comes from the endpoint the listener was constructed with. .NET does not
    // set IPV6_V6ONLY here either -- it exposes the option as `Socket.DualMode` and otherwise
    // leaves the OS default (Socket.cs:745-770), which on Linux is `net.ipv6.bindv6only = 0`. A
    // listener on `IPAddress::IPv6Any` therefore accepts IPv4 peers too, as .NET's does, and this
    // port does not second-guess the system setting.
    const int nativeFamily = detail::NativeFamilyOf(local_.getAddressProperty());
    SockFd sock = ::socket(nativeFamily, SOCK_STREAM, 0);
    if (sock == kBad)
        throw SocketException(toSocketError(lastErrorCode()), "TcpListener::Start: socket() failed: " + netErr());

#  if defined(_WIN32)
    BOOL opt = TRUE;
    ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#  else
    int opt = 1;
    ::setsockopt(toFd(sock), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#  endif

    sockaddr_storage addr{};
    const socklen_t  addrLen = detail::BuildIPSockAddr(local_, addr);

    if (::bind(sock, reinterpret_cast<struct sockaddr*>(&addr), addrLen) < 0) {
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
    if (::listen(sock, std::numeric_limits<int>::max()) < 0) {
        auto code = lastErrorCode();
        auto err = netErr();
        closeSk(toFd(sock));
        throw SocketException(toSocketError(code), "TcpListener::Start: listen() failed: " + err);
    }

    fd_ = toFd(sock);

    if (local_.getPortProperty() == 0) {
        // #2363: read the port out of a sockaddr_storage of whichever family the socket actually
        // has. Reading `sin_port` from a sockaddr_in overlaid on an AF_INET6 reply happens to
        // land on `sin6_port` (both are the second field at offset 2), so this was not a live
        // defect -- but it was correct by coincidence of two layouts, not by construction, which
        // is not a property to leave a listener's ephemeral port resting on.
        sockaddr_storage actual{};
        socklen_t        len = sizeof(actual);
        if (::getsockname(toSk(fd_), reinterpret_cast<struct sockaddr*>(&actual), &len) == 0)
            local_.setPortProperty(detail::IPEndPointFromNative(actual).getPortProperty());
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
    // #2363: a sockaddr_storage, so an IPv6 peer's address is not truncated into 16 bytes of
    // sockaddr_in. The accepted client is told the peer's REAL family rather than inheriting a
    // constant -- which is the difference between `getRemoteEndPointProperty()` reporting the
    // peer and reporting `0.0.0.0:0`.
    sockaddr_storage clientAddr{};
    socklen_t        len = sizeof(clientAddr);
    SockFd clientSock = ::accept(toSk(fd_), reinterpret_cast<struct sockaddr*>(&clientAddr), &len);
    if (clientSock == kBad)
        throw SocketException(toSocketError(lastErrorCode()), "TcpListener::AcceptTcpClient: accept() failed: " + netErr());
    return TcpClient(toFd(clientSock),
                     clientAddr.ss_family == AF_INET6 ? AddressFamily::InterNetworkV6
                                                      : AddressFamily::InterNetwork);
#endif
}

} // namespace System::Net::Sockets
