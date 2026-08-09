// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO::Compression::Detail {

    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;

    /**
     * @brief The one place `System::IO::Compression` states the contract its raw-pointer surface
     * needs before a length reaches zlib.
     *
     * .NET's encoder/decoder surface is expressed in `ReadOnlySpan<byte>`/`Span<byte>`. This port
     * replaced every span with a raw pointer plus a signed `intcs` length, and every body then
     * handed that length to zlib as an **unsigned** `uInt`:
     *
     * @code
     * zs.avail_in  = static_cast<uInt>(sourceLength);      // -1 becomes 4,294,967,295
     * zs.avail_out = static_cast<uInt>(destinationLength);
     * @endcode
     *
     * A negative length therefore became an enormous byte count and zlib read or wrote far past
     * the caller's allocation. AddressSanitizer reported a 65,536-byte READ past a one-byte source
     * and a WRITE past a one-byte destination (`build-probe/2146_probe1_before.log`, 15 crashes in
     * 63 cases). This module is the one whose ordinary job is parsing **attacker-supplied** bytes,
     * so the state a span cannot represent must be rejected before zlib sees it.
     *
     * The rule, the exception types and the messages are deliberately **identical** to
     * `System::IO::Hashing::Detail` (ticket #2141/#2142, SR-AUD-260/261), which repaired the same
     * idiom in the sibling module. Two components solving one problem two ways is how the
     * repository ends up with two answers to "what does a negative length mean".
     *
     * Not part of the public API surface of the module's types; declared in a public header only
     * because every implementation file in the component needs it.
     */

    /** @brief Throws `ArgumentOutOfRangeException(paramName, "Non-negative number required.")`. */
    [[noreturn]] void ThrowNegativeLength(const char* paramName);

    /** @brief Throws `ArgumentNullException(paramName)`. */
    [[noreturn]] void ThrowNullBuffer(const char* paramName);

    /**
     * @brief Rejects a source buffer that cannot be read for @p length bytes.
     *
     * A **null pointer with a length of zero is accepted**, deliberately: `default(ReadOnlySpan<byte>)`
     * in .NET is an empty span whose reference is null, and compressing or decompressing nothing is
     * legal. Only a null pointer with a *positive* length is an error.
     *
     * @throws System::ArgumentOutOfRangeException if @p length is negative.
     * @throws System::ArgumentNullException if @p length is positive and @p source is null.
     */
    inline void ValidateSource(const bytecs* source, intcs length) {
        if (length < 0) {
            ThrowNegativeLength("sourceLength");
        }
        if (length > 0 && source == nullptr) {
            ThrowNullBuffer("source");
        }
    }

    /**
     * @brief Rejects a destination buffer that cannot be written for @p length bytes.
     *
     * Symmetrical with ValidateSource, and separate from it so the thrown `paramName` names the
     * argument the caller actually got wrong. A zero-length destination is legal — every codec in
     * this module reports `DestinationTooSmall` for it rather than failing.
     *
     * @throws System::ArgumentOutOfRangeException if @p length is negative.
     * @throws System::ArgumentNullException if @p length is positive and @p destination is null.
     */
    inline void ValidateDestination(const bytecs* destination, intcs length) {
        if (length < 0) {
            ThrowNegativeLength("destinationLength");
        }
        if (length > 0 && destination == nullptr) {
            ThrowNullBuffer("destination");
        }
    }

} // namespace System::IO::Compression::Detail
