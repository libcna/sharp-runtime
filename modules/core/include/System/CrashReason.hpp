// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

/**
 * @brief Specifies the reason why a process crashed or failed fast.
 *
 * @warning **Project-specific public diagnostic type — not a .NET public API.**
 * .NET publishes no `System.CrashReason`. Its counterpart there is the `internal`,
 * nested `System.CrashInfo.CrashReason`, which only NativeAOT uses to encode triage
 * data. This enumeration reproduces that enumeration's constants and their numeric
 * values so that ported diagnostic code has a name for each, but it is published on
 * this project's own authority: nothing in .NET promises this name, this top-level
 * placement or the stability of these values, and a change to .NET's internal enum
 * is not by itself a defect in this header. Do not read it as a parity surface.
 *
 * It has **no first-party production consumer**. No crash or fail-fast path in this
 * repository maps a native failure reason onto it, so the constants are a vocabulary
 * for porting rather than a runtime contract, and nothing here writes them into a
 * triage record. It remains published rather than becoming an implementation detail
 * because `System/CrashReason.hpp` is already an independently includable public
 * header in the `Core.Base` include tree, and withdrawing it would be a source break
 * for consumers this repository is not permitted to inspect.
 *
 * Like every C++ scoped enumeration this one is not closed over its underlying type:
 * a cast can produce a value matching no enumerator, and no member of this API
 * rejects one.
 *
 * SR-AUD-127, tickets #2279 (review) and #2280.
 */
enum class CrashReason {
    /** @brief The crash reason is unknown. */
    Unknown = 0,
    /** @brief The process crashed due to an unhandled exception. */
    UnhandledException = 1,
    /** @brief The process called Environment.FailFast. */
    EnvironmentFailFast = 2,
    /** @brief The runtime itself triggered a fail-fast due to an internal error. */
    InternalFailFast = 3,
};

} // namespace System
