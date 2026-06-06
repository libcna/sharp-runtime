// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Net/IPEndPoint.hpp"
#include "System/NotImplementedException.hpp"

// NOTE: Full implementation requires POSIX sockets (Linux/macOS) or Winsock2 (Windows).
// To implement:
//   Linux/macOS: #include <sys/socket.h>, <netinet/in.h>, <arpa/inet.h>, <unistd.h>
//     socket() / connect() / send() / recv() / close()
//   Windows:     #include <winsock2.h>, <ws2tcpip.h>
//     WSAStartup / socket / connect / send / recv / closesocket
// Wrap the raw socket fd in a NetworkStream and expose it via GetStream().

namespace System::Net::Sockets {

    /**
     * @brief Provides client connections for TCP network services.
     *
     * Partial C++ counterpart of .NET System.Net.Sockets.TcpClient.
     *
     * @note Status: Stub — all methods throw NotImplementedException.
     *   See the comment at the top of this file for POSIX/Winsock integration notes.
     */
    class TcpClient {
    public:
        TcpClient() = default;
        explicit TcpClient(const IPEndPoint&) {}

        void Connect(const std::string& /*hostname*/, int /*port*/) {
            throw NotImplementedException(
                "TcpClient::Connect is not implemented. "
                "Requires POSIX sockets (connect/send/recv) or Winsock2. "
                "See include/System/Net/Sockets/TcpClient.hpp for integration notes.");
        }

        void Connect(const IPEndPoint&) {
            throw NotImplementedException("TcpClient::Connect requires POSIX sockets or Winsock2.");
        }

        void Close() {}

        [[nodiscard]] bool getConnectedProperty() const { return false; }

        [[nodiscard]] int Available() const {
            throw NotImplementedException("TcpClient::Available requires POSIX sockets or Winsock2.");
        }
    };

    /**
     * @brief Listens for connections from TCP network clients.
     *
     * Partial C++ counterpart of .NET System.Net.Sockets.TcpListener.
     *
     * @note Status: Stub — see TcpClient.hpp for integration notes.
     */
    class TcpListener {
    public:
        explicit TcpListener(const IPEndPoint&) {}
        TcpListener(const IPAddress&, int /*port*/) {}

        void Start() {
            throw NotImplementedException(
                "TcpListener::Start requires POSIX sockets (bind/listen/accept) or Winsock2.");
        }

        TcpClient AcceptTcpClient() {
            throw NotImplementedException("TcpListener::AcceptTcpClient requires POSIX sockets or Winsock2.");
        }

        void Stop() {}
    };

} // namespace System::Net::Sockets
