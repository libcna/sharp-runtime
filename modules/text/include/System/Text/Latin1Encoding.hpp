// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/Text/Encoding.hpp"
#include "System/Text/detail/RawDecodeRange.hpp"

namespace System::Text {

    /** ISO-8859-1 (Latin-1) encoding: each byte maps 1:1 to code points U+0000–U+00FF. */
    class Latin1Encoding : public Encoding {
    public:
        /** Returns the encoding name "iso-8859-1". */
        [[nodiscard]] std::string getEncodingNameProperty() const override { return "iso-8859-1"; }
        /** Returns the code page 28591 (ISO-8859-1). */
        [[nodiscard]] int getCodePageProperty() const override { return 28591; }
        /** Latin-1 always uses exactly one byte per character. */
        [[nodiscard]] bool getIsSingleByteProperty() const override { return true; }

        /** Encodes a string to Latin-1 bytes. */
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& s) const override {
            std::vector<SharpRuntime::bytecs> result;
            result.reserve(s.size());
            for (unsigned char c : s)
                result.push_back(static_cast<SharpRuntime::bytecs>(c));
            return result;
        }

        /**
         * Decodes a Latin-1 byte range to a string.
         *
         * @throws System::ArgumentOutOfRangeException if @p index or @p count is negative.
         * @throws System::ArgumentNullException if @p data is null and @p count is positive.
         * @note A raw pointer carries no length, so an @p index or @p count inside the signed
         *       domain but outside the caller's buffer cannot be detected here.
         */
        [[nodiscard]] std::string GetString(const SharpRuntime::bytecs* data,
                                             SharpRuntime::intcs index,
                                             SharpRuntime::intcs count) const override {
            // Ticket #2007 (SR-AUD-286): this override had no validation at all -- a null
            // buffer with a positive count was a segmentation fault, a negative count reached
            // `reserve(SIZE_MAX)` and threw std::length_error out of a System-shaped API, and
            // a negative index read before the buffer (measured: four guard bytes returned).
            const auto range = detail::checkedRawDecodeRange(data, index, count);
            std::string result;
            result.reserve(range.end - range.begin);
            for (std::size_t i = range.begin; i < range.end; ++i)
                result += static_cast<char>(static_cast<unsigned char>(data[i]));
            return result;
        }
    };

} // namespace System::Text
