// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <memory>
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/NetworkStream.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Net::Sockets {

    using SharpRuntime::intcs;

    /**
     * @brief Provides client connections for TCP network services.
     *
     * Partial C++ counterpart of .NET System.Net.Sockets.TcpClient.
     * Uses Winsock2 on Windows, POSIX sockets on Linux/macOS,
     * and throws PlatformNotSupportedException on Emscripten.
     *
     * @par IPv4 only — a measured, gated limitation (#2138, SR-AUD-266 family half)
     * Every connect, bind and accept path in this type is `AF_INET` with a `sockaddr_in`. The
     * limitation is **loud, not silent**, though not by design: an `IPEndPoint` carrying an IPv6
     * address reaches `IPAddress::getAddressProperty()`, which throws
     * `SocketException(OperationNotSupported)` — *"The requested property is not supported for
     * the 'InterNetworkV6' AddressFamily."* — and a hostname path refuses an IPv6 literal with
     * `SocketException(HostNotFound)` because `hints.ai_family` is `AF_INET`, so `getaddrinfo`
     * never resolves it. Nothing is silently narrowed to IPv4. **How far this port should carry
     * IPv6 is an open decision (#2138)**, not a bug awaiting a fix; the current behaviour is
     * pinned by `SocketsGatedBehaviourPins`.
     *
     * @note Status: Implemented — Windows (Winsock2) and POSIX (Linux/macOS).
     */
    class TcpClient {
        [[maybe_unused]] int  fd_        = -1;
        [[maybe_unused]] bool connected_ = false;
        // Verified against TCPClient.cs's GetStream(): `_dataStream ??= new NetworkStream(...)`
        // -- real .NET creates the NetworkStream once and returns the SAME cached instance on
        // every subsequent call. This port previously created a brand-new NetworkStream (and,
        // on Windows, transferred fd_ away entirely) on every call, so a second GetStream() call
        // returned an unrelated stream on POSIX and outright threw InvalidOperationException on
        // Windows (since fd_ had already been transferred to the first stream and connected_ was
        // reset to false).
        mutable std::shared_ptr<NetworkStream> stream_;

        /** @brief Constructs a TcpClient that already owns a connected socket fd (used by TcpListener). */
        explicit TcpClient(int connectedFd);
        friend class TcpListener;

    public:
        TcpClient();

        /**
         * @brief Creates a client whose socket is **bound** to @p localEP.
         *
         * Until #2137 this constructor had an empty body and the caller's endpoint was silently
         * discarded; a subsequent `Connect()` used an ephemeral local port. It now binds
         * immediately, as real .NET does (`TCPClient.cs: _clientSocket.Bind(localEP)`), so an
         * unavailable local endpoint fails here rather than surfacing later or not at all, and
         * `Connect()` connects **this** socket instead of creating a second one.
         *
         * A client in this state reports `getConnectedProperty() == false` — it is bound, not
         * connected — and `GetStream()` still refuses until a `Connect()` succeeds. If that
         * `Connect()` fails, the bound socket stays owned by this object rather than being
         * closed, so nothing leaks and the local endpoint is not silently dropped.
         *
         * @throws System::Net::Sockets::SocketException if the socket cannot be created or the
         *         endpoint cannot be bound.
         */
        explicit TcpClient(const IPEndPoint& localEP);
        ~TcpClient();

        // Not copyable: fd_ is a raw owned socket handle closed by the destructor -- an implicit
        // shallow copy (previously allowed) lets two instances' destructors both close the same
        // fd, either failing silently or, if the fd was reused in between, closing a handle this
        // instance doesn't own. Matches Socket's own established copy-deletion. Does not affect
        // TcpListener::AcceptTcpClient()'s `return TcpClient(fd);`, which is a direct prvalue
        // construction guaranteed to be copy-elided since C++17 -- no copy or move needed.
        TcpClient(const TcpClient&) = delete;
        TcpClient& operator=(const TcpClient&) = delete;

        /**
         * @brief Connects to a remote host by name and port.
         *
         * @throws System::ArgumentOutOfRangeException with `paramName == "port"` if @p port is
         *         outside `[IPEndPoint::MinPort, IPEndPoint::MaxPort]` (#2137). Previously a
         *         negative port surfaced as a `SocketException` about **DNS** and a port above
         *         65535 was truncated by `htons`, producing a connection attempt against a
         *         different port than the caller named.
         */
        void Connect(const std::string& hostname, intcs port);

        /** @brief Connects to the specified remote endpoint. */
        void Connect(const IPEndPoint& remoteEP);

        /** @brief Closes the underlying socket. */
        void Close();

        /** @brief Returns true when a connection has been established. */
        [[nodiscard]] bool getConnectedProperty() const { return connected_; }

        /** @brief Returns the number of bytes available to read without blocking. */
        [[nodiscard]] intcs Available() const;

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
     * @par IPv4 only (#2138) — see TcpClient's note; `Start()` and `AcceptTcpClient()` build
     * `sockaddr_in` unconditionally, and an IPv6 endpoint is refused rather than narrowed.
     *
     * @note Status: Implemented — Windows (Winsock2) and POSIX (Linux/macOS).
     */
    class TcpListener {
        [[maybe_unused]] int fd_    = -1;
        IPEndPoint           local_;

    public:
        explicit TcpListener(const IPEndPoint& localEP);
        TcpListener(const IPAddress& addr, intcs port);
        ~TcpListener();

        // Not copyable: fd_ is a raw owned socket handle closed by the destructor -- see
        // TcpClient's identical copy-deletion above for the full rationale.
        TcpListener(const TcpListener&) = delete;
        TcpListener& operator=(const TcpListener&) = delete;

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
