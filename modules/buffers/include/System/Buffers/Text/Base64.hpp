// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/FormatException.hpp"
#include "System/Buffers/OperationStatus.hpp"
#include "System/Span.hpp"
#include "System/ReadOnlySpan.hpp"

namespace System::Buffers::Text {

using SharpRuntime::intcs;

/**
 * @brief Converts between binary data and UTF-8 base64 encoded text.
 *
 * C++ counterpart of .NET System.Buffers.Text.Base64.
 * All methods are static. The implementation uses the standard RFC 4648 base64
 * alphabet with '=' padding. Any amount of ASCII whitespace (' ', '\\t', '\\r', '\\n')
 * is allowed anywhere in decode/validate input, matching .NET's Base64 behavior.
 */
class Base64 {
    /** Encoding produces at most this many bytes for a source of int.MaxValue/4*3 bytes (.NET's Base64Helper.MaximumEncodeLength). */
    static constexpr intcs kMaximumEncodeLength = 1610612733;

    static constexpr char kEncTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    static constexpr int8_t kDecTable[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };
    // -2 means '=' (padding); -1 means invalid

    static bool isWhitespace(uint8_t c) noexcept {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    // Shared decode core for both byte- and char-sourced overloads (CharT is uint8_t or char);
    // base64 text is always ASCII, so both element types are read via a static_cast<uint8_t>.
    template<typename CharT>
    static OperationStatus decodeCore(const CharT* src, intcs srcLen, uint8_t* dst, intcs dstLen,
                                       intcs& consumed, intcs& written, bool isFinalBlock) {
        consumed = 0; written = 0;
        intcs si = 0, di = 0;
        int8_t vals[4] = {0, 0, 0, 0};
        int valCount = 0, padCount = 0;

        while (si < srcLen) {
            uint8_t ch = static_cast<uint8_t>(src[si]);
            if (isWhitespace(ch)) { ++si; continue; }
            int8_t v = kDecTable[ch];
            ++si;
            if (v == -1) return OperationStatus::InvalidData;
            if (v == -2) {
                // '=' terminates the base64 content, so it can only ever be meaningful in
                // a FINAL block. .NET never reaches its padding handler otherwise:
                // Base64DecoderHelper.DecodeFrom sets `skipLastChunk = isFinalBlock ? 4 : 0`,
                // so with the flag clear the whole source runs through the four-element loop
                // where '=' is just an unmapped character, and the whitespace fallback
                // (DecodeWithWhiteSpaceBlockwise) forces its per-block localIsFinalBlock back
                // to false for the same reason. Without this, a chunked caller was told a
                // terminal quantum was ordinary intermediate data: DecodeFromUtf8("QQ==", ...,
                // false) returned Done with 4 consumed and 'A' written (ticket #1818 /
                // SR-AUD-080, build-probe/1818_prefix_defects.log). Rejecting at the FIRST
                // '=' rather than at the end of its quantum is what keeps bytesConsumed and
                // bytesWritten on the last completed quantum boundary, which is where .NET
                // leaves them too.
                if (!isFinalBlock) return OperationStatus::InvalidData;
                if (valCount < 2) return OperationStatus::InvalidData;
                ++padCount;
                vals[valCount] = 0;
            } else {
                if (padCount > 0) return OperationStatus::InvalidData;
                vals[valCount] = v;
            }
            ++valCount;

            if (valCount == 4) {
                if (padCount == 0) {
                    if (dstLen - di < 3) return OperationStatus::DestinationTooSmall;
                    uint32_t val = (uint32_t(vals[0])<<18)|(uint32_t(vals[1])<<12)|(uint32_t(vals[2])<<6)|uint32_t(vals[3]);
                    dst[di++] = static_cast<uint8_t>(val >> 16);
                    dst[di++] = static_cast<uint8_t>(val >> 8);
                    dst[di++] = static_cast<uint8_t>(val);
                } else if (padCount == 1) {
                    // A single '=' means the quantum carries two bytes, so only the top four
                    // bits of the third sextet are used and its low two bits MUST be zero.
                    // Without this, `AAB=` decoded to Done and IsValid agreed, where .NET
                    // rejects it -- Base64DecoderHelper.cs tests exactly these bits (ticket
                    // #1817 / SR-AUD-079). Checked before the destination check on purpose:
                    // whether the input is canonical is a property of the input alone and
                    // must not depend on how much room the caller happened to provide.
                    if ((vals[2] & 0x03) != 0) return OperationStatus::InvalidData;
                    if (dstLen - di < 2) return OperationStatus::DestinationTooSmall;
                    uint32_t val = (uint32_t(vals[0])<<18)|(uint32_t(vals[1])<<12)|(uint32_t(vals[2])<<6);
                    dst[di++] = static_cast<uint8_t>(val >> 16);
                    dst[di++] = static_cast<uint8_t>(val >> 8);
                } else { // padCount == 2
                    // Two '=' means the quantum carries one byte, so only the top two bits of
                    // the second sextet are used and its low four bits MUST be zero. `AB==`
                    // used to decode to Done. Same ordering rationale as above.
                    if ((vals[1] & 0x0F) != 0) return OperationStatus::InvalidData;
                    if (dstLen - di < 1) return OperationStatus::DestinationTooSmall;
                    uint32_t val = (uint32_t(vals[0])<<18)|(uint32_t(vals[1])<<12);
                    dst[di++] = static_cast<uint8_t>(val >> 16);
                }
                consumed = si; written = di;

                if (padCount > 0) {
                    // Padding terminates the base64 content; only trailing whitespace may follow.
                    for (intcs r = si; r < srcLen; ++r) {
                        if (!isWhitespace(static_cast<uint8_t>(src[r]))) return OperationStatus::InvalidData;
                    }
                    consumed = srcLen;
                    return OperationStatus::Done;
                }
                valCount = 0; padCount = 0;
            }
        }

        if (valCount > 0) {
            return isFinalBlock ? OperationStatus::InvalidData : OperationStatus::NeedMoreData;
        }
        consumed = si; written = di;
        return OperationStatus::Done;
    }

    // Shared validate core: same grammar as decodeCore but only counts decoded bytes.
    // It must agree with decodeCore on every rule, including the canonical final-bit rule
    // (ticket #1817 / SR-AUD-079); a validator that is more permissive than the decoder is
    // worse than no validator, because it tells the caller an input is safe to decode when
    // it is not. That is why the sextet values are retained here and not only counted.
    template<typename CharT>
    static bool validateCore(const CharT* src, intcs srcLen, intcs& decodedLength) {
        decodedLength = 0;
        intcs si = 0, di = 0;
        int8_t vals[4] = {0, 0, 0, 0};
        int valCount = 0, padCount = 0;

        while (si < srcLen) {
            uint8_t ch = static_cast<uint8_t>(src[si]);
            if (isWhitespace(ch)) { ++si; continue; }
            int8_t v = kDecTable[ch];
            ++si;
            if (v == -1) return false;
            if (v == -2) {
                if (valCount < 2) return false;
                ++padCount;
                vals[valCount] = 0;
            } else {
                if (padCount > 0) return false;
                vals[valCount] = v;
            }
            ++valCount;

            if (valCount == 4) {
                if (padCount == 1 && (vals[2] & 0x03) != 0) return false;
                if (padCount == 2 && (vals[1] & 0x0F) != 0) return false;
                di += (padCount == 0) ? 3 : (padCount == 1 ? 2 : 1);
                if (padCount > 0) {
                    for (intcs r = si; r < srcLen; ++r) {
                        if (!isWhitespace(static_cast<uint8_t>(src[r]))) return false;
                    }
                    decodedLength = di;
                    return true;
                }
                valCount = 0; padCount = 0;
            }
        }
        if (valCount > 0) return false;
        decodedLength = di;
        return true;
    }

    // Shared encode core for both byte- and char-destination overloads.
    template<typename CharT>
    static OperationStatus encodeCore(const uint8_t* src, intcs srcLen, CharT* dst, intcs dstLen,
                                       intcs& bytesConsumed, intcs& written, bool isFinalBlock) {
        bytesConsumed = 0; written = 0;
        intcs fullGroups = srcLen / 3;
        intcs remainder  = srcLen % 3;

        for (intcs i = 0; i < fullGroups; ++i) {
            if (dstLen - written < 4) return OperationStatus::DestinationTooSmall;
            uint32_t b = (uint32_t(src[0]) << 16) | (uint32_t(src[1]) << 8) | src[2];
            dst[0] = static_cast<CharT>(kEncTable[(b >> 18) & 0x3F]);
            dst[1] = static_cast<CharT>(kEncTable[(b >> 12) & 0x3F]);
            dst[2] = static_cast<CharT>(kEncTable[(b >>  6) & 0x3F]);
            dst[3] = static_cast<CharT>(kEncTable[(b      ) & 0x3F]);
            src += 3; dst += 4;
            bytesConsumed += 3;
            written  += 4;
        }

        if (!isFinalBlock && remainder > 0) {
            return OperationStatus::NeedMoreData;
        }

        if (isFinalBlock && remainder > 0) {
            if (dstLen - written < 4) return OperationStatus::DestinationTooSmall;
            if (remainder == 1) {
                uint32_t b = uint32_t(src[0]) << 16;
                dst[0] = static_cast<CharT>(kEncTable[(b >> 18) & 0x3F]);
                dst[1] = static_cast<CharT>(kEncTable[(b >> 12) & 0x3F]);
                dst[2] = static_cast<CharT>('='); dst[3] = static_cast<CharT>('=');
            } else {
                uint32_t b = (uint32_t(src[0]) << 16) | (uint32_t(src[1]) << 8);
                dst[0] = static_cast<CharT>(kEncTable[(b >> 18) & 0x3F]);
                dst[1] = static_cast<CharT>(kEncTable[(b >> 12) & 0x3F]);
                dst[2] = static_cast<CharT>(kEncTable[(b >>  6) & 0x3F]);
                dst[3] = static_cast<CharT>('=');
            }
            bytesConsumed += remainder;
            written  += 4;
        }
        return OperationStatus::Done;
    }

public:
    Base64() = delete;

    // -----------------------------------------------------------------------
    // Length helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Returns the maximum number of bytes that decoding @p base64Length bytes can produce.
     * C++ counterpart of .NET Base64.GetMaxDecodedFromUtf8Length(int).
     * @throws ArgumentOutOfRangeException if @p base64Length is negative.
     */
    [[nodiscard]] static intcs GetMaxDecodedFromUtf8Length(intcs base64Length) {
        if (base64Length < 0) throw ArgumentOutOfRangeException("base64Length", "Non-negative number required.");
        return (base64Length / 4) * 3;
    }

    /**
     * @brief Returns the maximum number of bytes that decoding @p base64Length bytes can produce.
     * C++ counterpart of .NET Base64.GetMaxDecodedLength(int) — alias of GetMaxDecodedFromUtf8Length.
     */
    [[nodiscard]] static intcs GetMaxDecodedLength(intcs base64Length) {
        return GetMaxDecodedFromUtf8Length(base64Length);
    }

    /**
     * @brief Returns the exact number of bytes that encoding @p bytesLength bytes produces.
     * C++ counterpart of .NET Base64.GetMaxEncodedToUtf8Length(int).
     * @throws ArgumentOutOfRangeException if @p bytesLength is negative or exceeds 1610612733.
     */
    [[nodiscard]] static intcs GetMaxEncodedToUtf8Length(intcs bytesLength) {
        if (bytesLength < 0 || bytesLength > kMaximumEncodeLength)
            throw ArgumentOutOfRangeException("bytesLength", "Non-negative number required, and less than or equal to 1610612733.");
        return ((bytesLength + 2) / 3) * 4;
    }

    /**
     * @brief Returns the exact number of bytes that encoding @p bytesLength bytes produces.
     * C++ counterpart of .NET Base64.GetEncodedLength(int) — alias of GetMaxEncodedToUtf8Length.
     */
    [[nodiscard]] static intcs GetEncodedLength(intcs bytesLength) {
        return GetMaxEncodedToUtf8Length(bytesLength);
    }

    // -----------------------------------------------------------------------
    // Encode — bytes
    // -----------------------------------------------------------------------

    /**
     * @brief Encodes binary bytes as UTF-8 base64 text.
     *
     * C++ counterpart of .NET Base64.EncodeToUtf8(ReadOnlySpan, Span, out int, out int, bool).
     * @param bytes           Source bytes.
     * @param utf8            Destination for base64 ASCII bytes.
     * @param bytesConsumed   Set to number of source bytes consumed.
     * @param bytesWritten    Set to number of output bytes written.
     * @param isFinalBlock    If true, output is padded with '='.
     * @return OperationStatus::Done, DestinationTooSmall, or NeedMoreData.
     */
    static OperationStatus EncodeToUtf8(
        System::ReadOnlySpan<uint8_t> bytes,
        System::Span<uint8_t>         utf8,
        intcs&                        bytesConsumed,
        intcs&                        bytesWritten,
        bool                          isFinalBlock = true)
    {
        return encodeCore<uint8_t>(bytes.getPointer(), bytes.getLengthProperty(),
                                    utf8.getPointer(), utf8.getLengthProperty(),
                                    bytesConsumed, bytesWritten, isFinalBlock);
    }

    /**
     * @brief Encodes binary bytes as UTF-8 base64 text, throwing if @p destination is too small.
     * C++ counterpart of .NET Base64.EncodeToUtf8(ReadOnlySpan, Span).
     * @return The number of bytes written into @p destination.
     * @throws ArgumentException if @p destination is too small to hold the encoded output.
     */
    static intcs EncodeToUtf8(System::ReadOnlySpan<uint8_t> source, System::Span<uint8_t> destination) {
        intcs consumed = 0, written = 0;
        OperationStatus status = EncodeToUtf8(source, destination, consumed, written);
        if (status == OperationStatus::Done) return written;
        throw ArgumentException("Destination is too short.", "destination");
    }

    /**
     * @brief Encodes binary bytes as UTF-8 base64 text into a newly allocated buffer.
     * C++ counterpart of .NET Base64.EncodeToUtf8(ReadOnlySpan).
     */
    [[nodiscard]] static std::vector<uint8_t> EncodeToUtf8(System::ReadOnlySpan<uint8_t> source) {
        intcs len = GetEncodedLength(source.getLengthProperty());
        std::vector<uint8_t> destination(static_cast<size_t>(len));
        intcs consumed = 0, written = 0;
        System::Span<uint8_t> dst(destination.data(), len);
        EncodeToUtf8(source, dst, consumed, written);
        return destination;
    }

    /**
     * @brief Attempts to encode binary bytes as UTF-8 base64 text.
     * C++ counterpart of .NET Base64.TryEncodeToUtf8(ReadOnlySpan, Span, out int).
     * @return true if the destination was large enough; false otherwise.
     */
    static bool TryEncodeToUtf8(System::ReadOnlySpan<uint8_t> source, System::Span<uint8_t> destination,
                                 intcs& bytesWritten) {
        intcs consumed = 0;
        return EncodeToUtf8(source, destination, consumed, bytesWritten) == OperationStatus::Done;
    }

    /**
     * @brief Encodes @p dataLength bytes at the start of @p buffer, in place, as base64 text.
     *
     * C++ counterpart of .NET Base64.EncodeToUtf8InPlace(Span, int, out int).
     * Encodes the trailing one/two-byte pack first and then walks the full 3-byte packs
     * from the last to the first, so a (larger) 4-byte encoded pack never overwrites a
     * source byte that has not been read yet.
     * @return OperationStatus::Done or DestinationTooSmall (never NeedMoreData/InvalidData).
     */
    static OperationStatus EncodeToUtf8InPlace(System::Span<uint8_t> buffer, intcs dataLength, intcs& bytesWritten) {
        bytesWritten = 0;
        intcs encodedLen = GetEncodedLength(dataLength);
        if (encodedLen > buffer.getLengthProperty()) return OperationStatus::DestinationTooSmall;

        uint8_t* buf = buffer.getPointer();
        intcs fullGroups = dataLength / 3;
        intcs remainder  = dataLength % 3;

        // The trailing remainder is the LAST pack, so it must be encoded FIRST -- this
        // ordering is the whole correctness argument and must not be "tidied" back into
        // source order. Encoding pack i writes at 4i..4i+3 and reads at 3i..3i+2, and
        // 4i >= 3i, so a pack can only clobber source bytes belonging to packs after it.
        // Walking the full packs backwards therefore protects every pack except the
        // remainder, which sits after all of them. Previously the remainder was read
        // after the loop, so the first full pack's fourth output byte had already
        // overwritten it: 'A','B','C',0 encoded as QUJDRA== instead of QUJDAA== and
        // still returned Done. That is wrong for EVERY length with both a full pack and
        // a remainder, not only 4 and 5 -- a 0..24 sweep against this type's own
        // out-of-place encoder produced 14 wrong lengths per type, 28 in total
        // (ticket #1816 / SR-AUD-078 / CCF-013, build-probe/1816_prefix_defects.log).
        //
        // .NET does the same thing for the same reason: Base64EncoderHelper.cs's
        // EncodeToUtf8InPlace encodes the leftover pack before its backwards loop, under
        // the comment "encode last pack to avoid conditional in the main loop".
        intcs dstOff = fullGroups * 4;
        if (remainder == 1) {
            uint8_t b0 = buf[fullGroups * 3];
            uint32_t b = uint32_t(b0) << 16;
            buf[dstOff]   = static_cast<uint8_t>(kEncTable[(b >> 18) & 0x3F]);
            buf[dstOff+1] = static_cast<uint8_t>(kEncTable[(b >> 12) & 0x3F]);
            buf[dstOff+2] = '='; buf[dstOff+3] = '=';
        } else if (remainder == 2) {
            uint8_t b0 = buf[fullGroups * 3], b1 = buf[fullGroups * 3 + 1];
            uint32_t b = (uint32_t(b0) << 16) | (uint32_t(b1) << 8);
            buf[dstOff]   = static_cast<uint8_t>(kEncTable[(b >> 18) & 0x3F]);
            buf[dstOff+1] = static_cast<uint8_t>(kEncTable[(b >> 12) & 0x3F]);
            buf[dstOff+2] = static_cast<uint8_t>(kEncTable[(b >>  6) & 0x3F]);
            buf[dstOff+3] = '=';
        }

        for (intcs i = fullGroups - 1; i >= 0; --i) {
            intcs srcOff = i * 3, dstOffFull = i * 4;
            uint32_t b = (uint32_t(buf[srcOff]) << 16) | (uint32_t(buf[srcOff+1]) << 8) | buf[srcOff+2];
            buf[dstOffFull]   = static_cast<uint8_t>(kEncTable[(b >> 18) & 0x3F]);
            buf[dstOffFull+1] = static_cast<uint8_t>(kEncTable[(b >> 12) & 0x3F]);
            buf[dstOffFull+2] = static_cast<uint8_t>(kEncTable[(b >>  6) & 0x3F]);
            buf[dstOffFull+3] = static_cast<uint8_t>(kEncTable[(b      ) & 0x3F]);
        }

        bytesWritten = encodedLen;
        return OperationStatus::Done;
    }

    /**
     * @brief Attempts to encode @p dataLength bytes at the start of @p buffer, in place, as base64 text.
     * C++ counterpart of .NET Base64.TryEncodeToUtf8InPlace(Span, int, out int).
     */
    static bool TryEncodeToUtf8InPlace(System::Span<uint8_t> buffer, intcs dataLength, intcs& bytesWritten) {
        return EncodeToUtf8InPlace(buffer, dataLength, bytesWritten) == OperationStatus::Done;
    }

    // -----------------------------------------------------------------------
    // Encode — chars
    // -----------------------------------------------------------------------

    /**
     * @brief Encodes binary bytes as base64 ASCII chars.
     * C++ counterpart of .NET Base64.EncodeToChars(ReadOnlySpan&lt;byte&gt;, Span&lt;char&gt;, out int, out int, bool).
     */
    static OperationStatus EncodeToChars(
        System::ReadOnlySpan<uint8_t> source,
        System::Span<char>            destination,
        intcs&                        bytesConsumed,
        intcs&                        charsWritten,
        bool                          isFinalBlock = true)
    {
        return encodeCore<char>(source.getPointer(), source.getLengthProperty(),
                                 destination.getPointer(), destination.getLengthProperty(),
                                 bytesConsumed, charsWritten, isFinalBlock);
    }

    /**
     * @brief Encodes binary bytes as base64 ASCII chars, throwing if @p destination is too small.
     * C++ counterpart of .NET Base64.EncodeToChars(ReadOnlySpan&lt;byte&gt;, Span&lt;char&gt;).
     */
    static intcs EncodeToChars(System::ReadOnlySpan<uint8_t> source, System::Span<char> destination) {
        intcs consumed = 0, written = 0;
        OperationStatus status = EncodeToChars(source, destination, consumed, written);
        if (status == OperationStatus::Done) return written;
        throw ArgumentException("Destination is too short.", "destination");
    }

    /**
     * @brief Attempts to encode binary bytes as base64 ASCII chars.
     * C++ counterpart of .NET Base64.TryEncodeToChars(ReadOnlySpan&lt;byte&gt;, Span&lt;char&gt;, out int).
     */
    static bool TryEncodeToChars(System::ReadOnlySpan<uint8_t> source, System::Span<char> destination,
                                  intcs& charsWritten) {
        intcs consumed = 0;
        return EncodeToChars(source, destination, consumed, charsWritten) == OperationStatus::Done;
    }

    /**
     * @brief Encodes binary bytes as a std::string of base64 ASCII.
     * C++ counterpart of .NET Base64.EncodeToString(ReadOnlySpan&lt;byte&gt;).
     */
    [[nodiscard]] static std::string EncodeToString(System::ReadOnlySpan<uint8_t> source) {
        intcs len = GetEncodedLength(source.getLengthProperty());
        std::string result(static_cast<size_t>(len), '\0');
        intcs consumed = 0, written = 0;
        System::Span<char> dst(result.data(), len);
        EncodeToChars(source, dst, consumed, written);
        return result;
    }

    /**
     * @brief Encodes binary bytes as a std::string of base64 ASCII.
     * Convenience overload accepting a std::vector directly (not in .NET API, but useful in C++).
     */
    [[nodiscard]] static std::string EncodeToString(const std::vector<uint8_t>& bytes) {
        return EncodeToString(System::ReadOnlySpan<uint8_t>(bytes));
    }

    // -----------------------------------------------------------------------
    // Decode — bytes
    // -----------------------------------------------------------------------

    /**
     * @brief Decodes UTF-8 base64 text to binary bytes.
     *
     * C++ counterpart of .NET Base64.DecodeFromUtf8(ReadOnlySpan, Span, out int, out int, bool).
     * Any amount of ASCII whitespace (' ', '\\t', '\\r', '\\n') is allowed anywhere in @p utf8.
     * @param utf8            Source base64 ASCII bytes.
     * @param bytes           Destination for decoded bytes.
     * @param bytesConsumed   Set to number of input bytes consumed.
     * @param bytesWritten    Set to number of decoded bytes written.
     * @param isFinalBlock    If true, trailing padding '=' is expected/allowed and an incomplete
     *                        trailing group is InvalidData rather than NeedMoreData. If false,
     *                        a '=' anywhere in @p utf8 is InvalidData — padding terminates the
     *                        content, so it cannot appear in a block that is not the last one
     *                        (ticket #1818 / SR-AUD-080; .NET's Base64DecoderHelper routes a
     *                        non-final call around final-padding handling entirely).
     * @return OperationStatus.
     */
    static OperationStatus DecodeFromUtf8(
        System::ReadOnlySpan<uint8_t> utf8,
        System::Span<uint8_t>         bytes,
        intcs&                        bytesConsumed,
        intcs&                        bytesWritten,
        bool                          isFinalBlock = true)
    {
        return decodeCore<uint8_t>(utf8.getPointer(), utf8.getLengthProperty(),
                                    bytes.getPointer(), bytes.getLengthProperty(),
                                    bytesConsumed, bytesWritten, isFinalBlock);
    }

    /**
     * @brief Decodes UTF-8 base64 text to binary bytes, throwing on invalid data or a too-small destination.
     * C++ counterpart of .NET Base64.DecodeFromUtf8(ReadOnlySpan, Span).
     * @throws ArgumentException if @p destination is too small.
     * @throws FormatException if @p source contains invalid base64 data.
     */
    static intcs DecodeFromUtf8(System::ReadOnlySpan<uint8_t> source, System::Span<uint8_t> destination) {
        intcs consumed = 0, written = 0;
        OperationStatus status = DecodeFromUtf8(source, destination, consumed, written);
        if (status == OperationStatus::Done) return written;
        if (status == OperationStatus::DestinationTooSmall)
            throw ArgumentException("Destination is too short.", "destination");
        throw FormatException("The input is not a valid Base-64 string as it contains a non-base 64 character, more than two padding characters, or an illegal character among the padding characters.");
    }

    /**
     * @brief Decodes UTF-8 base64 text into a newly allocated buffer.
     * C++ counterpart of .NET Base64.DecodeFromUtf8(ReadOnlySpan).
     * @throws FormatException if @p source contains invalid base64 data.
     */
    [[nodiscard]] static std::vector<uint8_t> DecodeFromUtf8(System::ReadOnlySpan<uint8_t> source) {
        intcs upperBound = GetMaxDecodedLength(source.getLengthProperty());
        std::vector<uint8_t> destination(static_cast<size_t>(upperBound));
        intcs consumed = 0, written = 0;
        System::Span<uint8_t> dst(destination.data(), upperBound);
        OperationStatus status = DecodeFromUtf8(source, dst, consumed, written);
        if (status != OperationStatus::Done)
            throw FormatException("The input is not a valid Base-64 string as it contains a non-base 64 character, more than two padding characters, or an illegal character among the padding characters.");
        destination.resize(static_cast<size_t>(written));
        return destination;
    }

    /**
     * @brief Attempts to decode UTF-8 base64 text to binary bytes.
     * C++ counterpart of .NET Base64.TryDecodeFromUtf8(ReadOnlySpan, Span, out int).
     * @throws FormatException if @p source contains invalid base64 data.
     * @return true if bytes decoded successfully; false if @p destination was too small.
     */
    static bool TryDecodeFromUtf8(System::ReadOnlySpan<uint8_t> source, System::Span<uint8_t> destination,
                                   intcs& bytesWritten) {
        intcs consumed = 0;
        OperationStatus status = DecodeFromUtf8(source, destination, consumed, bytesWritten);
        if (status == OperationStatus::InvalidData)
            throw FormatException("The input is not a valid Base-64 string as it contains a non-base 64 character, more than two padding characters, or an illegal character among the padding characters.");
        return status == OperationStatus::Done;
    }

    /**
     * @brief Decodes base64 text in @p buffer into binary data, in place.
     *
     * C++ counterpart of .NET Base64.DecodeFromUtf8InPlace(Span, out int).
     * Safe because decoding always deflates data (the output cursor never overtakes the input cursor).
     * @return OperationStatus::Done or InvalidData (never DestinationTooSmall/NeedMoreData).
     */
    static OperationStatus DecodeFromUtf8InPlace(System::Span<uint8_t> buffer, intcs& bytesWritten) {
        intcs consumed = 0;
        intcs len = buffer.getLengthProperty();
        System::ReadOnlySpan<uint8_t> source(buffer.getPointer(), len);
        return DecodeFromUtf8(source, buffer, consumed, bytesWritten, true);
    }

    // -----------------------------------------------------------------------
    // Decode — chars
    // -----------------------------------------------------------------------

    /**
     * @brief Decodes base64 ASCII chars to binary bytes.
     * C++ counterpart of .NET Base64.DecodeFromChars(ReadOnlySpan&lt;char&gt;, Span&lt;byte&gt;, out int, out int, bool).
     * Shares decodeCore with the UTF-8 overload, so @p isFinalBlock has exactly the same
     * meaning here, including that a '=' with @p isFinalBlock false is InvalidData.
     */
    static OperationStatus DecodeFromChars(
        System::ReadOnlySpan<char> source,
        System::Span<uint8_t>      destination,
        intcs&                     charsConsumed,
        intcs&                     bytesWritten,
        bool                       isFinalBlock = true)
    {
        return decodeCore<char>(source.getPointer(), source.getLengthProperty(),
                                 destination.getPointer(), destination.getLengthProperty(),
                                 charsConsumed, bytesWritten, isFinalBlock);
    }

    /**
     * @brief Decodes base64 ASCII chars to binary bytes, throwing on invalid data or a too-small destination.
     * C++ counterpart of .NET Base64.DecodeFromChars(ReadOnlySpan&lt;char&gt;, Span&lt;byte&gt;).
     */
    static intcs DecodeFromChars(System::ReadOnlySpan<char> source, System::Span<uint8_t> destination) {
        intcs consumed = 0, written = 0;
        OperationStatus status = DecodeFromChars(source, destination, consumed, written);
        if (status == OperationStatus::Done) return written;
        if (status == OperationStatus::DestinationTooSmall)
            throw ArgumentException("Destination is too short.", "destination");
        throw FormatException("The input is not a valid Base-64 string as it contains a non-base 64 character, more than two padding characters, or an illegal character among the padding characters.");
    }

    /**
     * @brief Attempts to decode base64 ASCII chars to binary bytes.
     * C++ counterpart of .NET Base64.TryDecodeFromChars(ReadOnlySpan&lt;char&gt;, Span&lt;byte&gt;, out int).
     */
    static bool TryDecodeFromChars(System::ReadOnlySpan<char> source, System::Span<uint8_t> destination,
                                    intcs& bytesWritten) {
        intcs consumed = 0;
        OperationStatus status = DecodeFromChars(source, destination, consumed, bytesWritten);
        if (status == OperationStatus::InvalidData)
            throw FormatException("The input is not a valid Base-64 string as it contains a non-base 64 character, more than two padding characters, or an illegal character among the padding characters.");
        return status == OperationStatus::Done;
    }

    /**
     * @brief Decodes base64 ASCII chars into a newly allocated buffer.
     * C++ counterpart of .NET Base64.DecodeFromChars(ReadOnlySpan&lt;char&gt;).
     */
    [[nodiscard]] static std::vector<uint8_t> DecodeFromChars(System::ReadOnlySpan<char> source) {
        intcs upperBound = GetMaxDecodedLength(source.getLengthProperty());
        std::vector<uint8_t> destination(static_cast<size_t>(upperBound));
        intcs consumed = 0, written = 0;
        System::Span<uint8_t> dst(destination.data(), upperBound);
        OperationStatus status = DecodeFromChars(source, dst, consumed, written);
        if (status != OperationStatus::Done)
            throw FormatException("The input is not a valid Base-64 string as it contains a non-base 64 character, more than two padding characters, or an illegal character among the padding characters.");
        destination.resize(static_cast<size_t>(written));
        return destination;
    }

    // -----------------------------------------------------------------------
    // Validate
    // -----------------------------------------------------------------------

    /**
     * @brief Returns true if @p base64Text is valid base64.
     * C++ counterpart of .NET Base64.IsValid(ReadOnlySpan&lt;byte&gt;).
     */
    [[nodiscard]] static bool IsValid(System::ReadOnlySpan<uint8_t> base64Text) {
        intcs decodedLength = 0;
        return validateCore<uint8_t>(base64Text.getPointer(), base64Text.getLengthProperty(), decodedLength);
    }

    /**
     * @brief Returns true if @p base64Text (as char string) is valid base64.
     * C++ counterpart of .NET Base64.IsValid(ReadOnlySpan&lt;char&gt;).
     */
    [[nodiscard]] static bool IsValid(System::ReadOnlySpan<char> base64Text) {
        intcs decodedLength = 0;
        return validateCore<char>(base64Text.getPointer(), base64Text.getLengthProperty(), decodedLength);
    }

    /**
     * @brief Validates base64 input and returns true along with the decoded byte count.
     * C++ counterpart of .NET Base64.IsValid(ReadOnlySpan&lt;byte&gt;, out int).
     */
    [[nodiscard]] static bool IsValid(System::ReadOnlySpan<uint8_t> base64Text, intcs& decodedLength) {
        return validateCore<uint8_t>(base64Text.getPointer(), base64Text.getLengthProperty(), decodedLength);
    }

    /**
     * @brief Validates base64 char input and returns true along with the decoded byte count.
     * C++ counterpart of .NET Base64.IsValid(ReadOnlySpan&lt;char&gt;, out int).
     */
    [[nodiscard]] static bool IsValid(System::ReadOnlySpan<char> base64Text, intcs& decodedLength) {
        return validateCore<char>(base64Text.getPointer(), base64Text.getLengthProperty(), decodedLength);
    }
};

} // namespace System::Buffers::Text
