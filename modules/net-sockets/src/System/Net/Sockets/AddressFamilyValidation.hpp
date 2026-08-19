// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/ArgumentException.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/AddressFamily.hpp"

namespace System::Net::Sockets::detail {

    /** @brief `"InterNetwork"` / `"InterNetworkV6"`, for .NET's exception text. */
    inline const char* AddressFamilyName(System::Net::Sockets::AddressFamily family) noexcept {
        switch (family) {
            case System::Net::Sockets::AddressFamily::InterNetwork:   return "InterNetwork";
            case System::Net::Sockets::AddressFamily::InterNetworkV6: return "InterNetworkV6";
            case System::Net::Sockets::AddressFamily::Unspecified:    return "Unspecified";
            case System::Net::Sockets::AddressFamily::Unix:           return "Unix";
            default:                                                  return "Unknown";
        }
    }

    /**
     * @brief Refuses an endpoint whose family the socket that would carry it does not accept.
     *
     * @par What this replaced, and why the replacement is narrower rather than merely different
     * Ticket **#2138** put an unconditional *"this module is IPv4"* refusal on all six endpoint
     * doors, because every socket in `TcpClient`, `TcpListener` and `UdpClient` was `AF_INET`.
     * #2363 removed that limitation, so an unconditional refusal would now be a lie -- an IPv6
     * endpoint is carried, not refused.
     *
     * What survives is the check .NET actually has, in the place .NET has it: a socket **that
     * already exists** has a family, and an endpoint of a different family cannot be used with
     * it. `Socket.cs:1757-1759` raises
     * `ArgumentException(SR.Format(SR.net_InvalidEndPointAddressFamily, given, mine), paramName)`,
     * and `Strings.resx:156-158` gives the text verbatim: *"The supplied EndPoint of AddressFamily
     * {0} is not valid for this Socket, use {1} instead."* The sentence is unchanged from #2138;
     * only the **condition** narrowed, and both families are now filled in rather than one being
     * a literal.
     *
     * A door that *creates* the socket from the endpoint it was given -- both `TcpListener`
     * constructors, `TcpClient(localEP)`, `UdpClient(localEP)`, and an unbound
     * `TcpClient::Connect(remoteEP)` -- has nothing to disagree with and calls this not at all.
     * That is the whole of the difference between this and #2138.
     *
     * @param family        The endpoint's family.
     * @param socketFamily  The family of the socket that would carry it.
     * @param socketIsDualMode Whether that socket has `IPV6_V6ONLY` clear, i.e. .NET's `DualMode`.
     * @param parameterName The parameter the endpoint arrived in.
     * @throws System::ArgumentException when the two are incompatible.
     */
    inline void ValidateEndPointFamilyForSocket(System::Net::Sockets::AddressFamily family,
                                                System::Net::Sockets::AddressFamily socketFamily,
                                                bool                                socketIsDualMode,
                                                const char*                         parameterName) {
        if (family == socketFamily
            || (family == System::Net::Sockets::AddressFamily::InterNetwork && socketIsDualMode))
            return;
        throw System::ArgumentException(
            std::string("The supplied EndPoint of AddressFamily ") + AddressFamilyName(family)
                + " is not valid for this Socket, use " + AddressFamilyName(socketFamily)
                + " instead.",
            parameterName);
    }

}  // namespace System::Net::Sockets::detail
