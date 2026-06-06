// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/Net/IPEndPoint.hpp"
#include "System/NotImplementedException.hpp"

// NOTE: Full implementation requires POSIX sockets (Linux/macOS) or Winsock2 (Windows).
// To implement:
//   Linux/macOS: socket(AF_INET, SOCK_DGRAM, 0) / bind / sendto / recvfrom / close
//   Windows:     WSAStartup / socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) / bind / sendto / recvfrom / closesocket
// See TcpClient.hpp for general socket integration notes.

namespace System::Net::Sockets {

    /**
     * @brief Provides UDP network services.
     *
     * Partial C++ counterpart of .NET System.Net.Sockets.UdpClient.
     *
     * @note Status: Stub — all methods throw NotImplementedException.
     *   See the comment at the top of this file for POSIX/Winsock integration notes.
     */
    class UdpClient {
    public:
        UdpClient() = default;
        explicit UdpClient(int /*port*/) {}
        explicit UdpClient(const IPEndPoint&) {}

        void Connect(const std::string& /*hostname*/, int /*port*/) {
            throw NotImplementedException(
                "UdpClient::Connect is not implemented. "
                "Requires POSIX sockets (sendto/recvfrom) or Winsock2. "
                "See include/System/Net/Sockets/UdpClient.hpp for integration notes.");
        }

        void Connect(const IPEndPoint&) {
            throw NotImplementedException("UdpClient::Connect requires POSIX sockets or Winsock2.");
        }

        int Send(const std::vector<SharpRuntime::bytecs>& /*dgram*/, int /*bytes*/) {
            throw NotImplementedException("UdpClient::Send requires POSIX sockets or Winsock2.");
        }

        std::vector<SharpRuntime::bytecs> Receive(IPEndPoint& /*remoteEP*/) {
            throw NotImplementedException("UdpClient::Receive requires POSIX sockets or Winsock2.");
        }

        void Close() {}
        [[nodiscard]] bool getClientProperty() const { return false; }
    };

} // namespace System::Net::Sockets
