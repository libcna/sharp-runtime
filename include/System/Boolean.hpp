// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <string>

namespace System {

    /**
     * @brief Represents the Boolean (true/false) value type and its string equivalents.
     *
     * C++ counterpart of .NET System.Boolean.
     * All members are static; the class cannot be instantiated.
     */
    class Boolean {
    public:
        Boolean() = delete;

        /** @brief The string representation of the Boolean true value ("True"). */
        static constexpr const char* TrueString  = "True";

        /** @brief The string representation of the Boolean false value ("False"). */
        static constexpr const char* FalseString = "False";

        /**
         * @brief Parses a string as a Boolean value.
         *
         * C++ counterpart of .NET Boolean.Parse(string).
         * Accepts "True"/"true" and "False"/"false".
         * @throws std::invalid_argument if the string is not recognised.
         */
        [[nodiscard]] static bool Parse(const std::string& s) {
            if (s == "True"  || s == "true")  return true;
            if (s == "False" || s == "false") return false;
            throw std::invalid_argument("String must be 'True' or 'False'.");
        }

        /**
         * @brief Attempts to parse a string as a Boolean value.
         *
         * C++ counterpart of .NET Boolean.TryParse(string, out bool).
         * @return true if @p s was successfully parsed; false otherwise.
         */
        static bool TryParse(const std::string& s, bool& result) {
            if (s == "True"  || s == "true")  { result = true;  return true; }
            if (s == "False" || s == "false") { result = false; return true; }
            result = false; return false;
        }

        /**
         * @brief Converts a Boolean value to its string representation.
         *
         * C++ counterpart of .NET Boolean.ToString().
         * @return "True" if @p value is true; "False" otherwise.
         */
        [[nodiscard]] static std::string ToString(bool value) {
            return value ? "True" : "False";
        }

        /**
         * @brief Compares two Boolean values.
         *
         * C++ counterpart of .NET Boolean.CompareTo(bool).
         * @return Negative if a < b, zero if equal, positive if a > b.
         */
        [[nodiscard]] static int CompareTo(bool a, bool b) noexcept {
            return static_cast<int>(a) - static_cast<int>(b);
        }

        /**
         * @brief Returns true if the two Boolean values are equal.
         *
         * C++ counterpart of .NET Boolean.Equals(bool).
         */
        [[nodiscard]] static bool Equals(bool a, bool b) noexcept { return a == b; }

        /**
         * @brief Returns a hash code for the specified Boolean value.
         *
         * C++ counterpart of .NET Boolean.GetHashCode().
         */
        [[nodiscard]] static int GetHashCode(bool value) noexcept {
            return value ? 1 : 0;
        }
    };

} // namespace System
