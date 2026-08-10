// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO::Hashing::Detail {

    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;

    /**
     * @brief The one place `System::IO::Hashing` states the contract its raw-pointer surface needs.
     *
     * .NET's `System.IO.Hashing` surface is expressed in `ReadOnlySpan<byte>`/`Span<byte>`. This
     * port replaced every span with a raw pointer plus a signed `intcs` length, and that
     * substitution makes two states representable that a span cannot represent:
     *
     * - a **negative length**, which a `for (intcs i = 0; i < length; ++i)` loop silently treats
     *   as "empty" — so the caller receives the hash of nothing and cannot tell; and
     * - a **null pointer with a positive length**, which every loop and `memcpy` dereferences.
     *
     * Four of the module's seven hasher types already rejected a negative length with
     * `ArgumentOutOfRangeException("length", "Non-negative number required.")`. These helpers hold
     * that single rule so the remaining types adopt it verbatim rather than re-inventing it.
     *
     * Not part of the public API surface of the module's types; declared in a public header only
     * because every implementation file in the component needs it.
     */

    /**
     * @brief Throws `ArgumentOutOfRangeException("length", "Non-negative number required.")`.
     */
    [[noreturn]] void ThrowNegativeLength();

    /** @brief Throws `ArgumentNullException("source")`. */
    [[noreturn]] void ThrowNullSource();

    /** @brief Throws `ArgumentNullException("destination")`. */
    [[noreturn]] void ThrowNullDestination();

    /** @brief Throws `ArgumentException("Destination is too short.", "destination")`. */
    [[noreturn]] void ThrowDestinationTooShort();

    /**
     * @brief Rejects a negative buffer length.
     *
     * A length of zero is accepted: `default(ReadOnlySpan<byte>)` in .NET is an empty span, and
     * hashing nothing is a legal, well-defined operation on every algorithm in this module.
     *
     * @throws System::ArgumentOutOfRangeException if @p length is negative.
     */
    inline void ValidateLength(intcs length) {
        if (length < 0) {
            ThrowNegativeLength();
        }
    }

    /**
     * @brief Rejects an input buffer that cannot be read for @p length bytes.
     *
     * A **null pointer with a length of zero is accepted**, and deliberately so:
     * `default(ReadOnlySpan<byte>)` in .NET is an empty span whose reference is null, and
     * appending it is legal. Only a null pointer with a *positive* length is an error — which is
     * the state a span cannot represent and this port's raw pointer can.
     *
     * @throws System::ArgumentOutOfRangeException if @p length is negative.
     * @throws System::ArgumentNullException if @p length is positive and @p source is null.
     */
    inline void ValidateSource(const bytecs* source, intcs length) {
        ValidateLength(length);
        if (length > 0 && source == nullptr) {
            ThrowNullSource();
        }
    }

    /**
     * @brief Rejects an output buffer that cannot hold @p requiredLength bytes.
     *
     * The capacity claim is checked **first**, so a caller who passes a short buffer still gets
     * the module's long-standing *"Destination is too short."* diagnostic regardless of the
     * pointer. Only a buffer whose *claimed capacity is sufficient* and whose pointer is null is
     * reported as a null argument — that combination is exactly the one that used to be written
     * through.
     *
     * @throws System::ArgumentException if @p destinationLength is below @p requiredLength.
     * @throws System::ArgumentNullException if the capacity suffices but @p destination is null.
     */
    inline void ValidateDestination(const bytecs* destination, intcs destinationLength,
                                    intcs requiredLength) {
        if (destinationLength < requiredLength) {
            ThrowDestinationTooShort();
        }
        if (destination == nullptr) {
            ThrowNullDestination();
        }
    }

    /**
     * @brief The `Try…` form of ValidateDestination: insufficient capacity is a `false` return.
     *
     * A null pointer is **not** a capacity condition and is still thrown, because a caller that
     * gets `false` would otherwise be told "the buffer is too small" about a buffer that does
     * not exist.
     *
     * @return false if @p destinationLength is below @p requiredLength.
     * @throws System::ArgumentNullException if the capacity suffices but @p destination is null.
     */
    [[nodiscard]] inline bool TryValidateDestination(const bytecs* destination,
                                                     intcs destinationLength, intcs requiredLength) {
        if (destinationLength < requiredLength) {
            return false;
        }
        if (destination == nullptr) {
            ThrowNullDestination();
        }
        return true;
    }

    /**
     * @brief Rejects a null output buffer at a door that carries no capacity argument.
     *
     * `Crc32ParameterSet::WriteCrcToSpan` and `Crc64ParameterSet::WriteCrcToSpan` are public and
     * take a bare destination pointer whose size is implied by the algorithm, so a capacity check
     * is not available to them — a null check is all there is.
     *
     * @throws System::ArgumentNullException if @p destination is null.
     */
    inline void ValidateNonNullDestination(const bytecs* destination) {
        if (destination == nullptr) {
            ThrowNullDestination();
        }
    }

} // namespace System::IO::Hashing::Detail
