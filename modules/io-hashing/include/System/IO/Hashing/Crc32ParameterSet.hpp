// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO::Hashing {

    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;
    using SharpRuntime::uintcs;

    /**
     * @brief Represents a set of parameters that define the behavior of a CRC-32 hash algorithm.
     *
     * The parameter-set instance precomputes a 256-entry lookup table to be used in the CRC
     * calculation; callers are expected to create a single instance and reuse it.
     *
     * C++ counterpart of .NET System.IO.Hashing.Crc32ParameterSet. .NET splits the reflected
     * and forward (non-reflected) variants into separate internal class hierarchies to enable
     * per-variant intrinsics; this port keeps a single class with a runtime branch on
     * getReflectValuesProperty(), since that branching was purely an internal performance
     * detail, not part of the public contract.
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
    class Crc32ParameterSet {
    private:
        uintcs polynomial_;
        uintcs initialValue_;
        uintcs finalXorValue_;
        bool reflectValues_;
        std::vector<uintcs> lookupTable_;

        Crc32ParameterSet(uintcs polynomial, uintcs initialValue, uintcs finalXorValue, bool reflectValues);

    public:
        /** Gets the polynomial value used for the CRC calculation. */
        [[nodiscard]] uintcs getPolynomialProperty() const { return polynomial_; }
        /** Gets the initial value (seed) for the CRC calculation. */
        [[nodiscard]] uintcs getInitialValueProperty() const { return initialValue_; }
        /** Gets the value to XOR with the final CRC result. */
        [[nodiscard]] uintcs getFinalXorValueProperty() const { return finalXorValue_; }
        /**
         * @brief Gets whether the input and output bytes are most-significant-bit (MSB) first, or last.
         *
         * true if the MSB is the least significant bit of the last byte; false if the MSB is the
         * most significant bit of the first byte.
         */
        [[nodiscard]] bool getReflectValuesProperty() const { return reflectValues_; }

        /** Creates a new Crc32ParameterSet with the specified parameters. */
        [[nodiscard]] static std::shared_ptr<Crc32ParameterSet> Create(
            uintcs polynomial, uintcs initialValue, uintcs finalXorValue, bool reflectValues);

        /** Gets the parameter set for the variant of CRC-32 as used in ITU-T V.42 and IEEE 802.3. */
        [[nodiscard]] static const std::shared_ptr<Crc32ParameterSet>& getCrc32Property();
        /** Gets the parameter set for the CRC-32C variant of CRC-32. */
        [[nodiscard]] static const std::shared_ptr<Crc32ParameterSet>& getCrc32CProperty();

        /**
         * @brief Updates @p value with @p length bytes from @p source using this set's lookup table.
         *
         * @throws System::ArgumentOutOfRangeException if @p length is negative.
         * @throws System::ArgumentNullException if @p length is positive and @p source is null.
         */
        [[nodiscard]] uintcs Update(uintcs value, const bytecs* source, intcs length) const;

        /** Applies the final XOR to @p value. */
        [[nodiscard]] uintcs Finalize(uintcs value) const { return value ^ finalXorValue_; }

        /**
         * @brief Writes @p crc to @p destination (4 bytes), little-endian when this set reflects
         *        its values and big-endian when it does not.
         *
         * Unlike every other destination door in this component this one carries **no capacity
         * argument**: the size is implied by the algorithm, so a short buffer cannot be detected
         * and the caller must supply at least 4 bytes.
         *
         * @throws System::ArgumentNullException if @p destination is null.
         */
        void WriteCrcToSpan(uintcs crc, bytecs* destination) const;
    };

} // namespace System::IO::Hashing
