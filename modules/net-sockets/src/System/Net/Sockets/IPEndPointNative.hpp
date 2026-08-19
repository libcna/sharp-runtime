// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

//! @file
//! @brief The single definition of the `IPEndPoint` <-> native `sockaddr` conversion (#2363).
//!
//! Before this header there was **one** family-agnostic conversion in the module -- inside
//! `Socket.cpp`'s anonymous namespace -- and `TcpClient`, `TcpListener` and `UdpClient` each
//! hand-built a `sockaddr_in` instead, which is what made them IPv4-only. Rather than teaching
//! three more sites to build a `sockaddr_in6`, the one that already worked was lifted here and
//! `Socket.cpp` now delegates to it, so the module has one definition and not four.
//!
//! This is a **private** header under `src/`, like `PortValidation.hpp`: it names POSIX and
//! Winsock types, which rule "POSIX includes must not appear in public .hpp headers" forbids in
//! `include/`.

#include <cstdint>
#include <cstring>

#include "System/ArgumentException.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/AddressFamily.hpp"

#if defined(_WIN32)
#  include <winsock2.h>
#  include <ws2tcpip.h>
#elif !defined(__EMSCRIPTEN__)
#  include <sys/socket.h>
#  include <netinet/in.h>
#endif

#if !defined(__EMSCRIPTEN__)

namespace System::Net::Sockets::detail {

    /** @brief `AF_INET6` for an IPv6 address, `AF_INET` otherwise. */
    inline int NativeFamilyOf(const System::Net::IPAddress& address) noexcept {
        return address.getIsIPv6Property() ? AF_INET6 : AF_INET;
    }

    /** @brief `AF_INET6` for `InterNetworkV6`, `AF_INET` otherwise. */
    inline int NativeFamilyOf(System::Net::Sockets::AddressFamily family) noexcept {
        return family == System::Net::Sockets::AddressFamily::InterNetworkV6 ? AF_INET6 : AF_INET;
    }

    /**
     * @brief Fills @p storage from @p endPoint and returns the address length to pass to
     *        `bind`/`connect`.
     *
     * The IPv6 branch carries the **scope id**. Dropping it is not a cosmetic loss: a link-local
     * address (`fe80::/10`) is ambiguous without one, and `connect` fails with `EINVAL` rather
     * than reaching the wrong host -- so the field is written for the same reason the address is.
     */
    inline socklen_t BuildIPSockAddr(const System::Net::IPEndPoint& endPoint,
                                     sockaddr_storage&              storage) {
        std::memset(&storage, 0, sizeof(storage));
        const System::Net::IPAddress& address = endPoint.getAddressProperty();
        const auto port = static_cast<uint16_t>(endPoint.getPortProperty());

        if (address.getIsIPv6Property()) {
            auto* a6 = reinterpret_cast<sockaddr_in6*>(&storage);
            a6->sin6_family = AF_INET6;
            a6->sin6_port   = htons(port);
            const auto bytes = address.GetAddressBytes();
            std::memcpy(&a6->sin6_addr, bytes.data(), bytes.size());
            a6->sin6_scope_id = static_cast<uint32_t>(address.getScopeIdProperty());
            return static_cast<socklen_t>(sizeof(sockaddr_in6));
        }

        auto* a4 = reinterpret_cast<sockaddr_in*>(&storage);
        a4->sin_family      = AF_INET;
        a4->sin_port        = htons(port);
        a4->sin_addr.s_addr = htonl(address.getAddressProperty());
        return static_cast<socklen_t>(sizeof(sockaddr_in));
    }

    /**
     * @brief Reads an `IPEndPoint` back out of a native address of either family.
     *
     * @throws System::ArgumentException if @p storage carries a family this runtime's
     *         `IPAddress` cannot represent. Returning a **default-constructed** endpoint instead
     *         -- which is what a hand-written `sockaddr_in` cast silently did for an `AF_INET6`
     *         peer -- reports `0.0.0.0:0` for a real, connected peer, and that is the shape of
     *         defect this ticket exists to remove.
     */
    inline System::Net::IPEndPoint IPEndPointFromNative(const sockaddr_storage& storage) {
        if (storage.ss_family == AF_INET) {
            const auto* a4 = reinterpret_cast<const sockaddr_in*>(&storage);
            return System::Net::IPEndPoint(
                System::Net::IPAddress(static_cast<uint32_t>(ntohl(a4->sin_addr.s_addr))),
                static_cast<SharpRuntime::intcs>(ntohs(a4->sin_port)));
        }
        if (storage.ss_family == AF_INET6) {
            const auto* a6 = reinterpret_cast<const sockaddr_in6*>(&storage);
            std::array<SharpRuntime::bytecs, 16> bytes{};
            std::memcpy(bytes.data(), &a6->sin6_addr, 16);
            return System::Net::IPEndPoint(
                System::Net::IPAddress(bytes, static_cast<SharpRuntime::longcs>(a6->sin6_scope_id)),
                static_cast<SharpRuntime::intcs>(ntohs(a6->sin6_port)));
        }
        throw System::ArgumentException(
            "The socket reported an address family this runtime cannot represent as an IPEndPoint.",
            "storage");
    }

    /**
     * @brief .NET's `Socket.CanTryAddressFamily` (`Socket.cs:780-783`), transcribed.
     *
     * `(family == socketFamily) || (family == InterNetwork && socketIsDualMode)`. An IPv4
     * endpoint is usable on a dual-mode IPv6 socket; an IPv6 endpoint on an IPv4 socket never is,
     * in either direction of the asymmetry -- there is no mapping that way.
     */
    inline bool CanTryAddressFamily(System::Net::Sockets::AddressFamily family,
                                    System::Net::Sockets::AddressFamily socketFamily,
                                    bool socketIsDualMode) noexcept {
        return family == socketFamily
            || (family == System::Net::Sockets::AddressFamily::InterNetwork && socketIsDualMode);
    }

    /**
     * @brief `IPV6_V6ONLY == 0` on an already-created socket, i.e. .NET's `Socket.DualMode`.
     *
     * .NET does **not** force this option at construction -- it reads and writes it through the
     * `DualMode` property and otherwise leaves the OS default (`Socket.cs:745-770`), which on
     * Linux is `net.ipv6.bindv6only = 0`, i.e. dual mode. This port does the same, so the
     * question "is this socket dual mode?" is answered by asking the socket, never by assuming.
     */
    inline bool SocketIsDualMode(int fd, System::Net::Sockets::AddressFamily socketFamily) noexcept {
        if (socketFamily != System::Net::Sockets::AddressFamily::InterNetworkV6) return false;
#  if defined(IPV6_V6ONLY)
        int       v6only = 1;
        socklen_t len    = sizeof(v6only);
#    if defined(_WIN32)
        if (::getsockopt(static_cast<SOCKET>(fd), IPPROTO_IPV6, IPV6_V6ONLY,
                         reinterpret_cast<char*>(&v6only), &len) != 0)
            return false;
#    else
        if (::getsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, &len) != 0) return false;
#    endif
        return v6only == 0;
#  else
        (void)fd;
        return false;
#  endif
    }

    /**
     * @brief The endpoint to hand a socket of @p socketFamily, mapping IPv4 to `::ffff:a.b.c.d`
     *        when the socket is a dual-mode IPv6 one.
     *
     * Transcribed from `Socket.cs:1828` and `:1952`, which do exactly this before `Connect` and
     * `Bind`. Without the mapping, a `sockaddr_in` would be handed to an `AF_INET6` socket and
     * the kernel would reject the call -- correct behaviour reported as an unexplained
     * `EAFNOSUPPORT`.
     */
    inline System::Net::IPEndPoint AdaptEndPointForSocket(
        const System::Net::IPEndPoint&      endPoint,
        System::Net::Sockets::AddressFamily socketFamily) {
        if (socketFamily == System::Net::Sockets::AddressFamily::InterNetworkV6
            && !endPoint.getAddressProperty().getIsIPv6Property()) {
            return System::Net::IPEndPoint(endPoint.getAddressProperty().MapToIPv6(),
                                           endPoint.getPortProperty());
        }
        return endPoint;
    }

}  // namespace System::Net::Sockets::detail

#endif  // !__EMSCRIPTEN__
