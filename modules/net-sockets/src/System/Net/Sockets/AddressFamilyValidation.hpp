// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/ArgumentException.hpp"
#include "System/Net/Sockets/AddressFamily.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"

namespace System::Net::Sockets::detail {

    /**
     * @brief Rejects an endpoint whose address family this module's `AF_INET` sockets cannot
     *        carry, at the door the caller used.
     *
     * @par Why this exists when the limitation was already loud
     * `TcpClient`, `TcpListener` and `UdpClient` create `AF_INET` sockets and fill `sockaddr_in`
     * throughout. Measured (`build-probe/2138_probe1_ipv6doors.cpp`,
     * `build-probe/2138_probe2_listener.cpp`), **nothing was ever silently narrowed** — the audit
     * finding's "silently misrepresented" wording does not reproduce, and SR-AUD-266's family
     * half is a *diagnostic* defect rather than a correctness one. Every path already threw.
     *
     * What it threw was the problem. **Three different refusals, none of them the operation's
     * own:**
     *
     *  1. the endpoint doors reached `IPAddress::getAddressProperty()`, which raised
     *     `SocketException(OperationNotSupported)` — *"The requested property is not supported
     *     for the 'InterNetworkV6' AddressFamily."* A caller who asked to *connect* was told
     *     about an **unsupported property**, and the sentence names no operation, no argument
     *     and no remedy;
     *  2. `TcpListener` deferred that same accident to `Start()`, because its constructors only
     *     store the endpoint — so the two listener doors reported at a different time from the
     *     four others;
     *  3. the two **hostname** doors refuse for a third, unrelated reason: `hints.ai_family` is
     *     `AF_INET`, so `getaddrinfo` never resolves an IPv6 literal and the caller is told
     *     *"DNS failed"* about a literal address that needs no DNS.
     *
     * Refusals 1 and 2 are replaced by this deliberate one. Refusal 3 is deliberately left alone
     * — see the note below.
     *
     * @par The exception shape is .NET's, transcribed
     * `Socket.cs:1759` and its five siblings raise
     * `ArgumentException(SR.Format(SR.net_InvalidEndPointAddressFamily, given, mine))` with the
     * offending parameter's name, and `Strings.resx:156-158` gives the text verbatim:
     * *"The supplied EndPoint of AddressFamily {0} is not valid for this Socket, use {1}
     * instead."* Nothing here is invented: this port simply states truthfully which family its
     * sockets are.
     *
     * @par What is NOT rejected here, and why
     * The two `Connect(hostname, port)` doors keep their `getaddrinfo` refusal. Deciding whether
     * `"::1"` arriving at a *hostname* parameter is a literal or a name is the question ticket
     * **#2359** holds for `System::Uri`, and answering it incidentally inside a socket door would
     * settle it in the wrong place. Their behaviour is pinned unchanged.
     *
     * Full dual-stack — `AF_UNSPEC` with a `getaddrinfo` result walk, as .NET does — is ticket
     * **#2363**. This function is what makes the gap honest until then, not a substitute for it.
     *
     * @param address    The address the caller supplied.
     * @param parameterName The name of the parameter it arrived in.
     * @throws System::ArgumentException if @p address is not `InterNetwork`.
     */
    inline void ValidateIPv4Address(const System::Net::IPAddress& address,
                                    const char* parameterName) {
        if (address.getAddressFamilyProperty() == System::Net::Sockets::AddressFamily::InterNetwork)
            return;
        // The family name is a LITERAL, not a lookup, and that is a measured decision rather
        // than a shortcut. `IPAddress::getAddressFamilyProperty()` is
        // `isIPv6_ ? InterNetworkV6 : InterNetwork` over a single `bool`
        // (`IPAddress.cpp:326-328`), so an `IPAddress` in this port CANNOT carry a third family.
        // A general name table here would be unreachable code, and a first cut that wrote one
        // was deleted rather than defended -- the same reason mutation M4 of this ticket
        // (`== InterNetwork` rewritten as `!= InterNetworkV6`) is not a mutation at all but
        // equivalent code, which the ticket record states plainly instead of claiming a catch.
        throw System::ArgumentException(
            "The supplied EndPoint of AddressFamily InterNetworkV6 is not valid for this Socket, "
            "use InterNetwork instead.",
            parameterName);
    }

    /** @brief `ValidateIPv4Address` for an endpoint. See its doc-comment for the whole rationale. */
    inline void ValidateIPv4EndPoint(const System::Net::IPEndPoint& endPoint,
                                     const char* parameterName) {
        ValidateIPv4Address(endPoint.getAddressProperty(), parameterName);
    }

}  // namespace System::Net::Sockets::detail
