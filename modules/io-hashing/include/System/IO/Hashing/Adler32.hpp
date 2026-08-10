// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <vector>
#include "System/IO/Hashing/NonCryptographicHashAlgorithm.hpp"

namespace System::IO::Hashing {

    using SharpRuntime::uintcs;

    /**
     * @brief Provides an implementation of the Adler-32 checksum algorithm, as specified in
     * RFC 1950.
     *
     * This algorithm produces a 32-bit checksum and is commonly used in data compression
     * formats such as zlib. It is not suitable for cryptographic purposes.
     *
     * C++ counterpart of .NET System.IO.Hashing.Adler32.
     *
     * @section rawptr The contract every raw-pointer overload in this component obeys
     *
     * .NET's surface is `ReadOnlySpan<byte>`/`Span<byte>`; this port replaced every span with a
     * raw pointer plus a signed @c intcs length, which makes two states representable that a
     * span cannot represent. All of them are stated here once rather than repeated per method:
     *
     * - **`length < 0`** throws `ArgumentOutOfRangeException("length", "Non-negative number
     *   required.")`. It is never treated as an empty buffer.
     * - **A null `source` with a positive `length`** throws `ArgumentNullException("source")`.
     * - **`length == 0` is accepted whatever `source` is, including `nullptr`** — `default(
     *   ReadOnlySpan<byte>)` is an empty span with a null reference, and hashing nothing is
     *   legal. A rejected `Append` leaves the accumulated hash unchanged.
     * - **A destination whose `destinationLength` is below the hash length** throws
     *   `ArgumentException("Destination is too short.", "destination")`, or returns `false` from
     *   the `Try…` forms. **The capacity claim is checked first**, so a null destination with an
     *   insufficient claimed length is reported as too short rather than as a null argument.
     * - **A null destination whose claimed capacity suffices** throws
     *   `ArgumentNullException("destination")`, from the `Try…` forms too. A rejected call leaves
     *   the destination buffer byte-for-byte unchanged.
     *
     * See `modules/io-hashing/README.md` for the same contract with the per-algorithm byte order.
     */
    class Adler32 final : public NonCryptographicHashAlgorithm {
    private:
        static constexpr intcs Size = 4;
        static constexpr uintcs InitialState = 1u;

        uintcs adler_ = InitialState;

        explicit Adler32(uintcs adler);

    protected:
        void GetCurrentHashCore(bytecs* destination) const override;
        void GetHashAndResetCore(bytecs* destination) override;

    public:
        /** Initializes a new instance of the Adler32 class. */
        Adler32();

        /** Returns a clone of the current instance, with a copy of the current instance's internal state. */
        [[nodiscard]] Adler32 Clone() const { return Adler32(adler_); }

        using NonCryptographicHashAlgorithm::Append;
        void Append(const bytecs* source, intcs length) override;
        void Reset() override;

        /** Gets the current computed hash value without modifying accumulated state. */
        [[nodiscard]] uintcs GetCurrentHashAsUInt32() const { return adler_; }

        /** Computes the Adler-32 hash of the provided data. */
        [[nodiscard]] static std::vector<bytecs> Hash(const bytecs* source, intcs length);
        /** Attempts to compute the Adler-32 hash of the provided data into @p destination. */
        static bool TryHash(const bytecs* source, intcs length, bytecs* destination, intcs destinationLength, intcs& bytesWritten);
        /** Computes the Adler-32 hash of the provided data into @p destination. */
        static intcs Hash(const bytecs* source, intcs length, bytecs* destination, intcs destinationLength);
        /** Computes the Adler-32 hash of the provided data. */
        [[nodiscard]] static uintcs HashToUInt32(const bytecs* source, intcs length);
    };

} // namespace System::IO::Hashing
