// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Sockets/TcpClient.hpp"
#include <stdexcept>
#include <cstdio>

#if defined(_WIN32)
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
#  include <mutex>
namespace {
    using SockFd = SOCKET;
    static const SockFd kBad = INVALID_SOCKET;
    inline int    toFd(SockFd s)  { return static_cast<int>(s); }
    inline SockFd toSk(int fd)    { return static_cast<SockFd>(fd); }
    inline void   closeSk(int fd) { ::closesocket(toSk(fd)); }
    inline bool   validFd(int fd) { return toSk(fd) != INVALID_SOCKET; }
    inline std::string netErr() {
        char buf[32]; snprintf(buf, sizeof(buf), "WSA error %d", WSAGetLastError()); return buf;
    }
    inline std::string gaErr(int /*rc*/) { return netErr(); }
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
    inline std::string netErr()              { return std::strerror(errno); }
    inline std::string gaErr(int rc)         { return ::gai_strerror(rc); }
    inline void wsaInit() {}
}
#endif

namespace System::Net::Sockets {

// ---------------------------------------------------------------------------
// TcpClient
// ---------------------------------------------------------------------------

TcpClient::TcpClient() = default;

TcpClient::TcpClient(const IPEndPoint& /*localEP*/) {}

TcpClient::TcpClient(int connectedFd) : fd_(connectedFd), connected_(connectedFd >= 0) {}

TcpClient::~TcpClient() { Close(); }

void TcpClient::Connect(const std::string& hostname, int port) {
#if defined(__EMSCRIPTEN__)
    (void)hostname; (void)port;
    throw System::PlatformNotSupportedException("TcpClient is not supported on Emscripten.");
#else
    wsaInit();
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    auto portStr = std::to_string(port);
    int rc = ::getaddrinfo(hostname.c_str(), portStr.c_str(), &hints, &res);
    if (rc != 0)
        throw std::runtime_error(std::string("TcpClient::Connect: DNS failed: ") + gaErr(rc));

    SockFd sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == kBad) { ::freeaddrinfo(res); throw std::runtime_error("socket(): " + netErr()); }
    if (::connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen)) < 0) {
        auto err = netErr();
        ::freeaddrinfo(res);
        closeSk(toFd(sock));
        throw std::runtime_error(std::string("TcpClient::Connect: connect() failed: ") + err);
    }
    ::freeaddrinfo(res);
    if (validFd(fd_)) closeSk(fd_);
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
    SockFd sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == kBad)
        throw std::runtime_error(std::string("TcpClient::Connect: socket() failed: ") + netErr());

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ::htonl(remoteEP.getAddressProperty().getAddressProperty());
    addr.sin_port        = ::htons(static_cast<uint16_t>(remoteEP.getPortProperty()));

    if (::connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        auto err = netErr();
        closeSk(toFd(sock));
        throw std::runtime_error(std::string("TcpClient::Connect: connect() failed: ") + err);
    }
    if (validFd(fd_)) closeSk(fd_);
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
#endif
}

int TcpClient::Available() const {
#if defined(__EMSCRIPTEN__)
    return 0;
#elif defined(_WIN32)
    if (!validFd(fd_)) return 0;
    u_long n = 0;
    if (::ioctlsocket(toSk(fd_), FIONREAD, &n) < 0)
        throw std::runtime_error(std::string("TcpClient::Available: ioctlsocket() failed: ") + netErr());
    return static_cast<int>(n);
#else
    if (!validFd(fd_)) return 0;
    int n = 0;
    if (::ioctl(fd_, FIONREAD, &n) < 0)
        throw std::runtime_error(std::string("TcpClient::Available: ioctl() failed: ") + netErr());
    return n;
#endif
}

std::shared_ptr<NetworkStream> TcpClient::GetStream() const {
#if defined(__EMSCRIPTEN__)
    throw System::PlatformNotSupportedException("TcpClient is not supported on Emscripten.");
#else
    if (!connected_ || !validFd(fd_))
        throw std::runtime_error("TcpClient::GetStream: client is not connected.");
#  if defined(_WIN32)
    // Winsock has no dup() — transfer ownership to the NetworkStream.
    int transferred = fd_;
    const_cast<TcpClient*>(this)->fd_ = -1;
    const_cast<TcpClient*>(this)->connected_ = false;
    return std::make_shared<NetworkStream>(transferred);
#  else
    int dupfd = ::dup(fd_);
    if (dupfd < 0)
        throw std::runtime_error(std::string("TcpClient::GetStream: dup() failed: ") + netErr());
    return std::make_shared<NetworkStream>(dupfd);
#  endif
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
        throw std::runtime_error(std::string("TcpListener::Start: socket() failed: ") + netErr());

#  if defined(_WIN32)
    BOOL opt = TRUE;
    ::setsockopt(toSk(sock), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#  else
    int opt = 1;
    ::setsockopt(toFd(sock), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#  endif

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ::htonl(local_.getAddressProperty().getAddressProperty());
    addr.sin_port        = ::htons(static_cast<uint16_t>(local_.getPortProperty()));

    if (::bind(toSk(sock), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        auto err = netErr();
        closeSk(toFd(sock));
        throw std::runtime_error(std::string("TcpListener::Start: bind() failed: ") + err);
    }
    if (::listen(toSk(sock), 5) < 0) {
        auto err = netErr();
        closeSk(toFd(sock));
        throw std::runtime_error(std::string("TcpListener::Start: listen() failed: ") + err);
    }

    fd_ = toFd(sock);

    if (local_.getPortProperty() == 0) {
        struct sockaddr_in actual{};
        socklen_t len = sizeof(actual);
        if (::getsockname(toSk(fd_), reinterpret_cast<struct sockaddr*>(&actual), &len) == 0)
            local_.setPortProperty(static_cast<SharpRuntime::intcs>(::ntohs(actual.sin_port)));
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
        throw std::runtime_error("TcpListener::AcceptTcpClient: listener is not started.");
    struct sockaddr_in clientAddr{};
    socklen_t len = sizeof(clientAddr);
    SockFd clientSock = ::accept(toSk(fd_), reinterpret_cast<struct sockaddr*>(&clientAddr), &len);
    if (clientSock == kBad)
        throw std::runtime_error(std::string("TcpListener::AcceptTcpClient: accept() failed: ") + netErr());
    return TcpClient(toFd(clientSock));
#endif
}

} // namespace System::Net::Sockets
