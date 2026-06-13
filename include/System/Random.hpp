// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 5/28/25.
//

#pragma once

#include <random>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;
    using SharpRuntime::bytecs;
    using SharpRuntime::longcs;

    /**
     * @brief Represents a pseudo-random number generator.
     *
     * This is a simplified C++ counterpart of the .NET System::Random class.
     * It provides methods for generating pseudo-random 32-bit signed integers.
     * @note Status: DONE
     */
    class Random {
    private:
        std::mt19937 generator;

    public:
        /// @brief Initializes a new instance using a random seed from the hardware device.
        Random();

        /// @brief Initializes a new instance with the specified @p seed value.
        explicit Random(intcs seed);

        /// Deleted copy constructor — Random instances are not copyable.
        Random(const Random&) = delete;
        /// Deleted copy-assignment — Random instances are not copyable.
        Random& operator=(const Random&) = delete;

        /// Move constructor.
        Random(Random&&) noexcept = default;
        /// Move-assignment operator.
        Random& operator=(Random&&) noexcept = default;

        /// Destructor.
        ~Random() = default;

        /**
         * @brief Returns a non-negative random integer.
         *
         * @return A 32-bit signed integer greater than or equal to 0
         *         and less than INTCS_MAX.
         */
        intcs Next();

        /**
         * @brief Returns a non-negative random integer less than the specified maximum.
         *
         * @param maxValue The exclusive upper bound of the random number to be generated.
         *                 Must be greater than or equal to 0.
         * @return A 32-bit signed integer greater than or equal to 0 and less than maxValue.
         *         If maxValue is 0, returns 0.
         */
        intcs Next(intcs maxValue);

        /**
         * @brief Returns a random integer within a specified range.
         *
         * @param minValue The inclusive lower bound of the random number returned.
         * @param maxValue The exclusive upper bound of the random number returned.
         *                 Must be greater than or equal to minValue.
         * @return A 32-bit signed integer greater than or equal to minValue
         *         and less than maxValue.
         *         If minValue equals maxValue, returns minValue.
         */
        intcs Next(intcs minValue, intcs maxValue);

        /**
         * @brief Returns a random floating-point number in [0.0, 1.0).
         *
         * @return A double greater than or equal to 0.0 and less than 1.0.
         */
        double NextDouble();

        /**
         * @brief Returns a random single-precision float in [0.0f, 1.0f).
         *
         * @return A float greater than or equal to 0.0f and less than 1.0f.
         */
        float NextSingle();

        /// @brief Returns a non-negative random 64-bit integer in [0, Int64.MaxValue).
        longcs NextInt64();

        /// @brief Returns a non-negative random 64-bit integer in [0, maxValue).
        longcs NextInt64(longcs maxValue);

        /// @brief Returns a random 64-bit integer in [minValue, maxValue).
        longcs NextInt64(longcs minValue, longcs maxValue);

        /**
         * @brief Fills the elements of a byte vector with random values.
         *
         * @param buffer Vector whose elements are to be filled with random bytes.
         */
        void NextBytes(std::vector<bytecs>& buffer);
    };

} // namespace System
