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
     * @brief UTF-32 encoding; defaults to little-endian with a byte-order mark.
     *
     * This runtime represents `System::String`/`std::string` as UTF-8 internally, so GetBytes()
     * decodes the source UTF-8 sequence into Unicode scalar values and re-encodes each as one
     * 4-byte UTF-32 code unit; GetString() reverses this — the full Unicode range is supported,
     * not just ASCII.
     *
     * C++ counterpart of .NET System.Text.UTF32Encoding.
     */
    class UTF32Encoding : public Encoding {
        bool bigEndian_;
        bool byteOrderMark_;

    public:
        /** Constructs a UTF-32 LE encoding with a byte-order mark. */
        UTF32Encoding() : bigEndian_(false), byteOrderMark_(true) { setDefaultFallbacks(); }
        /** Constructs a UTF-32 encoding with the specified endianness and BOM setting. */
        UTF32Encoding(bool bigEndian, bool byteOrderMark)
            : bigEndian_(bigEndian), byteOrderMark_(byteOrderMark) { setDefaultFallbacks(); }

        /** Returns the encoding name "utf-32". */
        [[nodiscard]] std::string getEncodingNameProperty() const override { return "utf-32"; }
        /** Returns the code page (12001 for big-endian, 12000 for little-endian). */
        [[nodiscard]] int getCodePageProperty() const override { return bigEndian_ ? 12001 : 12000; }

        /** @return true if this instance encodes as big-endian UTF-32. */
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
         * .NET keeps the two apart: the mark is what `GetPreamble()` returns (`UTF32Encoding.cs:1113-1128`), and
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
            return bigEndian_
                       ? std::vector<SharpRuntime::bytecs>{0x00, 0x00, 0xFE, 0xFF}
                       : std::vector<SharpRuntime::bytecs>{0xFF, 0xFE, 0x00, 0x00};
        }

        /**
         * @brief Encodes a string (decoded from UTF-8) to a UTF-32 byte sequence.
         * @note Pre-sizes the output buffer to its exact worst case (every decoded code point
         * emits exactly 4 bytes, and there are at most `s.size()` code points, so
         * `s.size() * 4 + 4` is a provable upper bound) and writes through indexing rather than
         * `reserve()` + `push_back()`. GCC 14's `-Wfree-nonheap-object`/`-Warray-bounds` (both
         * fatal under `-Werror` in a Release build) misanalyze the inlined
         * reserve-then-push_back reallocation path here as a real bug even though the
         * reallocation branch is unreachable in practice; indexed writes into a pre-sized
         * buffer sidestep that code shape entirely.
         */
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& s) const override {
            std::vector<SharpRuntime::bytecs> result(s.size() * 4 + 4);
            size_t pos = 0;
            size_t i = 0;
            while (i < s.size()) {
                uint32_t cp;
                size_t len;
                decodeUtf8(s, i, cp, len);
                i += len;
                writeUnit(result, pos, cp);
            }
            result.resize(pos);
            return result;
        }

        /**
         * @brief Decodes a UTF-32 byte range to a UTF-8-encoded string.
         *
         * A 4-byte unit that isn't a valid Unicode scalar value (> U+10FFFF, or a surrogate
         * value U+D800-U+DFFF) is replaced with U+FFFD, matching .NET's default
         * DecoderFallback behavior. Without this check, an arbitrary/corrupt 32-bit value
         * (e.g. 0xFFFFFFFF) would previously reach encodeUtf8() unvalidated and produce a
         * byte sequence that isn't even structurally valid UTF-8, not just the wrong code
         * point.
         *
         * @throws System::ArgumentOutOfRangeException if @p index or @p count is negative.
         * @throws System::ArgumentNullException if @p data is null and @p count is positive.
         * @note A raw pointer carries no length, so an @p index or @p count inside the signed
         *       domain but outside the caller's buffer cannot be detected here.
         */
        [[nodiscard]] std::string GetString(const SharpRuntime::bytecs* data,
                                             SharpRuntime::intcs index,
                                             SharpRuntime::intcs count) const override {
            // Ticket #2007 (SR-AUD-286): this override had no validation -- a null buffer
            // with a positive count was a segmentation fault, and a negative index read
            // before the caller's buffer.
            const auto range = detail::checkedRawDecodeRange(data, index, count);
            std::string result;
            std::size_t start = range.begin;
            std::size_t end = range.end;
            if (end - start >= 4) {
                uint32_t maybeBom = readUnit(data, start);
                if (maybeBom == 0x0000FEFF) {
                    start += 4;
                }
            }
            // #2017 (SR-AUD-292/293): every undecodable unit goes through the CONFIGURED
            // decoder fallback rather than being substituted with U+FFFD directly. The default
            // fallback for this encoding is the U+FFFD replacement (UTF32Encoding.cs:65-76), so
            // the default output is byte-identical to before -- what changes is that a caller
            // who CONFIGURED something else finally gets it.
            std::size_t i = start;
            for (; i + 3 < end; i += 4) {
                uint32_t cp = readUnit(data, i);
                if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
                    detail::AppendDecoderFallback(result, *this, data + i, 4);
                } else {
                    encodeUtf8(cp, result);
                }
            }
            // #2017's second half: a truncated trailing unit used to be discarded outright --
            // UTF32LE.GetString(6 bytes) returned one character and the other two bytes vanished
            // with no diagnostic. It is undecodable input like any other.
            if (i < end) {
                detail::AppendDecoderFallback(result, *this, data + i, end - i);
            }
            return result;
        }

    private:
        /**
         * .NET's UTF32Encoding.SetDefaultFallbacks (`UTF32Encoding.cs:65-76`) installs a U+FFFD
         * replacement fallback rather than the base's "?" -- "For UTF-X encodings, we use a
         * replacement fallback". Needed here for the same reason as in UnicodeEncoding: once the
         * decode routes through the configured fallback, the DEFAULT fallback decides the
         * default output, and U+FFFD is what this encoding substituted before #2017.
         */
        void setDefaultFallbacks() {
            setEncoderFallbackProperty(std::make_shared<EncoderReplacementFallback>("\xEF\xBF\xBD"));
            setDecoderFallbackProperty(std::make_shared<DecoderReplacementFallback>("\xEF\xBF\xBD"));
        }

        void writeUnit(std::vector<SharpRuntime::bytecs>& out, size_t& pos, uint32_t cp) const {
            if (!bigEndian_) {
                out[pos++] = static_cast<SharpRuntime::bytecs>(cp & 0xFF);
                out[pos++] = static_cast<SharpRuntime::bytecs>((cp >> 8) & 0xFF);
                out[pos++] = static_cast<SharpRuntime::bytecs>((cp >> 16) & 0xFF);
                out[pos++] = static_cast<SharpRuntime::bytecs>((cp >> 24) & 0xFF);
            } else {
                out[pos++] = static_cast<SharpRuntime::bytecs>((cp >> 24) & 0xFF);
                out[pos++] = static_cast<SharpRuntime::bytecs>((cp >> 16) & 0xFF);
                out[pos++] = static_cast<SharpRuntime::bytecs>((cp >> 8) & 0xFF);
                out[pos++] = static_cast<SharpRuntime::bytecs>(cp & 0xFF);
            }
        }

        [[nodiscard]] uint32_t readUnit(const SharpRuntime::bytecs* data, std::size_t i) const {
            uint8_t b0 = data[i], b1 = data[i + 1], b2 = data[i + 2], b3 = data[i + 3];
            if (!bigEndian_) {
                return static_cast<uint32_t>(b0) | (static_cast<uint32_t>(b1) << 8) | (static_cast<uint32_t>(b2) << 16) |
                       (static_cast<uint32_t>(b3) << 24);
            }
            return (static_cast<uint32_t>(b0) << 24) | (static_cast<uint32_t>(b1) << 16) | (static_cast<uint32_t>(b2) << 8) |
                   static_cast<uint32_t>(b3);
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
