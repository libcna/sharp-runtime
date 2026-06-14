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

        /// @brief Parses a Guid from its string representation. Throws FormatException on invalid input.
        [[nodiscard]] static Guid Parse(const std::string& s);

        /// @brief Tries to parse a Guid; returns false without throwing if the format is invalid.
        static bool TryParse(const std::string& s, Guid& result);

        /**
         * @brief Returns the string representation in the form
         * "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx".
         */
        [[nodiscard]] std::string ToString() const;

        /**
         * @brief Returns the string representation in the specified format.
         *
         * Supported formats: "D" (default, with hyphens), "N" (no hyphens),
         * "B" ({braces}), "P" ((parentheses)).
         */
        [[nodiscard]] std::string ToString(const std::string& format) const;

        /// Returns the underlying 16-byte array of this Guid.
        [[nodiscard]] const std::array<uint8_t, 16>& ToByteArray() const { return bytes_; }

        /// Returns true if this Guid is equal to the specified Guid.
        [[nodiscard]] bool Equals(const Guid& other) const { return bytes_ == other.bytes_; }

        /**
         * @brief Compares this Guid to another.
         * @return negative if less, 0 if equal, positive if greater.
         */
        [[nodiscard]] int CompareTo(const Guid& other) const;

        bool operator==(const Guid& other) const { return bytes_ == other.bytes_; }
        bool operator!=(const Guid& other) const { return bytes_ != other.bytes_; }
        bool operator< (const Guid& other) const { return bytes_ <  other.bytes_; }
        bool operator<=(const Guid& other) const { return bytes_ <= other.bytes_; }
        bool operator> (const Guid& other) const { return bytes_ >  other.bytes_; }
        bool operator>=(const Guid& other) const { return bytes_ >= other.bytes_; }
    };

} // namespace System
