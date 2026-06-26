// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <memory>
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/NetworkStream.hpp"

namespace System::Net::Sockets {

    /**
     * @brief Provides client connections for TCP network services.
     *
     * Partial C++ counterpart of .NET System.Net.Sockets.TcpClient.
     * Uses Winsock2 on Windows, POSIX sockets on Linux/macOS,
     * and throws PlatformNotSupportedException on Emscripten.
     *
     * @note Status: Implemented — Windows (Winsock2) and POSIX (Linux/macOS).
     */
    class TcpClient {
        [[maybe_unused]] int  fd_        = -1;
        [[maybe_unused]] bool connected_ = false;

        /** @brief Constructs a TcpClient that already owns a connected socket fd (used by TcpListener). */
        explicit TcpClient(int connectedFd);
        friend class TcpListener;

    public:
        TcpClient();
        explicit TcpClient(const IPEndPoint& localEP);
        ~TcpClient();

        /** @brief Connects to a remote host by name and port. */
        void Connect(const std::string& hostname, int port);

        /** @brief Connects to the specified remote endpoint. */
        void Connect(const IPEndPoint& remoteEP);

        /** @brief Closes the underlying socket. */
        void Close();

        /** @brief Returns true when a connection has been established. */
        [[nodiscard]] bool getConnectedProperty() const { return connected_; }

        /** @brief Returns the number of bytes available to read without blocking. */
        [[nodiscard]] int Available() const;

        /** @brief Returns a NetworkStream for reading and writing (dup-ed fd). */
        [[nodiscard]] std::shared_ptr<NetworkStream> GetStream() const;
    };

    /**
     * @brief Listens for connections from TCP network clients.
     *
     * Partial C++ counterpart of .NET System.Net.Sockets.TcpListener.
     * Uses Winsock2 on Windows, POSIX sockets on Linux/macOS,
     * and throws PlatformNotSupportedException on Emscripten.
     *
     * @note Status: Implemented — Windows (Winsock2) and POSIX (Linux/macOS).
     */
    class TcpListener {
        [[maybe_unused]] int fd_    = -1;
        IPEndPoint           local_;

    public:
        explicit TcpListener(const IPEndPoint& localEP);
        TcpListener(const IPAddress& addr, int port);
        ~TcpListener();

        /** @brief Starts listening for incoming connections (bind + listen). */
        void Start();

        /** @brief Stops listening and closes the socket. */
        void Stop();

        /** @brief Accepts a pending connection (blocks until a client connects). */
        TcpClient AcceptTcpClient();

        /** @brief Returns the local endpoint (valid after Start()). */
        [[nodiscard]] const IPEndPoint& getLocalEndpointProperty() const { return local_; }
    };

} // namespace System::Net::Sockets
