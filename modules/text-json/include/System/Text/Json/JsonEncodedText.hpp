// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/Text/Unicode/Utf16.hpp"
#include "System/Text/Unicode/Utf8.hpp"

namespace System::Text::Json {

    /**
     * @brief Represents a pre-encoded (validated) UTF-8/JSON text value that can be written to a
     * Utf8JsonWriter without re-validating or re-escaping it every time.
     *
     * C++ counterpart of .NET System.Text.Json.JsonEncodedText.
     *
     * @note No `JavaScriptEncoder` parameter — `System.Text.Encodings.Web` is out of scope in this
     * runtime (see JsonWriterOptions's doc comment); `Encode()` validates the input is well-formed
     * but does not apply any custom escaping.
     *
     * @note **Both `Encode` overloads apply the same validation.** Ticket #2113 (SR-AUD-329,
     * cause TJ-E, `docs/SystemTextJsonNamespaceReviewPlan.md` §6.4): the narrow overload used to
     * copy its argument verbatim and accept *every* malformed input class — `C3 28`, the
     * UTF-8-encoded lone surrogates `ED A0 80`/`ED B0 80`, the overlong `C0 AF`, the out-of-range
     * `F5 80 80 80`, a stray continuation byte, a truncated sequence and an embedded NUL — while
     * the UTF-16 overload rejected the one malformed class UTF-16 can express. The narrow overload
     * is now the single validator and the UTF-16 overload delegates to it, so the two cannot drift
     * apart again.
     *
     * @note **Why the control-character set stops where it does.** This type has no consumer inside
     * this module, so whether `Value` is a whole JSON *document* or one JSON *string body* is
     * undetermined — a byte may therefore only be rejected when it is illegal at **both**
     * granularities. Measured against this module's own parser
     * (`build-probe/2113_probe1_encodedtext.cpp`): `0x00`–`0x08`, `0x0B`, `0x0C` and `0x0E`–`0x1F`
     * are rejected both inside a string and in whitespace position, so `Encode` rejects them;
     * `0x09`, `0x0A` and `0x0D` are legal **as whitespace between tokens**, and `0x7F` is legal
     * **inside a string**, so `Encode` accepts all four rather than risk a false rejection. The
     * boundary is measured, not assumed — `/rv` is absent.
     *
     * @note **Known deviation, deliberately not repaired here:** `Value` is a public field, so
     * `JsonEncodedText t; t.Value = <anything>;` bypasses `Encode` entirely. .NET's counterpart is
     * a readonly struct whose only public producer is `Encode`. Closing that door is a **public
     * source break** and is out of scope for this layout-neutral ticket. (Aggregate initialization
     * is *not* a second bypass: the user-declared `= default` constructor disqualifies the struct
     * from being an aggregate under C++20's P1008R1.)
     */
    struct JsonEncodedText {
        std::string Value;

        JsonEncodedText() = default;

        /**
         * @brief Encodes @p value.
         * @throws System::ArgumentException if @p value isn't well-formed UTF-16, or if its UTF-8
         * transcoding would fail the same validation `Encode(const std::string&)` applies — the two
         * overloads agree on every input class by construction, because this one delegates to it.
         */
        static JsonEncodedText Encode(const std::u16string& value) {
            if (!System::Text::Unicode::Utf16::IsValid(value))
                throw System::ArgumentException("The input string contains invalid UTF-16 text.", "value");
            std::vector<SharpRuntime::bytecs> utf8(value.size() * 4);
            SharpRuntime::intcs charsRead = 0, bytesWritten = 0;
            System::Text::Unicode::Utf8::FromUtf16(value, utf8, charsRead, bytesWritten,
                                                    /*replaceInvalidSequences=*/false);
            return Encode(std::string(utf8.begin(), utf8.begin() + bytesWritten));
        }

        /**
         * @brief Validates @p value as well-formed JSON-writable UTF-8 text and wraps it as
         * pre-encoded text.
         *
         * This is the single validator both overloads run through (see the class doc comment).
         *
         * @throws System::ArgumentException if @p value is not well-formed UTF-8 (invalid,
         * overlong, out-of-range, truncated, a stray continuation byte, or a surrogate code point
         * encoded as UTF-8), or if it contains a raw control character that is illegal in JSON text
         * at every granularity (`0x00`–`0x08`, `0x0B`, `0x0C`, `0x0E`–`0x1F`). Tab, line feed,
         * carriage return and `0x7F` are accepted — see the class doc comment for the measurement
         * that fixes that boundary.
         */
        static JsonEncodedText Encode(const std::string& value) {
            const std::vector<SharpRuntime::bytecs> bytes(value.begin(), value.end());
            const SharpRuntime::intcs invalid = System::Text::Unicode::Utf8::IndexOfInvalidSubsequence(bytes);
            if (invalid >= 0)
                throw System::ArgumentException(
                    "The input string contains invalid UTF-8 text at index " + std::to_string(invalid) + ".",
                    "value");

            for (std::string::size_type i = 0; i < value.size(); ++i) {
                const auto b = static_cast<unsigned char>(value[i]);
                if (b < 0x20 && b != 0x09 && b != 0x0A && b != 0x0D) {
                    char code[8];
                    std::snprintf(code, sizeof(code), "U+%04X", b);
                    throw System::ArgumentException(
                        "The input string contains the unescaped control character " + std::string(code) +
                            " at index " + std::to_string(i) +
                            ", which is not valid in JSON text. Escape it as \\u" + (code + 2) + ".",
                        "value");
                }
            }

            JsonEncodedText result;
            result.Value = value;
            return result;
        }

        [[nodiscard]] bool Equals(const JsonEncodedText& other) const { return Value == other.Value; }
        bool operator==(const JsonEncodedText& other) const { return Equals(other); }
        bool operator!=(const JsonEncodedText& other) const { return !Equals(other); }

        [[nodiscard]] std::string ToString() const { return Value; }
    };

} // namespace System::Text::Json
