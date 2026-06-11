// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Sockets/NetworkStream.hpp"
#include "System/NotSupportedException.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <cerrno>
#include <cstring>

namespace System::Net::Sockets {

NetworkStream::NetworkStream(int fd) : fd_(fd) {}

NetworkStream::~NetworkStream() { Close(); }

intcs NetworkStream::Read(bytecs buffer[], intcs offset, intcs count) {
    if (fd_ < 0) return 0;
    auto n = ::recv(fd_, reinterpret_cast<char*>(buffer + offset), static_cast<size_t>(count), 0);
    if (n < 0) throw std::runtime_error(std::string("NetworkStream::Read failed: ") + std::strerror(errno));
    return static_cast<intcs>(n);
}

void NetworkStream::Write(const bytecs buffer[], intcs offset, intcs count) {
    if (fd_ < 0) return;
    auto n = ::send(fd_, reinterpret_cast<const char*>(buffer + offset), static_cast<size_t>(count), 0);
    if (n < 0) throw std::runtime_error(std::string("NetworkStream::Write failed: ") + std::strerror(errno));
}

void NetworkStream::Close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

intcs NetworkStream::getLengthProperty() const {
    throw System::NotSupportedException("NetworkStream does not support seeking.");
}

} // namespace System::Net::Sockets
