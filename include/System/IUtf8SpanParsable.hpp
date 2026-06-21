// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>
#include "System/Span.hpp"

namespace System {

    /**
     * @brief Defines a mechanism for parsing a span of UTF-8 characters to a value.
     *
     * C++ counterpart of .NET System.IUtf8SpanParsable<TSelf> (stub).
     * In .NET this interface requires @c static abstract members, which C++ does
     * not support on base classes. The interface is represented as a CRTP-style
     * abstract base with virtual methods so that concrete types can be tested
     * polymorphically.
     *
     * @tparam TSelf The concrete type that implements this interface.
     *
     * @note Status: Stub — static abstract members cannot be expressed in C++.
     */
    template<typename TSelf>
    class IUtf8SpanParsable {
    public:
        /** @brief Virtual destructor for safe polymorphic destruction. */
        virtual ~IUtf8SpanParsable() = default;

        /**
         * @brief Parses a span of UTF-8 bytes into a value.
         *
         * C++ counterpart of .NET IUtf8SpanParsable<TSelf>.Parse(ReadOnlySpan<byte>, IFormatProvider?).
         * @param utf8Text A view of UTF-8 encoded characters to parse.
         * @return The parsed value.
         * @throws std::invalid_argument if @p utf8Text is not in the correct format.
         */
        [[nodiscard]] virtual TSelf ParseUtf8(const Span<uint8_t>& utf8Text) const = 0;

        /**
         * @brief Tries to parse a span of UTF-8 bytes into a value.
         *
         * C++ counterpart of .NET IUtf8SpanParsable<TSelf>.TryParse(ReadOnlySpan<byte>, IFormatProvider?, out TSelf).
         * @param utf8Text A view of UTF-8 encoded characters to parse.
         * @param result   On success, the parsed value; on failure, a default value.
         * @return true if parsing succeeded; false otherwise.
         */
        virtual bool TryParseUtf8(const Span<uint8_t>& utf8Text, TSelf& result) const noexcept = 0;
    };

} // namespace System
