// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <vector>
#include "System/IO/Hashing/NonCryptographicHashAlgorithm.hpp"
#include "System/IO/Hashing/XxHash3Shared.hpp"

namespace System::IO::Hashing {

    using SharpRuntime::longcs;
    using SharpRuntime::ulongcs;

    /**
     * @brief A 128-bit hash value, represented as independent high/low 64-bit halves.
     *
     * C++ counterpart of the pair returned by .NET's XxHash128.GetCurrentHashAsUInt128()/
     * HashToUInt128() (System.UInt128); this codebase's System::UInt128 requires GCC/Clang's
     * unsigned __int128 (see System/UInt128.hpp), so XxHash128 exposes a portable, dependency-free
     * Hash128 struct instead, usable on every platform this runtime targets (including MSVC).
     */
    struct Hash128 {
        /** The low 64 bits of the hash. */
        ulongcs Low64 = 0;
        /** The high 64 bits of the hash. */
        ulongcs High64 = 0;

        Hash128() = default;
        Hash128(ulongcs low64, ulongcs high64) : Low64(low64), High64(high64) {}

        bool operator==(const Hash128& other) const { return Low64 == other.Low64 && High64 == other.High64; }
        bool operator!=(const Hash128& other) const { return !(*this == other); }
    };

    /**
     * @brief Provides an implementation of the XXH128 hash algorithm for generating a 128-bit hash.
     *
     * For methods that persist the computed numerical hash value as bytes, the value is written
     * in big-endian byte order (high 64 bits first, then low 64 bits).
     *
     * C++ counterpart of .NET System.IO.Hashing.XxHash128. Based on the XXH128 implementation
     * from https://github.com/Cyan4973/xxHash.
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
    class XxHash128 final : public NonCryptographicHashAlgorithm {
    private:
        static constexpr intcs Size = 16;

        Detail::XxHash3Shared::State state_;

        explicit XxHash128(const Detail::XxHash3Shared::State& state);

    protected:
        void GetCurrentHashCore(bytecs* destination) const override;

    public:
        /** Initializes a new instance of the XxHash128 class using the default seed value 0. */
        XxHash128();
        /** Initializes a new instance of the XxHash128 class using the specified seed. */
        explicit XxHash128(longcs seed);

        /** Returns a clone of the current instance, with a copy of the current instance's internal state. */
        [[nodiscard]] XxHash128 Clone() const { return XxHash128(state_); }

        void Reset() override;
        using NonCryptographicHashAlgorithm::Append;
        void Append(const bytecs* source, intcs length) override;

        /** Gets the current computed hash value without modifying accumulated state. */
        [[nodiscard]] Hash128 GetCurrentHashAsHash128() const;

        /** Computes the XXH128 hash of the provided data, using the default seed of 0. */
        [[nodiscard]] static std::vector<bytecs> Hash(const bytecs* source, intcs length);
        /** Computes the XXH128 hash of the provided data using the provided seed. */
        [[nodiscard]] static std::vector<bytecs> Hash(const bytecs* source, intcs length, longcs seed);
        /** Computes the XXH128 hash of the provided data into @p destination using the optionally provided seed. */
        static intcs Hash(const bytecs* source, intcs length, bytecs* destination, intcs destinationLength, longcs seed = 0);
        /** Attempts to compute the XXH128 hash of the provided data into @p destination using the optionally provided seed. */
        static bool TryHash(const bytecs* source, intcs length, bytecs* destination, intcs destinationLength,
                             intcs& bytesWritten, longcs seed = 0);
        /** Computes the XXH128 hash of the provided data using the optionally provided seed. */
        [[nodiscard]] static Hash128 HashToHash128(const bytecs* source, intcs length, longcs seed = 0);
    };

} // namespace System::IO::Hashing
