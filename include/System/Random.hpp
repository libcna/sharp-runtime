// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 5/28/25.
//

#pragma once

#include <random>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;

    /**
     * @brief Represents a pseudo-random number generator.
     *
     * This is a simplified C++ counterpart of the .NET System::Random class.
     * It provides methods for generating pseudo-random 32-bit signed integers.
     * @note Status: Partial
     */
    class Random {
    private:
        std::mt19937 generator;

    public:
        /**
         * @brief Initializes a new instance of the Random class using a random seed.
         */
        Random();

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
    };

} // namespace System
