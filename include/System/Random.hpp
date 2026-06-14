// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <limits>
#include <random>
#include <string>
#include <type_traits>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Span.hpp"

namespace System {

    using SharpRuntime::intcs;
    using SharpRuntime::bytecs;
    using SharpRuntime::longcs;

    /**
     * @brief Represents a pseudo-random number generator.
     *
     * C++ counterpart of .NET System.Random. Provides methods for generating
     * pseudo-random integers, floating-point values, byte sequences, and strings.
     * The default constructor seeds from a hardware source; the integer constructor
     * produces a deterministic sequence for the given seed.
     */
    class Random {
    private:
        std::mt19937 generator;

    public:
        /** @brief Initializes a new instance using a random seed from the hardware device. */
        Random();

        /**
         * @brief Initializes a new instance with the specified seed value.
         * @param seed A number used to calculate a starting value for the pseudo-random sequence.
         */
        explicit Random(intcs seed);

        /** @brief Random instances are not copyable. */
        Random(const Random&) = delete;
        /** @brief Random instances are not copyable. */
        Random& operator=(const Random&) = delete;

        /** @brief Move constructor. */
        Random(Random&&) noexcept = default;
        /** @brief Move-assignment operator. */
        Random& operator=(Random&&) noexcept = default;

        ~Random() = default;

        // -------------------------------------------------------------------------
        // Shared instance
        // -------------------------------------------------------------------------

        /**
         * @brief Gets a thread-safe shared Random instance usable from any call site.
         *
         * C++ counterpart of .NET Random.Shared. The returned reference is valid
         * for the lifetime of the process. The static instance is initialized
         * thread-safely on first access (C++11 guarantee).
         */
        static Random& getSharedProperty();

        // -------------------------------------------------------------------------
        // Int overloads
        // -------------------------------------------------------------------------

        /**
         * @brief Returns a non-negative random integer in [0, int.MaxValue).
         * @return A 32-bit signed integer ≥ 0 and < Int32.MaxValue.
         */
        intcs Next();

        /**
         * @brief Returns a non-negative random integer in [0, maxValue).
         * @param maxValue Exclusive upper bound; must be ≥ 0.
         * @throws System::ArgumentOutOfRangeException if maxValue < 0.
         */
        intcs Next(intcs maxValue);

        /**
         * @brief Returns a random integer in [minValue, maxValue).
         * @param minValue Inclusive lower bound.
         * @param maxValue Exclusive upper bound; must be ≥ minValue.
         * @throws System::ArgumentOutOfRangeException if minValue > maxValue.
         */
        intcs Next(intcs minValue, intcs maxValue);

        // -------------------------------------------------------------------------
        // Int64 overloads
        // -------------------------------------------------------------------------

        /**
         * @brief Returns a non-negative random 64-bit integer in [0, Int64.MaxValue).
         * @return A 64-bit signed integer ≥ 0 and < Int64.MaxValue.
         */
        longcs NextInt64();

        /**
         * @brief Returns a non-negative random 64-bit integer in [0, maxValue).
         * @param maxValue Exclusive upper bound; must be ≥ 0.
         * @throws System::ArgumentOutOfRangeException if maxValue < 0.
         */
        longcs NextInt64(longcs maxValue);

        /**
         * @brief Returns a random 64-bit integer in [minValue, maxValue).
         * @param minValue Inclusive lower bound.
         * @param maxValue Exclusive upper bound; must be ≥ minValue.
         * @throws System::ArgumentOutOfRangeException if minValue > maxValue.
         */
        longcs NextInt64(longcs minValue, longcs maxValue);

        // -------------------------------------------------------------------------
        // Generic integer overloads
        // -------------------------------------------------------------------------

        /**
         * @brief Returns a non-negative random integer of type T in [0, T::max].
         *
         * C++ counterpart of .NET Random.NextInteger<T>(). T must be an integral type.
         * Unlike Next(), the return range is inclusive of T::max.
         * @tparam T An integral type (int, long, uint32_t, etc.).
         */
        template<typename T>
        T NextInteger() {
            static_assert(std::is_integral_v<T>, "T must be an integral type");
            std::uniform_int_distribution<T> dist(T{0}, std::numeric_limits<T>::max());
            return dist(generator);
        }

        /**
         * @brief Returns a non-negative random integer of type T in [0, maxValue).
         * @param maxValue Exclusive upper bound; must be ≥ 0.
         * @throws System::ArgumentOutOfRangeException if maxValue < 0.
         */
        template<typename T>
        T NextInteger(T maxValue) {
            static_assert(std::is_integral_v<T>, "T must be an integral type");
            if (maxValue < T{0})
                throw ArgumentOutOfRangeException("maxValue must be >= 0");
            if (maxValue == T{0}) return T{0};
            std::uniform_int_distribution<T> dist(T{0}, maxValue - T{1});
            return dist(generator);
        }

        /**
         * @brief Returns a random integer of type T in [minValue, maxValue).
         * @param minValue Inclusive lower bound.
         * @param maxValue Exclusive upper bound; must be ≥ minValue.
         * @throws System::ArgumentOutOfRangeException if minValue > maxValue.
         */
        template<typename T>
        T NextInteger(T minValue, T maxValue) {
            static_assert(std::is_integral_v<T>, "T must be an integral type");
            if (minValue > maxValue)
                throw ArgumentOutOfRangeException("minValue must be <= maxValue");
            if (minValue == maxValue) return minValue;
            std::uniform_int_distribution<T> dist(minValue, maxValue - T{1});
            return dist(generator);
        }

        // -------------------------------------------------------------------------
        // Floating-point
        // -------------------------------------------------------------------------

        /**
         * @brief Returns a random single-precision float in [0.0f, 1.0f).
         * @return A float ≥ 0.0f and < 1.0f.
         */
        float NextSingle();

        /**
         * @brief Returns a random double-precision float in [0.0, 1.0).
         * @return A double ≥ 0.0 and < 1.0.
         */
        double NextDouble();

        // -------------------------------------------------------------------------
        // Byte buffers
        // -------------------------------------------------------------------------

        /**
         * @brief Fills a byte vector with random values.
         * @param buffer Vector to fill; its size determines the number of bytes generated.
         */
        void NextBytes(std::vector<bytecs>& buffer);

        /**
         * @brief Fills a Span<byte> with random values.
         *
         * C++ counterpart of .NET Random.NextBytes(Span<byte>).
         * @param buffer Span over mutable storage to fill with random bytes.
         */
        void NextBytes(Span<bytecs> buffer);

        // -------------------------------------------------------------------------
        // Collection utilities
        // -------------------------------------------------------------------------

        /**
         * @brief Performs an in-place Fisher-Yates shuffle of a vector.
         *
         * C++ counterpart of .NET Random.Shuffle<T>(T[]).
         * @tparam T Element type.
         * @param values The vector to shuffle in place.
         */
        template<typename T>
        void Shuffle(std::vector<T>& values) {
            for (intcs i = static_cast<intcs>(values.size()) - 1; i > 0; --i) {
                intcs j = Next(i + 1);
                std::swap(values[i], values[j]);
            }
        }

        /**
         * @brief Returns a vector of length @p length filled with elements chosen
         * randomly (with replacement) from @p choices.
         *
         * C++ counterpart of .NET Random.GetItems<T>(ReadOnlySpan<T>, int).
         * @tparam T Element type.
         * @param choices Non-empty source collection to sample from.
         * @param length  Number of elements to pick.
         * @return A new vector of size @p length.
         */
        template<typename T>
        std::vector<T> GetItems(const std::vector<T>& choices, intcs length) {
            std::vector<T> result;
            result.reserve(static_cast<std::size_t>(length));
            for (intcs i = 0; i < length; ++i)
                result.push_back(choices[static_cast<std::size_t>(
                    Next(static_cast<intcs>(choices.size())))]);
            return result;
        }

        // -------------------------------------------------------------------------
        // String utilities
        // -------------------------------------------------------------------------

        /**
         * @brief Returns a random string of @p length characters drawn from @p choices.
         *
         * C++ counterpart of .NET Random.GetString(ReadOnlySpan<char>, int).
         * @param choices Non-empty string of characters to sample from.
         * @param length  Number of characters in the returned string.
         * @return A new std::string of size @p length.
         */
        std::string GetString(const std::string& choices, intcs length);

        /**
         * @brief Returns a random hexadecimal string of the specified length.
         *
         * C++ counterpart of .NET Random.GetHexString(int, bool).
         * @param stringLength Number of hex characters to generate.
         * @param lowercase    If true, uses lowercase hex digits (a–f); otherwise uppercase (A–F).
         * @return A std::string of @p stringLength hex characters.
         */
        std::string GetHexString(intcs stringLength, bool lowercase = false);
    };

} // namespace System
