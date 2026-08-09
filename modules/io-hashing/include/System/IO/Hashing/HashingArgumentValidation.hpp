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

} // namespace System::IO::Hashing::Detail
