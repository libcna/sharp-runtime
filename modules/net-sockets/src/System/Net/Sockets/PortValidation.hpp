// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Net::Sockets::detail {

    /**
     * @brief Rejects a port outside `[IPEndPoint::MinPort, IPEndPoint::MaxPort]`.
     *
     * `System::Net::IPEndPoint` has always validated its own port, so every endpoint-taking
     * overload in this module was already sound; the **bare-`int` overloads** were not. They
     * reached `htons(static_cast<uint16_t>(port))` directly, so `70000` silently became `4464`
     * and `-1` became `65535` — the caller's argument was truncated rather than refused
     * (SR-AUD-267, cause NS-C).
     *
     * Measured before the repair (`build-probe/2137_probe1_before.log`), the two `Connect(host,
     * port)` doors were worse than merely permissive: a **negative** port was rejected, but by
     * `getaddrinfo`, as *"DNS failed: Servname not supported for ai_socktype"* — a
     * `SocketException` blaming name resolution for an argument the caller controls — while a
     * port **above** 65535 was truncated and connected to the wrong port. One door lied about
     * which argument was wrong; the other did not complain at all.
     *
     * This is the module's single port rule, expressed against `IPEndPoint`'s own constants and
     * raising `IPEndPoint`'s own exception shape, so the bare-`int` doors and the endpoint doors
     * cannot drift apart again. RFC 793's 16-bit port field is the evidence that a value outside
     * `[0, 65535]` is not a port; no `/rv` reference is needed for it.
     *
     * @param port The caller's port argument.
     * @throws System::ArgumentOutOfRangeException with `paramName == "port"`.
     */
    inline void ValidatePort(SharpRuntime::intcs port) {
        System::ArgumentOutOfRangeException::ThrowIfLessThan(
            port, System::Net::IPEndPoint::MinPort, "port");
        System::ArgumentOutOfRangeException::ThrowIfGreaterThan(
            port, System::Net::IPEndPoint::MaxPort, "port");
    }

}  // namespace System::Net::Sockets::detail
