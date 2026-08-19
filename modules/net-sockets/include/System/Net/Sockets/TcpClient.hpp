// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <memory>
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/AddressFamily.hpp"
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
     * @par IPv6 and dual-stack (#2363)
     * Every connect, bind and accept path carries either family. The rule is .NET's: **the
     * endpoint decides**. A client constructed from an `IPEndPoint` gets a socket of that
     * endpoint's family (`TCPClient.cs:50-51`), `Connect(remoteEP)` on an *unbound* client
     * likewise, and `Connect(hostname, port)` resolves with `AF_UNSPEC` and tries **every**
     * returned address in turn, so an IPv6-only host connects and an IPv6 literal is no longer
     * reported as a DNS failure.
     *
     * The one place a family can be wrong is a client already **bound** to a local endpoint: it
     * owns a socket, and an endpoint of another family cannot be used with it. That raises
     * `System::ArgumentException` with .NET's own text — *"The supplied EndPoint of AddressFamily
     * {0} is not valid for this Socket, use {1} instead."* (`Socket.cs:1757-1759`). #2138's
     * unconditional IPv4 refusal is **gone**, not made unreachable; what replaced it is the
     * narrower check .NET actually has.
     *
     * `IPV6_V6ONLY` is left at the operating system's default, exactly as .NET does — it is
     * exposed there as `Socket.DualMode` and never forced at construction — so on Linux an IPv6
     * client or listener also reaches IPv4 peers.
     *
     * @note Status: Implemented — Windows (Winsock2) and POSIX (Linux/macOS).
     */
    class TcpClient {
        [[maybe_unused]] int  fd_        = -1;
        [[maybe_unused]] bool connected_ = false;
        // #2363. .NET's `TcpClient` carries the same state for the same reason (`private
        // AddressFamily _family;`, TCPClient.cs:17): once a socket exists its family is not
        // recoverable from an fd without a syscall, and every family decision after construction
        // needs it.
        //
        // It is stored as a BOOL, and that is measured rather than assumed. The first cut used an
        // `AddressFamily` and grew sizeof(TcpClient) from 24 to 32 -- the three bytes of padding
        // after `connected_` cannot hold a 4-byte, 4-aligned enum. A bool fits, and it loses
        // nothing: this port's `IPAddress` is itself `bool isIPv6_` with
        // `getAddressFamilyProperty()` computed from it (IPAddress.cpp:326-328), so a `TcpClient`
        // cannot carry a third family any more than an address can. The public accessor still
        // returns `AddressFamily`, so nothing about the surface changes -- only the cost, which
        // is now zero and needs no consumer rebuild.
        [[maybe_unused]] bool isIPv6_    = false;
        // Verified against TCPClient.cs's GetStream(): `_dataStream ??= new NetworkStream(...)`
        // -- real .NET creates the NetworkStream once and returns the SAME cached instance on
        // every subsequent call. This port previously created a brand-new NetworkStream (and,
        // on Windows, transferred fd_ away entirely) on every call, so a second GetStream() call
        // returned an unrelated stream on POSIX and outright threw InvalidOperationException on
        // Windows (since fd_ had already been transferred to the first stream and connected_ was
        // reset to false).
        mutable std::shared_ptr<NetworkStream> stream_;

        /** @brief Constructs a TcpClient that already owns a connected socket fd (used by TcpListener). */
        TcpClient(int connectedFd, AddressFamily family);
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

        /**
         * @brief The address family of this client's socket (#2363).
         *
         * .NET reaches the same value as `tcpClient.Client.AddressFamily`; this port has no
         * public `Client` property, so the family is published directly. For a client that has
         * neither bound nor connected it is `InterNetwork`, matching .NET's IPv4 fallback when no
         * family has been chosen.
         */
        [[nodiscard]] AddressFamily getAddressFamilyProperty() const {
            return isIPv6_ ? AddressFamily::InterNetworkV6 : AddressFamily::InterNetwork;
        }
    };

    /**
     * @brief Listens for connections from TCP network clients.
     *
     * Partial C++ counterpart of .NET System.Net.Sockets.TcpListener.
     * Uses Winsock2 on Windows, POSIX sockets on Linux/macOS,
     * and throws PlatformNotSupportedException on Emscripten.
     *
     * @par IPv6 (#2363) — see `TcpClient`'s note. `Start()` creates a socket of the constructed
     * endpoint's family (`TCPListener.cs:29,45`) and `AcceptTcpClient()` reads the peer through a
     * `sockaddr_storage`, so an accepted client carries the peer's real family. Neither
     * constructor refuses a family any more, because neither has a socket to disagree with.
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
