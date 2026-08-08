// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
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
    class UdpClient {
        int              fd_        = -1;
        Net::IPEndPoint  remote_{Net::IPAddress::Any, 0};
        bool             hasRemote_ = false;

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
    };

} // namespace System::Net::Sockets
