// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include "System/Text/detail/FallbackDispatch.hpp"
#include "System/Text/detail/Utf8Scalar.hpp"
#include <memory>
#include <string>
#include <vector>
#include "System/Text/Encoding.hpp"
#include "System/Text/detail/RawDecodeRange.hpp"

namespace System::Text {

    /**
     * ISO-8859-1 (Latin-1) encoding.
     *
     * @warning **This class does not currently implement the mapping its name and code page
     *          (28591) advertise.** It copies the runtime's UTF-8 *storage* bytes straight
     *          through in both directions instead of converting Unicode scalar values, so
     *          (measured, `build-probe/2006_probe1_before.log` §D):
     *          - `GetBytes(u8"é")` produces `c3 a9`, where ISO-8859-1 is the single byte
     *            `e9`; and
     *          - `GetString({0xE9})` produces the single byte `e9`, which is not well-formed
     *            UTF-8, where the correct answer is `c3 a9`.
     *
     *          Only the ASCII range round-trips correctly. It is the one encoding in this
     *          component that does **not** decode the UTF-8 storage into scalars first —
     *          `ASCIIEncoding`, `UnicodeEncoding` and `UTF32Encoding` all do. Ticket #2012
     *          (SR-AUD-289) states the divergence; repairing it changes the produced bytes
     *          for every non-ASCII input and is the approval-gated ticket **#2014**
     *          (`docs/SystemTextNamespaceReviewPlan.md` §14.2).
     */
    class Latin1Encoding : public Encoding {
    public:
        /** Returns the encoding name "iso-8859-1". */
        [[nodiscard]] std::string getEncodingNameProperty() const override { return "iso-8859-1"; }
        /** Returns the code page 28591 (ISO-8859-1). */
        [[nodiscard]] int getCodePageProperty() const override { return 28591; }
        /** Latin-1 always uses exactly one byte per character. */
        [[nodiscard]] bool getIsSingleByteProperty() const override { return true; }

        /**
         * @brief Encodes @p s to ISO-8859-1 bytes: one byte per Unicode scalar, not per UTF-8
         * storage byte.
         *
         * Ticket #2014 (SR-AUD-289). Before it, this copied the UTF-8 storage bytes through
         * unchanged, so `GetBytes(u8"é")` returned `c3 a9` where ISO-8859-1 is the single byte
         * `e9`. `Latin1Encoding` was the only encoding in the component that did not decode the
         * storage representation into scalars first -- `ASCIIEncoding`, `UnicodeEncoding` and
         * `UTF32Encoding` all do -- so the repair is the shape those three already had.
         *
         * A scalar outside U+0000..U+00FF cannot be represented in ISO-8859-1 and becomes `?`,
         * which is what `Latin1Encoding` does in .NET (its default encoder fallback is the
         * replacement fallback with `"?"`) and what this component's `ASCIIEncoding` already
         * does for the same reason. A supplementary-plane scalar produces **two** `?`, matching
         * the two UTF-16 code units .NET would encode it from -- again as `ASCIIEncoding` does.
         */
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& s) const override {
            std::vector<SharpRuntime::bytecs> result;
            result.reserve(s.size());
            std::size_t i = 0;
            while (i < s.size()) {
                std::uint32_t cp = 0;
                std::size_t len = 0;
                detail::DecodeUtf8Scalar(s, i, cp, len);
                i += len;
                if (cp <= 0xFF) {
                    result.push_back(static_cast<SharpRuntime::bytecs>(cp));
                } else if (cp < 0x10000) {
                    // #2017: the CONFIGURED encoder fallback. The default is the replacement
                    // fallback with "?", so the observable result is unchanged unless a caller
                    // asked for something else.
                    detail::AppendEncoderFallback(result, *this, cp);
                } else {
                    detail::AppendEncoderFallback(result, *this, cp);
                    detail::AppendEncoderFallback(result, *this, cp);
                }
            }
            return result;
        }

        /**
         * @brief Decodes ISO-8859-1 bytes: every byte is the scalar of the same value, re-encoded
         * into this runtime's UTF-8 `System::String` representation.
         *
         * Ticket #2014. Before it, the bytes were copied through as if they were already UTF-8,
         * so `GetString({0xE9})` returned the ill-formed single byte `e9` where the UTF-8 answer
         * is `c3 a9`. ISO-8859-1 is the one encoding where the byte value **is** the scalar
         * value, so this needs no table -- and that is also why the previous behaviour looked
         * plausible while being wrong at exactly the bytes that matter.
         *
         * Unlike the encode direction this **cannot fail**: every one of the 256 byte values is
         * a valid ISO-8859-1 character, so no fallback is reachable here.
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
            for (std::size_t i = range.begin; i < range.end; ++i) {
                detail::AppendUtf8Scalar(result, static_cast<std::uint32_t>(
                                                     static_cast<unsigned char>(data[i])));
            }
            return result;
        }
    };

} // namespace System::Text
