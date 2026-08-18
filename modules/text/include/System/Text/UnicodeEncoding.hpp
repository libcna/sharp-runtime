// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Text/detail/FallbackDispatch.hpp"
#include <memory>
#include <cstdint>
#include <string>
#include <vector>
#include "System/Text/Encoding.hpp"
#include "System/Text/detail/RawDecodeRange.hpp"
#include "System/Text/detail/Utf8Scalar.hpp"

namespace System::Text {

    /**
     * @brief Encoding that uses UTF-16 (matches .NET's UnicodeEncoding — little-endian by
     * default, or big-endian).
     *
     * This runtime represents `System::String`/`std::string` as UTF-8 internally, so GetBytes()
     * decodes the source UTF-8 sequence into Unicode scalar values and re-encodes each as one or
     * two UTF-16 code units (surrogate pairs for U+10000 and above); GetString() reverses this.
     * Unlike an earlier, simpler version of this file, this handles the full Unicode range, not
     * just ASCII.
     *
     * C++ counterpart of .NET System.Text.UnicodeEncoding.
     */
    class UnicodeEncoding : public Encoding {
        bool bigEndian_;
        bool byteOrderMark_;

    public:
        /** Constructs a UTF-16 LE encoding with no byte-order mark. */
        UnicodeEncoding() : bigEndian_(false), byteOrderMark_(false) { setDefaultFallbacks(); }
        /** Constructs a UTF-16 encoding with the specified endianness and BOM setting. */
        UnicodeEncoding(bool bigEndian, bool byteOrderMark)
            : bigEndian_(bigEndian), byteOrderMark_(byteOrderMark) { setDefaultFallbacks(); }

        /** Returns the encoding name ("utf-16" or "utf-16BE"). */
        [[nodiscard]] std::string getEncodingNameProperty() const override { return bigEndian_ ? "utf-16BE" : "utf-16"; }
        /** Returns the code page (1201 for big-endian, 1200 for little-endian). */
        [[nodiscard]] int getCodePageProperty() const override { return bigEndian_ ? 1201 : 1200; }

        /** @return true if this instance encodes as big-endian UTF-16. */
        [[nodiscard]] bool getIsBigEndianProperty() const { return bigEndian_; }
        /** @return true if GetBytes() prepends a byte-order mark. */
        [[nodiscard]] bool getByteOrderMarkProperty() const { return byteOrderMark_; }

        /**
         * @brief Returns the byte-order mark this encoding declares, or an empty vector.
         *
         * Ticket #2016 (SR-AUD-291). Until it, `GetBytes` PREPENDED the mark to its output, so
         * `Encoding::UTF32()->GetBytes("A")` returned eight bytes for one character and the mark
         * travelled as payload -- it was concatenated into strings, counted in lengths and
         * written a second time by anything that also wrote a preamble.
         *
         * .NET keeps the two apart: the mark is what `GetPreamble()` returns (`UnicodeEncoding.cs`), and
         * `GetBytes` never emits it. A caller that wants a BOM writes the preamble itself, which
         * is what `StreamWriter` does.
         *
         * @note **Not virtual, and not on `Encoding`.** .NET declares `GetPreamble` on the base,
         *       but adding a virtual to a public base class is precisely what
         *       `docs/StandingApprovals.md` SA-3 excludes from standing approval, so it lives on
         *       the concrete encodings that have a mark. A caller holding only an `Encoding&`
         *       does not need it: an encoding with no mark has nothing to write.
         */
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetPreamble() const {
            if (!byteOrderMark_) return {};
            return bigEndian_ ? std::vector<SharpRuntime::bytecs>{0xFE, 0xFF}
                              : std::vector<SharpRuntime::bytecs>{0xFF, 0xFE};
        }

        /**
         * @brief Encodes a string (decoded from UTF-8) to UTF-16 bytes, in the configured endianness.
         * @note Pre-sizes the output buffer to its exact worst case (2 output bytes per input
         * UTF-8 byte -- every decode path here emits at most 2 output bytes per byte consumed,
         * so `s.size() * 2 + 2` is a provable upper bound) and writes through indexing rather
         * than `reserve()` + `push_back()`. GCC 14's `-Wfree-nonheap-object`/`-Warray-bounds`
         * (both fatal under `-Werror` in a Release build) misanalyze the inlined
         * reserve-then-push_back reallocation path here as a real bug even though the
         * reallocation branch is unreachable in practice; indexed writes into a pre-sized
         * buffer sidestep that code shape entirely.
         */
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& s) const override {
            std::vector<SharpRuntime::bytecs> out(s.size() * 2 + 2);
            size_t pos = 0;
            auto writeUnit = [&](uint16_t unit) {
                if (bigEndian_) {
                    out[pos++] = static_cast<SharpRuntime::bytecs>((unit >> 8) & 0xFF);
                    out[pos++] = static_cast<SharpRuntime::bytecs>(unit & 0xFF);
                } else {
                    out[pos++] = static_cast<SharpRuntime::bytecs>(unit & 0xFF);
                    out[pos++] = static_cast<SharpRuntime::bytecs>((unit >> 8) & 0xFF);
                }
            };
            size_t i = 0;
            while (i < s.size()) {
                uint32_t cp;
                size_t len;
                decodeUtf8(s, i, cp, len);
                i += len;
                if (cp < 0x10000) {
                    writeUnit(static_cast<uint16_t>(cp));
                } else {
                    uint32_t v = cp - 0x10000;
                    writeUnit(static_cast<uint16_t>(0xD800 + (v >> 10)));
                    writeUnit(static_cast<uint16_t>(0xDC00 + (v & 0x3FF)));
                }
            }
            out.resize(pos);
            return out;
        }

        /**
         * @brief Decodes a UTF-16 byte range (in the configured endianness) to a UTF-8-encoded string.
         *
         * An unpaired surrogate (a high surrogate not followed by a matching low surrogate, or
         * a lone low surrogate) is replaced with U+FFFD, matching .NET's default
         * DecoderFallback behavior. Without this check, `cp` would previously stay set to the
         * raw surrogate value (0xD800-0xDFFF) and reach encodeUtf8() unvalidated -- real UTF-8
         * forbids encoding a surrogate value directly (that's CESU-8/WTF-8, not UTF-8), so this
         * produced ill-formed output bytes rather than a genuine encoding error.
         *
         * @throws System::ArgumentOutOfRangeException if @p index or @p count is negative.
         * @throws System::ArgumentNullException if @p data is null and @p count is positive.
         * @note A raw pointer carries no length, so an @p index or @p count inside the signed
         *       domain but outside the caller's buffer cannot be detected here.
         */
        [[nodiscard]] std::string GetString(const SharpRuntime::bytecs* data,
                                            SharpRuntime::intcs index,
                                            SharpRuntime::intcs count) const override {
            // Ticket #2007 (SR-AUD-286): this override had no validation, and its own
            // `intcs end = index + count` was signed overflow -- GetString(p, INTCS_MAX, 4)
            // was measured as a segmentation fault. The shared range is unsigned, so the
            // addition cannot overflow.
            const auto range = detail::checkedRawDecodeRange(data, index, count);
            std::string out;
            std::size_t i = range.begin;
            const std::size_t end = range.end;

            if (end - i >= 2) {
                uint16_t bom = readUnit(data, i);
                if (bom == 0xFEFF) i += 2;
            }

            // #2017 (SR-AUD-292/293): every undecodable unit goes through the CONFIGURED
            // decoder fallback. Before it this substituted U+FFFD directly, so a caller who
            // installed DecoderFallback::ExceptionFallback() got silence where .NET throws --
            // a policy that was accepted, stored, and then ignored. The default fallback for
            // this encoding is the U+FFFD replacement (matching UnicodeEncoding.cs:56-68), so
            // the default output is byte-identical to before.
            while (i + 1 < end) {
                const std::size_t unitStart = i;
                uint16_t unit = readUnit(data, i);
                i += 2;
                uint32_t cp = unit;
                bool undecodable = false;
                std::size_t badLength = 2;
                if (unit >= 0xD800 && unit <= 0xDBFF) {
                    if (i + 1 < end) {
                        uint16_t low = readUnit(data, i);
                        if (low >= 0xDC00 && low <= 0xDFFF) {
                            cp = 0x10000 + ((static_cast<uint32_t>(unit - 0xD800) << 10) | (low - 0xDC00));
                            i += 2;
                        } else {
                            undecodable = true;   // unpaired high surrogate
                        }
                    } else {
                        undecodable = true;       // unpaired high surrogate at end of input
                    }
                } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
                    undecodable = true;           // lone low surrogate
                }
                if (undecodable) {
                    detail::AppendDecoderFallback(out, *this, data + unitStart, badLength);
                } else {
                    encodeUtf8(cp, out);
                }
            }

            // #2017's second half: a TRUNCATED trailing unit used to be discarded outright, so
            // UTF16LE.GetString(3 bytes) returned one character and the odd byte vanished with
            // no diagnostic of any kind. It is undecodable input like any other and now reaches
            // the fallback, which is what makes an exception fallback able to report it.
            if (i < end) {
                detail::AppendDecoderFallback(out, *this, data + i, end - i);
            }
            return out;
        }

    private:
        /**
         * .NET's UnicodeEncoding.SetDefaultFallbacks (`UnicodeEncoding.cs:56-68`) installs a
         * U+FFFD replacement fallback rather than the base's "?", with the comment "For UTF-X
         * encodings, we use a replacement fallback". #2017 needs that default here for the same
         * reason .NET has it: once the decode routes through the configured fallback, the
         * DEFAULT fallback is what decides the default output, and U+FFFD is what this encoding
         * substituted before.
         */
        void setDefaultFallbacks() {
            setEncoderFallbackProperty(std::make_shared<EncoderReplacementFallback>("\xEF\xBF\xBD"));
            setDecoderFallbackProperty(std::make_shared<DecoderReplacementFallback>("\xEF\xBF\xBD"));
        }

        [[nodiscard]] uint16_t readUnit(const SharpRuntime::bytecs* data, std::size_t i) const {
            uint8_t b0 = data[i];
            uint8_t b1 = data[i + 1];
            return bigEndian_ ? static_cast<uint16_t>((b0 << 8) | b1) : static_cast<uint16_t>((b1 << 8) | b0);
        }

        // Ticket #2354 (2026-08-18): this was a header-inline COPY of the one UTF-8 scalar
        // decode. #2014 factored the rule into System/detail/Utf8Scalar.hpp and moved the two
        // .cpp copies onto it; the three header-inline ones were left because they are
        // entangled with their types' own loops. They are not entangled -- they are the same
        // rule with a different wrapper, which is exactly why five copies accumulated. What is
        // preserved is the CONTRACT this door needs: an ill-formed sequence becomes U+FFFD over
        // a single byte, matching .NET's default DecoderFallback.
        static void decodeUtf8(const std::string& s, size_t i, uint32_t& codePoint, size_t& length) {
            System::detail::DecodeUtf8Scalar(s, i, codePoint, length);
        }

        static void encodeUtf8(uint32_t cp, std::string& out) {
            System::detail::AppendUtf8Scalar(out, cp);
        }
    };

} // namespace System::Text
