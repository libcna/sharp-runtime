// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>

namespace System {

    class IFormatProvider;

    /**
     * @brief Defines a mechanism for parsing a string to a value of the implementing type.
     *
     * C++ counterpart of .NET System.IParsable<TSelf>.
     * Implement Parse() and TryParse() to provide string-parsing capability.
     *
     * @tparam TSelf The implementing type (CRTP pattern).
     */
    template<typename TSelf>
    class IParsable {
    public:
        /** @brief Virtual destructor for safe polymorphic destruction. */
        virtual ~IParsable() = default;
        /**
         * @brief Parses a string into a value of type @p TSelf.
         * @param s      The string to parse.
         * @param provider Optional format provider (may be nullptr).
         * @return Parsed value.
         * @throws System::FormatException if the string cannot be parsed.
         */
        virtual TSelf Parse(const std::string& s, const IFormatProvider* provider) = 0;
        /**
         * @brief Tries to parse a string into a value of type @p TSelf.
         * @param s       The string to parse.
         * @param provider Optional format provider (may be nullptr).
         * @param result  Receives the parsed value on success.
         * @return true if parsing succeeded; false otherwise.
         */
        virtual bool TryParse(const std::string& s, const IFormatProvider* provider, TSelf& result) = 0;
    };

} // namespace System
