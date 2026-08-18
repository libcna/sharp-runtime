// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Text/UTF8Encoding.hpp"
#include "System/Text/detail/RawDecodeRange.hpp"
#include "System/detail/Utf8Scalar.hpp"
#include <cstdint>

namespace System::Text {

namespace {

    // Returns the byte length (1-4) of a well-formed UTF-8 sequence starting at data[i] within
    // [i, end), or 0 if the byte at i does not begin (or complete) a well-formed sequence.
    // UTF8Encoding's well-formed bytes pass through unchanged (source and destination are both
    // UTF-8), so there is no re-encoding step to fold a substitution into -- which is why this
    // door wants a validity LENGTH rather than a code point.
    //
    // Ticket #2354 (2026-08-18): this was the copy #2014 could not move, because it reads a
    // (pointer, end) range rather than a std::string. That is now a parameter of the shared
    // decode rather than a reason to hold a sixth copy of the rule.
    size_t wellFormedUtf8Length(const SharpRuntime::bytecs* data, size_t i, size_t end) {
        std::uint32_t cp  = 0;
        std::size_t   len = 0;
        return System::detail::TryDecodeUtf8Scalar(reinterpret_cast<const char*>(data), end, i,
                                                   cp, len)
                   ? len
                   : 0;
    }

} // namespace

    UTF8Encoding::UTF8Encoding() {
        // Real UTF8Encoding.SetDefaultFallbacks() uses a U+FFFD replacement fallback, not the
        // generic Encoding base class's "?" default (which only fits single-byte code pages).
        setEncoderFallbackProperty(std::make_shared<EncoderReplacementFallback>("\xEF\xBF\xBD"));
        setDecoderFallbackProperty(std::make_shared<DecoderReplacementFallback>("\xEF\xBF\xBD"));
    }

    std::vector<SharpRuntime::bytecs> UTF8Encoding::GetBytes(const std::string& str) const {
        const auto* data = reinterpret_cast<const SharpRuntime::bytecs*>(str.data());
        size_t end = str.size();
        std::vector<SharpRuntime::bytecs> result;
        result.reserve(end);
        auto fallback = getEncoderFallbackProperty();
        size_t i = 0;
        while (i < end) {
            size_t len = wellFormedUtf8Length(data, i, end);
            if (len > 0) {
                result.insert(result.end(), data + i, data + i + len);
                i += len;
            } else {
                auto buffer = fallback->CreateFallbackBuffer();
                if (buffer->Fallback(static_cast<char>(data[i]), static_cast<SharpRuntime::intcs>(i))) {
                    while (buffer->getRemainingProperty() > 0) {
                        result.push_back(static_cast<SharpRuntime::bytecs>(buffer->GetNextChar()));
                    }
                }
                ++i;
            }
        }
        return result;
    }

    std::string UTF8Encoding::GetString(const SharpRuntime::bytecs* data,
                                        SharpRuntime::intcs index,
                                        SharpRuntime::intcs count) const {
        // Ticket #2007 (SR-AUD-286): a negative index used to reach `static_cast<size_t>`
        // below. For (-1, 1) the derived `end` wrapped to 0 and the loop did not run -- which
        // is why the finding's named reproduction does NOT reproduce here (plan §4.1) -- but
        // for count < |index| it did, and read wild memory.
        const auto range = detail::checkedRawDecodeRange(data, index, count);
        if (!range.any()) return {};
        size_t i = range.begin;
        size_t end = range.end;
        std::string result;
        result.reserve(end - i);
        auto fallback = getDecoderFallbackProperty();
        while (i < end) {
            size_t len = wellFormedUtf8Length(data, i, end);
            if (len > 0) {
                result.append(reinterpret_cast<const char*>(data + i), len);
                i += len;
            } else {
                std::vector<SharpRuntime::bytecs> bad{data[i]};
                auto buffer = fallback->CreateFallbackBuffer();
                if (buffer->Fallback(bad, static_cast<SharpRuntime::intcs>(i))) {
                    while (buffer->getRemainingProperty() > 0) {
                        result.push_back(buffer->GetNextChar());
                    }
                }
                ++i;
            }
        }
        return result;
    }

} // namespace System::Text
