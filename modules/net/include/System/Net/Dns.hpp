// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPHostEntry.hpp"
#include "System/Net/Sockets/AddressFamily.hpp"

namespace System::Net {

    /**
     * @brief Provides simple domain name resolution functionality.
     *
     * Partial C++ counterpart of .NET System.Net.Dns.
     *
     * @note Status: Partial. Only the synchronous, non-obsolete surface is ported
     * (GetHostName, GetHostAddresses, GetHostEntry). The async Task-returning overloads,
     * the IAsyncResult-based Begin/End methods, and the obsolete GetHostByName/Resolve/
     * GetHostByAddress legacy methods are not ported, since this runtime's Task
     * infrastructure doesn't route real async I/O (see NEXT.md, System.Threading.Tasks
     * limitations), so those overloads would just be synchronous work wrapped in a Task
     * for no benefit.
     * @note This note previously stated that resolution was "restricted to IPv4", that the
     * hints "always request AF_INET regardless of the @p family argument", and that
     * AddressFamily::InterNetworkV6 "returns an empty result". None of that has been true for
     * some time and all three were measured false; the accurate contract is below (ticket
     * #2039).
     * @note Both IPv4 and IPv6 are resolved. AddressFamily::InterNetwork requests AF_INET,
     * InterNetworkV6 requests AF_INET6, and every other value -- including Unspecified --
     * requests AF_UNSPEC, so both families are returned.
     * @note Text that parses as an IP literal is answered directly, without contacting the
     * resolver, using System::Net::IPAddress::TryParse -- the same parser IPAddress::Parse
     * uses, so the two can never disagree about what a literal means. A literal whose family
     * does not match a requested family other than Unspecified raises
     * System::Net::Sockets::SocketException (HostNotFound) rather than returning an empty
     * result, so "no address of that family" is distinguishable from "checked and found
     * nothing".
     * @note Addresses are returned in the resolver's own order with exact duplicates removed.
     * "Duplicate" means binary IPAddress equality -- family, address bits and, for IPv6, the
     * scope id. No canonicalisation is performed: 1.2.3.4 and ::ffff:1.2.3.4 are distinct, and
     * so are fe80::1%7 and fe80::1%9.
     * @note A resolver failure raises SocketException with SocketError::HostNotFound and a
     * message carrying the platform resolver's own description of the failure (gai_strerror on
     * POSIX, the WSA code on Windows).
     * @note POSIX (getaddrinfo/getnameinfo/gethostname) and Windows (Winsock2) are both
     * implemented; Emscripten throws PlatformNotSupportedException.
     */
    class Dns {
    public:
        Dns() = delete;

        /**
         * @return The host name of the local machine.
         * @throws System::Net::Sockets::SocketException if the platform call fails.
         */
        static std::string GetHostName();

        /**
         * Resolves a host name or IP address string to its addresses, of any family.
         * Equivalent to the two-argument overload with AddressFamily::Unspecified.
         * @throws System::Net::Sockets::SocketException if resolution fails.
         */
        static std::vector<IPAddress> GetHostAddresses(const std::string& hostNameOrAddress);

        /**
         * Resolves a host name or IP address string to addresses of the given family, in the
         * resolver's order, with exact duplicates removed.
         * @throws System::Net::Sockets::SocketException if resolution fails, or if
         *         @p hostNameOrAddress is an IP literal whose family is not @p family and
         *         @p family is not AddressFamily::Unspecified.
         */
        static std::vector<IPAddress> GetHostAddresses(const std::string& hostNameOrAddress, System::Net::Sockets::AddressFamily family);

        /**
         * Resolves a host name or IP address string to an IPHostEntry.
         * @throws System::Net::Sockets::SocketException if resolution fails.
         */
        static IPHostEntry GetHostEntry(const std::string& hostNameOrAddress);

        /**
         * Resolves a host name or IP address string to an IPHostEntry, restricted to the given
         * family. The address list carries the resolver's order with exact duplicates removed.
         * @throws System::Net::Sockets::SocketException if resolution fails, or if
         *         @p hostNameOrAddress is an IP literal whose family is not @p family and
         *         @p family is not AddressFamily::Unspecified.
         */
        static IPHostEntry GetHostEntry(const std::string& hostNameOrAddress, System::Net::Sockets::AddressFamily family);

        /** Resolves an IPAddress to an IPHostEntry via reverse then forward DNS lookup. */
        static IPHostEntry GetHostEntry(const IPAddress& address);
    };

} // namespace System::Net
