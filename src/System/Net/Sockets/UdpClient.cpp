// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Sockets/UdpClient.hpp"
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
    static const SockFd kBad = -1;
    inline int    toFd(SockFd s)  { return s; }
    inline SockFd toSk(int fd)    { return fd; }
    inline void   closeSk(int fd) { ::close(fd); }
    inline bool   validFd(int fd) { return fd >= 0; }
    inline std::string netErr()          { return std::strerror(errno); }
    inline std::string gaErr(int rc)     { return ::gai_strerror(rc); }
    inline void wsaInit() {}
}
#endif

namespace System::Net::Sockets {

#if !defined(__EMSCRIPTEN__)
static int makeUdpSocket() {
    wsaInit();
    SockFd fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == kBad)
        throw std::runtime_error(std::string("UdpClient: socket() failed: ") + netErr());
    return toFd(fd);
}
#endif

UdpClient::UdpClient() {
#if defined(__EMSCRIPTEN__)
    throw System::PlatformNotSupportedException("UdpClient is not supported on Emscripten.");
#else
    fd_ = makeUdpSocket();
#endif
}

UdpClient::UdpClient(int port) {
#if defined(__EMSCRIPTEN__)
    (void)port;
    throw System::PlatformNotSupportedException("UdpClient is not supported on Emscripten.");
#else
    fd_ = makeUdpSocket();
    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = ::htons(static_cast<uint16_t>(port));
    if (::bind(toSk(fd_), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        auto err = netErr();
        closeSk(fd_); fd_ = -1;
        throw std::runtime_error(std::string("UdpClient: bind() failed: ") + err);
    }
#endif
}

UdpClient::UdpClient(const Net::IPEndPoint& localEP) {
#if defined(__EMSCRIPTEN__)
    (void)localEP;
    throw System::PlatformNotSupportedException("UdpClient is not supported on Emscripten.");
#else
    fd_ = makeUdpSocket();
    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ::htonl(localEP.getAddressProperty().getAddressProperty());
    addr.sin_port        = ::htons(static_cast<uint16_t>(localEP.getPortProperty()));
    if (::bind(toSk(fd_), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        auto err = netErr();
        closeSk(fd_); fd_ = -1;
        throw std::runtime_error(std::string("UdpClient: bind() failed: ") + err);
    }
#endif
}

UdpClient::~UdpClient() { Close(); }

void UdpClient::Connect(const std::string& hostname, int port) {
#if defined(__EMSCRIPTEN__)
    (void)hostname; (void)port;
    throw System::PlatformNotSupportedException("UdpClient is not supported on Emscripten.");
#else
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    auto portStr = std::to_string(port);
    int rc = ::getaddrinfo(hostname.c_str(), portStr.c_str(), &hints, &res);
    if (rc != 0)
        throw std::runtime_error(std::string("UdpClient::Connect: DNS failed: ") + gaErr(rc));

    auto* sa = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
    remote_.setAddressProperty(Net::IPAddress(::ntohl(sa->sin_addr.s_addr)));
    remote_.setPortProperty(static_cast<SharpRuntime::intcs>(::ntohs(sa->sin_port)));
    ::freeaddrinfo(res);

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ::htonl(remote_.getAddressProperty().getAddressProperty());
    addr.sin_port        = ::htons(static_cast<uint16_t>(remote_.getPortProperty()));
    if (::connect(toSk(fd_), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error(std::string("UdpClient::Connect: connect() failed: ") + netErr());
    hasRemote_ = true;
#endif
}

void UdpClient::Connect(const Net::IPEndPoint& remoteEP) {
#if defined(__EMSCRIPTEN__)
    (void)remoteEP;
    throw System::PlatformNotSupportedException("UdpClient is not supported on Emscripten.");
#else
    remote_ = remoteEP;
    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ::htonl(remoteEP.getAddressProperty().getAddressProperty());
    addr.sin_port        = ::htons(static_cast<uint16_t>(remoteEP.getPortProperty()));
    if (::connect(toSk(fd_), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error(std::string("UdpClient::Connect: connect() failed: ") + netErr());
    hasRemote_ = true;
#endif
}

int UdpClient::Send(const std::vector<SharpRuntime::bytecs>& dgram, int bytes) {
#if defined(__EMSCRIPTEN__)
    (void)dgram; (void)bytes;
    throw System::PlatformNotSupportedException("UdpClient is not supported on Emscripten.");
#else
    if (!hasRemote_)
        throw std::runtime_error("UdpClient::Send: must call Connect() before Send().");
    auto n = ::send(toSk(fd_), reinterpret_cast<const char*>(dgram.data()),
                    static_cast<size_t>(bytes), 0);
    if (n < 0)
        throw std::runtime_error(std::string("UdpClient::Send: send() failed: ") + netErr());
    return static_cast<int>(n);
#endif
}

std::vector<SharpRuntime::bytecs> UdpClient::Receive(Net::IPEndPoint& remoteEP) {
#if defined(__EMSCRIPTEN__)
    (void)remoteEP;
    throw System::PlatformNotSupportedException("UdpClient is not supported on Emscripten.");
#else
    std::vector<SharpRuntime::bytecs> buf(65536);
    struct sockaddr_in sender{};
    socklen_t len = sizeof(sender);
    auto n = ::recvfrom(toSk(fd_), reinterpret_cast<char*>(buf.data()), buf.size(), 0,
                        reinterpret_cast<struct sockaddr*>(&sender), &len);
    if (n < 0)
        throw std::runtime_error(std::string("UdpClient::Receive: recvfrom() failed: ") + netErr());
    remoteEP.setAddressProperty(Net::IPAddress(::ntohl(sender.sin_addr.s_addr)));
    remoteEP.setPortProperty(static_cast<SharpRuntime::intcs>(::ntohs(sender.sin_port)));
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
