// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <array>
#include <string>
#include <cstdint>

namespace System {

    /**
     * @brief Represents a globally unique identifier (GUID).
     *
     * Partial C++ counterpart of .NET System.Guid.
     *
     * @note Status: Partial
     */
    class Guid {
    private:
        std::array<uint8_t, 16> bytes_;

    public:
        /// Initializes a new Guid with all bytes set to zero.
        Guid();
        /// Initializes a new Guid from a 16-byte array.
        explicit Guid(const std::array<uint8_t, 16>& bytes);
        /// Parses a Guid from its canonical string representation.
        explicit Guid(const std::string& guidString);

        /// Represents a Guid whose value is all zeros.
        static const Guid Empty;

        /**
         * @brief Creates a new Guid with a random value (version 4).
         */
        [[nodiscard]] static Guid NewGuid();

        /**
         * @brief Returns the string representation in the form
         * "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx".
         */
        [[nodiscard]] std::string ToString() const;

        /// Returns the underlying 16-byte array of this Guid.
        [[nodiscard]] const std::array<uint8_t, 16>& ToByteArray() const { return bytes_; }

        /// Returns true if this Guid is equal to the specified Guid.
        bool operator==(const Guid& other) const { return bytes_ == other.bytes_; }
        /// Returns true if this Guid is not equal to the specified Guid.
        bool operator!=(const Guid& other) const { return bytes_ != other.bytes_; }
        /// Provides an ordering for use in sorted containers.
        bool operator< (const Guid& other) const { return bytes_ <  other.bytes_; }
    };

} // namespace System
