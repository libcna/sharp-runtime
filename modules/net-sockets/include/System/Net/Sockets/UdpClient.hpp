// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/AddressFamily.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Net::Sockets {

    using SharpRuntime::intcs;

    /**
     * @brief Provides UDP network services.
     *
     * Partial C++ counterpart of .NET System.Net.Sockets.UdpClient.
     * Uses Winsock2 on Windows, POSIX sockets on Linux/macOS,
     * and throws PlatformNotSupportedException on Emscripten.
     *
     * @par IPv6 (#2363), and why the DEFAULT is still IPv4
     * `UdpClient(const IPEndPoint&)` takes its family from the endpoint, and `Receive` reports an
     * IPv6 sender correctly. But `UdpClient()` and `UdpClient(port)` remain **IPv4**, and that is
     * .NET, not a leftover: `public UdpClient() : this(AddressFamily.InterNetwork)`
     * (`UDPClient.cs:24`) and `public UdpClient(int port) : this(port,
     * AddressFamily.InterNetwork)` (`:47`). Only `TcpClient`'s parameterless path is dual-mode in
     * .NET; `UdpClient`'s is not, and this port matches both rather than unifying them.
     *
     * Because a `UdpClient` always owns a socket by the time `Connect` runs, it cannot adopt the
     * resolver's family the way `TcpClient` can. `Connect(hostname, port)` resolves with
     * `AF_UNSPEC` and then uses only addresses the socket can carry — .NET's
     * `IsAddressFamilyCompatible` (`UDPClient.cs:743-745`) — and `Connect(remoteEP)` raises
     * `System::ArgumentException` for a family the socket cannot carry, with .NET's own text
     * (`Socket.cs:1757-1759`). To reach an IPv6 peer, construct the client from an IPv6 local
     * endpoint.
     *
     * @note Status: Implemented — Windows (Winsock2) and POSIX (Linux/macOS).
     */
    class UdpClient {
        int              fd_        = -1;
        Net::IPEndPoint  remote_{Net::IPAddress::Any, 0};
        bool             hasRemote_ = false;
        // #2363. .NET carries the same state with the same default: `private AddressFamily
        // _family = AddressFamily.InterNetwork;` (UDPClient.cs:21). Stored as a bool for the
        // reason spelled out in TcpClient.hpp -- an enum member costs eight bytes of layout that
        // a bool costs nothing for, and this port's own `IPAddress` represents the same fact the
        // same way. sizeof(UdpClient) is UNCHANGED, pinned below.
        bool             isIPv6_    = false;

    public:
        /** @brief Creates a UDP socket without binding to a specific port. */
        UdpClient();

        /**
         * @brief Creates a UDP socket bound to the given local port.
         *
         * @param port A port in `[IPEndPoint::MinPort, IPEndPoint::MaxPort]`.
         * @throws System::ArgumentOutOfRangeException with `paramName == "port"` if @p port is
         *         outside that range. Until #2137 the value was truncated instead
         *         (`htons(static_cast<uint16_t>(port))`), so `70000` bound port `4464` and `-1`
         *         bound port `65535`. The check runs **before** the socket is created, so a
         *         rejected port cannot leak one.
         */
        explicit UdpClient(intcs port);

        /** @brief Creates a UDP socket bound to the given local endpoint. */
        explicit UdpClient(const Net::IPEndPoint& localEP);

        ~UdpClient();

        // Not copyable: fd_ is a raw owned socket handle closed by the destructor -- an implicit
        // shallow copy (previously allowed) lets two instances' destructors both close the same
        // fd, either failing silently or, if the fd was reused in between, closing a handle this
        // instance doesn't own. Matches Socket's own established copy-deletion.
        UdpClient(const UdpClient&) = delete;
        UdpClient& operator=(const UdpClient&) = delete;

        /**
         * @brief Sets the default remote host/port for subsequent Send calls.
         *
         * @throws System::ArgumentOutOfRangeException with `paramName == "port"` if @p port is
         *         outside `[IPEndPoint::MinPort, IPEndPoint::MaxPort]` (#2137). Previously a
         *         negative port surfaced as a `SocketException` about **DNS**, because
         *         `getaddrinfo` rejected the service string, and a port above 65535 was silently
         *         truncated.
         */
        void Connect(const std::string& hostname, intcs port);

        /** @brief Sets the default remote endpoint for subsequent Send calls. */
        void Connect(const Net::IPEndPoint& remoteEP);

        /** @brief Sends a datagram to the default remote endpoint (must call Connect first). */
        intcs Send(const std::vector<SharpRuntime::bytecs>& dgram, intcs bytes);

        /** @brief Receives a UDP datagram; fills remoteEP with the sender's endpoint. */
        std::vector<SharpRuntime::bytecs> Receive(Net::IPEndPoint& remoteEP);

        /** @brief Closes the underlying socket. */
        void Close();

        /** @brief Returns true when the socket is open. */
        [[nodiscard]] bool getClientProperty() const { return fd_ >= 0; }

        /**
         * @brief The address family of this client's socket (#2363).
         *
         * `InterNetwork` unless the client was constructed from an IPv6 local endpoint, matching
         * .NET's `_family` default of `AddressFamily.InterNetwork` (`UDPClient.cs:21`).
         */
        [[nodiscard]] AddressFamily getAddressFamilyProperty() const {
            return isIPv6_ ? AddressFamily::InterNetworkV6 : AddressFamily::InterNetwork;
        }
    };

} // namespace System::Net::Sockets
