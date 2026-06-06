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
        Guid();
        explicit Guid(const std::array<uint8_t, 16>& bytes);
        explicit Guid(const std::string& guidString);

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

        [[nodiscard]] const std::array<uint8_t, 16>& ToByteArray() const { return bytes_; }

        bool operator==(const Guid& other) const { return bytes_ == other.bytes_; }
        bool operator!=(const Guid& other) const { return bytes_ != other.bytes_; }
        bool operator< (const Guid& other) const { return bytes_ <  other.bytes_; }
    };

} // namespace System
