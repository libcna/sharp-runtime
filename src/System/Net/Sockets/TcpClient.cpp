// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Sockets/TcpClient.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdexcept>
#include <cerrno>
#include <cstring>

namespace System::Net::Sockets {

// ---------------------------------------------------------------------------
// TcpClient
// ---------------------------------------------------------------------------

TcpClient::TcpClient() = default;

TcpClient::TcpClient(const IPEndPoint& /*localEP*/) {}

TcpClient::TcpClient(int connectedFd) : fd_(connectedFd), connected_(connectedFd >= 0) {}

TcpClient::~TcpClient() { Close(); }

void TcpClient::Connect(const std::string& hostname, int port) {
    struct addrinfo hints{};
    struct addrinfo* res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    auto portStr = std::to_string(port);
    int rc = ::getaddrinfo(hostname.c_str(), portStr.c_str(), &hints, &res);
    if (rc != 0)
        throw std::runtime_error(std::string("TcpClient::Connect: DNS resolution failed: ") + ::gai_strerror(rc));

    int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        ::freeaddrinfo(res);
        throw std::runtime_error(std::string("TcpClient::Connect: socket() failed: ") + std::strerror(errno));
    }

    if (::connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        int err = errno;
        ::freeaddrinfo(res);
        ::close(fd);
        throw std::runtime_error(std::string("TcpClient::Connect: connect() failed: ") + std::strerror(err));
    }

    ::freeaddrinfo(res);

    if (fd_ >= 0) ::close(fd_);
    fd_        = fd;
    connected_ = true;
}

void TcpClient::Connect(const IPEndPoint& remoteEP) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        throw std::runtime_error(std::string("TcpClient::Connect: socket() failed: ") + std::strerror(errno));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ::htonl(remoteEP.getAddressProperty().getAddressProperty());
    addr.sin_port        = ::htons(static_cast<uint16_t>(remoteEP.getPortProperty()));

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        int err = errno;
        ::close(fd);
        throw std::runtime_error(std::string("TcpClient::Connect: connect() failed: ") + std::strerror(err));
    }

    if (fd_ >= 0) ::close(fd_);
    fd_        = fd;
    connected_ = true;
}

void TcpClient::Close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_        = -1;
        connected_ = false;
    }
}

int TcpClient::Available() const {
    if (fd_ < 0) return 0;
    int n = 0;
    if (::ioctl(fd_, FIONREAD, &n) < 0)
        throw std::runtime_error(std::string("TcpClient::Available: ioctl() failed: ") + std::strerror(errno));
    return n;
}

std::shared_ptr<NetworkStream> TcpClient::GetStream() const {
    if (!connected_ || fd_ < 0)
        throw std::runtime_error("TcpClient::GetStream: client is not connected.");
    int dupfd = ::dup(fd_);
    if (dupfd < 0)
        throw std::runtime_error(std::string("TcpClient::GetStream: dup() failed: ") + std::strerror(errno));
    return std::make_shared<NetworkStream>(dupfd);
}

// ---------------------------------------------------------------------------
// TcpListener
// ---------------------------------------------------------------------------

TcpListener::TcpListener(const IPEndPoint& localEP) : local_(localEP) {}

TcpListener::TcpListener(const IPAddress& addr, int port)
    : local_(addr, static_cast<SharpRuntime::intcs>(port)) {}

TcpListener::~TcpListener() { Stop(); }

void TcpListener::Start() {
    if (fd_ >= 0) return;

    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0)
        throw std::runtime_error(std::string("TcpListener::Start: socket() failed: ") + std::strerror(errno));

    int opt = 1;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ::htonl(local_.getAddressProperty().getAddressProperty());
    addr.sin_port        = ::htons(static_cast<uint16_t>(local_.getPortProperty()));

    if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        int err = errno;
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error(std::string("TcpListener::Start: bind() failed: ") + std::strerror(err));
    }

    if (::listen(fd_, 5) < 0) {
        int err = errno;
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error(std::string("TcpListener::Start: listen() failed: ") + std::strerror(err));
    }

    // If port 0 was requested, read back the actual port the OS assigned.
    if (local_.getPortProperty() == 0) {
        struct sockaddr_in actual{};
        socklen_t len = sizeof(actual);
        if (::getsockname(fd_, reinterpret_cast<struct sockaddr*>(&actual), &len) == 0)
            local_.setPortProperty(static_cast<SharpRuntime::intcs>(::ntohs(actual.sin_port)));
    }
}

void TcpListener::Stop() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

TcpClient TcpListener::AcceptTcpClient() {
    if (fd_ < 0)
        throw std::runtime_error("TcpListener::AcceptTcpClient: listener is not started.");
    struct sockaddr_in clientAddr{};
    socklen_t len = sizeof(clientAddr);
    int clientFd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&clientAddr), &len);
    if (clientFd < 0)
        throw std::runtime_error(std::string("TcpListener::AcceptTcpClient: accept() failed: ") + std::strerror(errno));
    return TcpClient(clientFd);
}

} // namespace System::Net::Sockets
