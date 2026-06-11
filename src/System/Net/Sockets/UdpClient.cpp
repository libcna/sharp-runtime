// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Sockets/UdpClient.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdexcept>
#include <cerrno>
#include <cstring>

namespace System::Net::Sockets {

static int makeUdpSocket() {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        throw std::runtime_error(std::string("UdpClient: socket() failed: ") + std::strerror(errno));
    return fd;
}

UdpClient::UdpClient() : fd_(makeUdpSocket()) {}

UdpClient::UdpClient(int port) : fd_(makeUdpSocket()) {
    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = ::htons(static_cast<uint16_t>(port));
    if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        int err = errno;
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error(std::string("UdpClient: bind() failed: ") + std::strerror(err));
    }
}

UdpClient::UdpClient(const Net::IPEndPoint& localEP) : fd_(makeUdpSocket()) {
    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ::htonl(localEP.getAddressProperty().getAddressProperty());
    addr.sin_port        = ::htons(static_cast<uint16_t>(localEP.getPortProperty()));
    if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        int err = errno;
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error(std::string("UdpClient: bind() failed: ") + std::strerror(err));
    }
}

UdpClient::~UdpClient() { Close(); }

void UdpClient::Connect(const std::string& hostname, int port) {
    struct addrinfo hints{};
    struct addrinfo* res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    auto portStr = std::to_string(port);
    int rc = ::getaddrinfo(hostname.c_str(), portStr.c_str(), &hints, &res);
    if (rc != 0)
        throw std::runtime_error(std::string("UdpClient::Connect: DNS failed: ") + ::gai_strerror(rc));

    auto* sa = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
    remote_.setAddressProperty(Net::IPAddress(::ntohl(sa->sin_addr.s_addr)));
    remote_.setPortProperty(static_cast<SharpRuntime::intcs>(::ntohs(sa->sin_port)));
    ::freeaddrinfo(res);

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ::htonl(remote_.getAddressProperty().getAddressProperty());
    addr.sin_port        = ::htons(static_cast<uint16_t>(remote_.getPortProperty()));
    if (::connect(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error(std::string("UdpClient::Connect: connect() failed: ") + std::strerror(errno));

    hasRemote_ = true;
}

void UdpClient::Connect(const Net::IPEndPoint& remoteEP) {
    remote_ = remoteEP;

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ::htonl(remoteEP.getAddressProperty().getAddressProperty());
    addr.sin_port        = ::htons(static_cast<uint16_t>(remoteEP.getPortProperty()));
    if (::connect(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error(std::string("UdpClient::Connect: connect() failed: ") + std::strerror(errno));

    hasRemote_ = true;
}

int UdpClient::Send(const std::vector<SharpRuntime::bytecs>& dgram, int bytes) {
    if (!hasRemote_)
        throw std::runtime_error("UdpClient::Send: must call Connect() before Send().");
    auto n = ::send(fd_, reinterpret_cast<const char*>(dgram.data()),
                    static_cast<size_t>(bytes), 0);
    if (n < 0)
        throw std::runtime_error(std::string("UdpClient::Send: send() failed: ") + std::strerror(errno));
    return static_cast<int>(n);
}

std::vector<SharpRuntime::bytecs> UdpClient::Receive(Net::IPEndPoint& remoteEP) {
    std::vector<SharpRuntime::bytecs> buf(65536);
    struct sockaddr_in sender{};
    socklen_t len = sizeof(sender);
    auto n = ::recvfrom(fd_, reinterpret_cast<char*>(buf.data()), buf.size(), 0,
                        reinterpret_cast<struct sockaddr*>(&sender), &len);
    if (n < 0)
        throw std::runtime_error(std::string("UdpClient::Receive: recvfrom() failed: ") + std::strerror(errno));
    remoteEP.setAddressProperty(Net::IPAddress(::ntohl(sender.sin_addr.s_addr)));
    remoteEP.setPortProperty(static_cast<SharpRuntime::intcs>(::ntohs(sender.sin_port)));
    buf.resize(static_cast<size_t>(n));
    return buf;
}

void UdpClient::Close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_        = -1;
        hasRemote_ = false;
    }
}

} // namespace System::Net::Sockets
