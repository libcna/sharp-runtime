// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Sockets/UdpClient.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "AddressFamilyValidation.hpp"
#include "IPEndPointNative.hpp"
#include "PortValidation.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Net/Sockets/SocketException.hpp"
#include "System/Net/Sockets/detail/ErrnoTranslation.hpp"
#include <algorithm>
#include <cstdio>

#if defined(_WIN32)
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  if defined(_MSC_VER)
#    pragma comment(lib, "ws2_32.lib")
#  endif
#  include <mutex>
namespace {
    using SockFd = SOCKET;
    // Winsock's recv/send/recvfrom take an int transfer length where POSIX takes size_t.
    // Every call site here passes either an intcs count or a fixed 65536 buffer size, so the
    // conversion is exact; naming the platform's own type keeps it from being an implicit
    // 64-to-32 narrowing (MSVC C4267).
    using SockLen = int;
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
namespace {
    inline bool validFd(int fd) { return fd >= 0; }
    inline void closeSk([[maybe_unused]] int fd) {}
}
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <cerrno>
#  include <cstring>
namespace {
    using SockFd = int;
    // POSIX recv/send/recvfrom take a size_t transfer length; see the Windows branch above.
    using SockLen = std::size_t;
    static const SockFd kBad = -1;
    inline int    toFd(SockFd s)  { return s; }
    inline SockFd toSk(int fd)    { return fd; }
    inline void   closeSk(int fd) { ::close(fd); }
    inline bool   validFd(int fd) { return fd >= 0; }
    inline int lastErrorCode()           { return errno; }
    inline std::string netErr()          { return std::strerror(errno); }
    inline std::string gaErr(int rc)     { return ::gai_strerror(rc); }
    inline void wsaInit() {}
    // Verified against SocketErrorPal.Unix.cs's GetSocketErrorForNativeError: real .NET
    // translates POSIX errno into the WSA-numbered SocketError space before constructing a
    // SocketException. This port previously cast the raw POSIX errno straight into
    // SocketException's "errorCode" parameter unchanged, so SocketErrorCode never matched any
    // real SocketError value.
    inline System::Net::Sockets::SocketError toSocketError(int code) {
        return SharpRuntimeDetail::Net::Sockets::TranslateErrno(code);
    }
}
#endif

namespace System::Net::Sockets {

#if !defined(__EMSCRIPTEN__)
static int makeUdpSocket(AddressFamily family) {
    wsaInit();
    SockFd fd = ::socket(detail::NativeFamilyOf(family), SOCK_DGRAM, 0);
    if (fd == kBad)
        throw SocketException(toSocketError(lastErrorCode()), "UdpClient: socket() failed: " + netErr());
    return toFd(fd);
}
#endif

// #2363 and a PREMISE CORRECTION. The ticket said the module's AF_INET constants must all become
// "the endpoint's or the resolved result's family". For this constructor and the port-taking one
// below, the constant is what .NET has: `public UdpClient() : this(AddressFamily.InterNetwork)`
// (UDPClient.cs:24) and `public UdpClient(int port) : this(port, AddressFamily.InterNetwork)`
// (:47). Unlike `TcpClient()`, which defaults to a dual-mode IPv6 socket, .NET's parameterless
// `UdpClient` is IPv4 and always has been. Making it dual-stack here would be a deviation dressed
// as a repair, so these two are deliberately unchanged and a test pins that they are.
UdpClient::UdpClient() {
#if defined(__EMSCRIPTEN__)
    throw System::PlatformNotSupportedException("UdpClient is not supported on Emscripten.");
#else
    fd_ = makeUdpSocket(getAddressFamilyProperty());
#endif
}

UdpClient::UdpClient(int port) {
    // SR-AUD-267 / #2137. Before this check the port went straight to
    // htons(static_cast<uint16_t>(port)), so 70000 bound port 4464 and -1 bound port 65535 -- the
    // caller's argument was truncated rather than refused. The check comes BEFORE makeUdpSocket()
    // deliberately: a rejected port must not leak the socket it would otherwise have created,
    // which is not a claim to be careful about if the socket never exists.
    detail::ValidatePort(static_cast<SharpRuntime::intcs>(port));
#if defined(__EMSCRIPTEN__)
    throw System::PlatformNotSupportedException("UdpClient is not supported on Emscripten.");
#else
    fd_ = makeUdpSocket(getAddressFamilyProperty());
    sockaddr_storage addr{};
    const socklen_t  addrLen = detail::BuildIPSockAddr(
        Net::IPEndPoint(Net::IPAddress::Any, static_cast<SharpRuntime::intcs>(port)), addr);
    if (::bind(toSk(fd_), reinterpret_cast<struct sockaddr*>(&addr), addrLen) < 0) {
        auto code = lastErrorCode();
        auto err = netErr();
        closeSk(fd_); fd_ = -1;
        throw SocketException(toSocketError(code), "UdpClient: bind() failed: " + err);
    }
#endif
}

// #2363: `_family = localEP.AddressFamily;` then `CreateClientSocket()` (UDPClient.cs:89-91) --
// the endpoint decides, so #2138's refusal has nothing left to refuse and is gone from here.
UdpClient::UdpClient(const Net::IPEndPoint& localEP) {
#if defined(__EMSCRIPTEN__)
    (void)localEP;
    throw System::PlatformNotSupportedException("UdpClient is not supported on Emscripten.");
#else
    isIPv6_ = localEP.getAddressProperty().getIsIPv6Property();
    fd_     = makeUdpSocket(getAddressFamilyProperty());
    sockaddr_storage addr{};
    const socklen_t  addrLen = detail::BuildIPSockAddr(localEP, addr);
    if (::bind(toSk(fd_), reinterpret_cast<struct sockaddr*>(&addr), addrLen) < 0) {
        auto code = lastErrorCode();
        auto err = netErr();
        closeSk(fd_); fd_ = -1;
        throw SocketException(toSocketError(code), "UdpClient: bind() failed: " + err);
    }
#endif
}

UdpClient::~UdpClient() { Close(); }

void UdpClient::Connect(const std::string& hostname, int port) {
    // Measured before #2137: a NEGATIVE port here was rejected by getaddrinfo as "DNS failed:
    // Servname not supported for ai_socktype" -- a SocketException blaming name resolution for an
    // argument the caller controls -- while 65536 and 70000 were silently truncated and connected
    // to the wrong port. Validating first makes both cases say the same, true thing.
    detail::ValidatePort(static_cast<SharpRuntime::intcs>(port));
#if defined(__EMSCRIPTEN__)
    (void)hostname;
    throw System::PlatformNotSupportedException("UdpClient is not supported on Emscripten.");
#else
    // #2363: AF_UNSPEC so an IPv6 host resolves at all, then .NET's filter -- and the filter is
    // the point. `UdpClient` ALWAYS owns a socket by the time this runs (every constructor makes
    // one), so unlike `TcpClient` it cannot adopt whatever family the resolver returns. .NET
    // walks the addresses and uses only those `IsAddressFamilyCompatible` accepts
    // (UDPClient.cs:698-706, 743-745), which is `family == _family || (family == InterNetwork &&
    // DualMode)`. A default (IPv4) `UdpClient` connecting to an IPv6-only host therefore still
    // fails -- that is .NET's behaviour, not a residue of the old limitation, and the way to
    // reach such a host is to construct the client from an IPv6 local endpoint.
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    auto portStr = std::to_string(port);
    int rc = ::getaddrinfo(hostname.c_str(), portStr.c_str(), &hints, &res);
    if (rc != 0)
        throw SocketException(SocketError::HostNotFound, "UdpClient::Connect: DNS failed: " + gaErr(rc));

    const int  myFamily = detail::NativeFamilyOf(getAddressFamilyProperty());
    const bool dual     = detail::SocketIsDualMode(fd_, getAddressFamilyProperty());

    int         lastCode = 0;
    std::string lastErr;
    bool        anyTried = false;
    for (struct addrinfo* it = res; it != nullptr; it = it->ai_next) {
        if (it->ai_family != myFamily && !(it->ai_family == AF_INET && dual)) continue;
        anyTried = true;
        sockaddr_storage native{};
        std::memcpy(&native, it->ai_addr, std::min<size_t>(it->ai_addrlen, sizeof(native)));
        Net::IPEndPoint resolved = detail::IPEndPointFromNative(native);
        // On a dual-mode socket an IPv4 result must be handed over as ::ffff:a.b.c.d, exactly as
        // .NET does before connecting (Socket.cs:1828).
        sockaddr_storage addr{};
        const socklen_t  addrLen =
            detail::BuildIPSockAddr(detail::AdaptEndPointForSocket(resolved, getAddressFamilyProperty()), addr);
        if (::connect(toSk(fd_), reinterpret_cast<struct sockaddr*>(&addr), addrLen) < 0) {
            lastCode = lastErrorCode();
            lastErr  = netErr();
            continue;
        }
        // `remote_` keeps the address as RESOLVED, not as mapped -- a caller who asked for an
        // IPv4 host should be told the IPv4 address they will actually exchange datagrams with.
        remote_    = resolved;
        hasRemote_ = true;
        ::freeaddrinfo(res);
        return;
    }
    ::freeaddrinfo(res);
    if (!anyTried)
        throw SocketException(SocketError::AddressFamilyNotSupported,
                              "UdpClient::Connect: the host resolved only to addresses of an "
                              "address family this client's socket cannot carry.");
    throw SocketException(toSocketError(lastCode), "UdpClient::Connect: connect() failed: " + lastErr);
#endif
}

// #2363: this is the ONE `UdpClient` door where a family disagreement is possible, because the
// socket already exists and the caller named a family. .NET raises `ArgumentException` here too --
// `UdpClient.Connect(IPEndPoint)` reaches `Socket.Connect`, whose first act is
// `if (!CanTryAddressFamily(remoteEP.AddressFamily)) throw new ArgumentException(...)`
// (Socket.cs:1757-1759). The exception #2138 introduced therefore SURVIVES here, with the same
// sentence, under a condition that is now true rather than unconditional.
void UdpClient::Connect(const Net::IPEndPoint& remoteEP) {
#if defined(__EMSCRIPTEN__)
    (void)remoteEP;
    throw System::PlatformNotSupportedException("UdpClient is not supported on Emscripten.");
#else
    detail::ValidateEndPointFamilyForSocket(
        remoteEP.getAddressProperty().getAddressFamilyProperty(), getAddressFamilyProperty(),
        detail::SocketIsDualMode(fd_, getAddressFamilyProperty()), "remoteEP");
    remote_ = remoteEP;
    sockaddr_storage addr{};
    const socklen_t  addrLen =
        detail::BuildIPSockAddr(detail::AdaptEndPointForSocket(remoteEP, getAddressFamilyProperty()), addr);
    if (::connect(toSk(fd_), reinterpret_cast<struct sockaddr*>(&addr), addrLen) < 0)
        throw SocketException(toSocketError(lastErrorCode()), "UdpClient::Connect: connect() failed: " + netErr());
    hasRemote_ = true;
#endif
}

int UdpClient::Send(const std::vector<SharpRuntime::bytecs>& dgram, int bytes) {
#if defined(__EMSCRIPTEN__)
    (void)dgram; (void)bytes;
    throw System::PlatformNotSupportedException("UdpClient is not supported on Emscripten.");
#else
    // Real .NET's UdpClient.Send(byte[], int, ...) routes through Socket.Send's
    // ValidateBufferArguments, which rejects a `bytes` count outside [0, dgram.Length]. This
    // port previously cast `bytes` straight to size_t with no check at all -- confirmed via a
    // standalone ASan repro that an oversized (or negative, wrapping to huge) count reads past
    // the end of `dgram` (heap-buffer-overflow).
    if (static_cast<uint32_t>(bytes) > dgram.size())
        throw System::ArgumentOutOfRangeException("bytes");
    if (!hasRemote_)
        throw System::InvalidOperationException("UdpClient::Send: must call Connect() before Send().");
    auto n = ::send(toSk(fd_), reinterpret_cast<const char*>(dgram.data()),
                    static_cast<SockLen>(bytes), 0);
    if (n < 0)
        throw SocketException(toSocketError(lastErrorCode()), "UdpClient::Send: send() failed: " + netErr());
    return static_cast<int>(n);
#endif
}

std::vector<SharpRuntime::bytecs> UdpClient::Receive(Net::IPEndPoint& remoteEP) {
#if defined(__EMSCRIPTEN__)
    (void)remoteEP;
    throw System::PlatformNotSupportedException("UdpClient is not supported on Emscripten.");
#else
    std::vector<SharpRuntime::bytecs> buf(65536);
    // #2363: a sockaddr_storage. With a sockaddr_in an IPv6 sender's 16-byte address was
    // truncated to the four bytes that fit, so the caller was handed a plausible but FABRICATED
    // IPv4 address for a datagram that came from somewhere else entirely -- the one genuinely
    // silent misreport in the family (the send and bind paths all failed loudly instead).
    sockaddr_storage sender{};
    socklen_t len = sizeof(sender);
    auto n = ::recvfrom(toSk(fd_), reinterpret_cast<char*>(buf.data()), static_cast<SockLen>(buf.size()), 0,
                        reinterpret_cast<struct sockaddr*>(&sender), &len);
    if (n < 0)
        throw SocketException(toSocketError(lastErrorCode()), "UdpClient::Receive: recvfrom() failed: " + netErr());
    remoteEP = detail::IPEndPointFromNative(sender);
    buf.resize(static_cast<size_t>(n));
    return buf;
#endif
}

void UdpClient::Close() {
    if (validFd(fd_)) {
        closeSk(fd_);
        fd_        = -1;
        hasRemote_ = false;
    }
}

} // namespace System::Net::Sockets
