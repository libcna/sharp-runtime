// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/InvalidOperationException.hpp"

#include <memory>
#include <string>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/Text/DecoderFallback.hpp"
#include "System/Text/EncoderFallback.hpp"

namespace System::Text
{
    /**
     * <summary>
     * Represents a character encoding.
     *
     * Abstract base class; use UTF8(), ASCII(), or Unicode() factory methods
     * to obtain a concrete instance.
     * </summary>
     *
     * @note **Unit of every count and index in this type: UTF-8 storage bytes, not managed
     *       characters.** This runtime represents `System::String` as a UTF-8 `std::string`,
     *       where .NET's `string` is UTF-16, so `GetCharCount` returns the number of **bytes**
     *       `GetString` would produce — `GetCharCount` of the four UTF-8 bytes of U+1F600 is
     *       `4`, where .NET reports `2` UTF-16 code units. Ticket #2012 (SR-AUD-290) states
     *       this rather than leaving the member doc-comments promising characters; changing
     *       the unit is the approval-gated ticket **#2015**
     *       (`docs/SystemTextNamespaceReviewPlan.md` §14.3) and is not done here.
     *
     * @note **The factory instances are shared and READ-ONLY** since ticket #2013 (SR-AUD-288).
     *       `UTF8()` and its six siblings each return the same object on every call, so a
     *       caller that installed a fallback on one changed what every other caller decoded --
     *       and there was a measured data race between the setter and a concurrent conversion.
     *       .NET solves this by making exactly those instances read-only
     *       (`ASCIIEncoding.s_default` and friends) and rejecting a fallback setter on them
     *       with `InvalidOperationException` (`Encoding.cs:485-497`). Call `Clone()` for a
     *       writable copy, which is what .NET's own `Clone` is for -- it clears the flag
     *       (`Encoding.cs:499-506`).
     */
    class Encoding
    {
        std::shared_ptr<DecoderFallback> decoderFallback_ = DecoderFallback::ReplacementFallback();
        std::shared_ptr<EncoderFallback> encoderFallback_ = EncoderFallback::ReplacementFallback();
        /**
         * Whether this instance rejects fallback mutation (ticket #2013).
         *
         * `false` for an encoding a caller constructed or cloned, `true` for the seven shared
         * factory instances. .NET spells the same state as `Encoding._isReadOnly`, set on its
         * `s_default` singletons and cleared by `Clone()`.
         */
        bool isReadOnly_ = false;

    public:
        virtual ~Encoding() = default;

        /** Returns a shared UTF-8 encoding instance. */
        [[nodiscard]] static std::shared_ptr<Encoding> UTF8();

        /** Returns a shared ASCII encoding instance. */
        [[nodiscard]] static std::shared_ptr<Encoding> ASCII();

        /** Returns a shared UTF-16 LE (Unicode) encoding instance. */
        [[nodiscard]] static std::shared_ptr<Encoding> Unicode();

        /** Returns a shared big-endian UTF-16 encoding instance. */
        [[nodiscard]] static std::shared_ptr<Encoding> BigEndianUnicode();

        /** Returns a shared UTF-32 LE encoding instance. */
        [[nodiscard]] static std::shared_ptr<Encoding> UTF32();

        /** Returns a shared UTF-7 encoding instance. */
        [[nodiscard]] static std::shared_ptr<Encoding> UTF7();

        /** Returns a shared Latin-1 (ISO-8859-1) encoding instance. */
        [[nodiscard]] static std::shared_ptr<Encoding> Latin1();

        /** Returns the default encoding for this runtime (UTF-8, matching .NET Core/5+). */
        [[nodiscard]] static std::shared_ptr<Encoding> Default() { return UTF8(); }

        /**
         * Encodes a string to a byte vector using this encoding.
         * @param str The string to encode.
         * @return Encoded bytes.
         */
        [[nodiscard]] virtual std::vector<SharpRuntime::bytecs> GetBytes(const std::string& str) const;

        /**
         * Decodes a range of bytes to a string using this encoding.
         * @param data  Pointer to the byte buffer; may be null only when @p count is zero.
         * @param index Start index within @p data.
         * @param count Number of bytes to decode.
         * @return Decoded string; empty when @p count is zero.
         * @throws System::ArgumentOutOfRangeException if @p index or @p count is negative.
         * @throws System::ArgumentNullException if @p data is null and @p count is positive.
         * @note Ticket #2007 (SR-AUD-286) gave every override in this component one shared
         *       argument policy (`System/Text/detail/RawDecodeRange.hpp`). A raw pointer
         *       carries no length, so an @p index or @p count inside the signed domain but
         *       outside the caller's buffer still cannot be detected — that limit is inherent
         *       to this signature, not a gap in the validation.
         */
        [[nodiscard]] virtual std::string GetString(
            const SharpRuntime::bytecs* data,
            SharpRuntime::intcs index,
            SharpRuntime::intcs count) const;

        /** Decodes an entire byte vector to a string using this encoding. */
        [[nodiscard]] std::string GetString(const std::vector<SharpRuntime::bytecs>& bytes) const {
            return GetString(bytes.data(), 0, static_cast<SharpRuntime::intcs>(bytes.size()));
        }

        /** Returns the number of bytes that GetBytes() would produce for @p str, without allocating them. */
        [[nodiscard]] virtual SharpRuntime::intcs GetByteCount(const std::string& str) const {
            return static_cast<SharpRuntime::intcs>(GetBytes(str).size());
        }

        /**
         * Returns the number of UTF-8 **bytes** that GetString() would produce for the given
         * byte range — not the number of managed UTF-16 characters .NET's `GetCharCount`
         * returns.
         *
         * Measured: for the four UTF-8 bytes of U+1F600 this returns `4`; .NET returns `2`.
         * Ticket #2012 (SR-AUD-290) makes the contract true; changing it is #2015
         * (`docs/SystemTextNamespaceReviewPlan.md` §14.3).
         *
         * @throws System::ArgumentOutOfRangeException if @p index or @p count is negative.
         * @throws System::ArgumentNullException if @p data is null and @p count is positive.
         */
        [[nodiscard]] virtual SharpRuntime::intcs GetCharCount(const SharpRuntime::bytecs* data, SharpRuntime::intcs index,
                                                                SharpRuntime::intcs count) const {
            return static_cast<SharpRuntime::intcs>(GetString(data, index, count).size());
        }

        /** Gets the IANA name of this encoding (e.g. "utf-8", "us-ascii"). */
        [[nodiscard]] virtual std::string getEncodingNameProperty() const { return "utf-8"; }

        /** Gets the code page identifier for this encoding (e.g. 65001 for UTF-8). */
        [[nodiscard]] virtual int getCodePageProperty() const { return 65001; }

        /** Gets the human-readable name of this encoding (defaults to the IANA name). */
        [[nodiscard]] virtual std::string getWebNameProperty() const { return getEncodingNameProperty(); }

        /** Returns true if this encoding uses exactly one byte per character (e.g. ASCII, Latin-1). */
        [[nodiscard]] virtual bool getIsSingleByteProperty() const { return false; }

        /** Gets the DecoderFallback used when a byte sequence cannot be decoded. Never null. */
        [[nodiscard]] std::shared_ptr<DecoderFallback> getDecoderFallbackProperty() const { return decoderFallback_; }
        /**
         * Sets the DecoderFallback used when a byte sequence cannot be decoded.
         *
         * @throws System::ArgumentNullException if @p value is null.
         * @note Ticket #2008 (SR-AUD-287). Every conversion path dereferences this object
         *       without checking it, so accepting null armed a segmentation fault that fired
         *       on the next malformed byte, arbitrarily far from the setter that caused it —
         *       measured in `build-probe/2006_probe1_before.log` §B. A rejected value leaves
         *       the previous fallback installed. .NET's `Encoding.DecoderFallback` setter
         *       raises the same exception for the same reason.
         */
        void setDecoderFallbackProperty(std::shared_ptr<DecoderFallback> value) {
            // #2013: the read-only test runs FIRST, and that order is .NET's --
            // `Encoding.cs:490-494` checks IsReadOnly before ArgumentNullException.ThrowIfNull,
            // so a null value handed to a shared factory instance reports the read-only
            // violation rather than the null one.
            throwIfReadOnly();
            if (value == nullptr) {
                throw System::ArgumentNullException("value");
            }
            decoderFallback_ = std::move(value);
        }

        /** Gets the EncoderFallback used when a character cannot be encoded. Never null. */
        [[nodiscard]] std::shared_ptr<EncoderFallback> getEncoderFallbackProperty() const { return encoderFallback_; }
        /**
         * Sets the EncoderFallback used when a character cannot be encoded.
         *
         * @throws System::ArgumentNullException if @p value is null.
         * @note Ticket #2008 (SR-AUD-287). The encoder direction is the identical crash as
         *       the decoder direction above; the finding names only the decoder, and
         *       repairing one setter would have left the other.
         */
        void setEncoderFallbackProperty(std::shared_ptr<EncoderFallback> value) {
            throwIfReadOnly();
            if (value == nullptr) {
                throw System::ArgumentNullException("value");
            }
            encoderFallback_ = std::move(value);
        }

        /** Returns true if @p other has the same code page as this encoding. */
        [[nodiscard]] virtual bool Equals(const Encoding& other) const { return getCodePageProperty() == other.getCodePageProperty(); }
        bool operator==(const Encoding& o) const { return Equals(o); }
        bool operator!=(const Encoding& o) const { return !Equals(o); }

        /** @return A hash code derived from the code page identifier. */
        [[nodiscard]] virtual SharpRuntime::intcs GetHashCode() const { return getCodePageProperty(); }

        /**
         * @brief Whether this instance rejects fallback mutation.
         *
         * True for the seven shared factory instances, false for anything a caller constructed
         * or obtained from `Clone()`. C++ counterpart of .NET `Encoding.IsReadOnly`
         * (`Encoding.cs:508-512`).
         */
        [[nodiscard]] bool getIsReadOnlyProperty() const { return isReadOnly_; }

    protected:
        Encoding() = default;

        /**
         * Throws `InvalidOperationException("Instance is read-only.")` when this instance is one
         * of the shared factory encodings. The message is .NET's `SR.InvalidOperation_ReadOnly`
         * (`Strings.resx:2753`).
         */
        void throwIfReadOnly() const {
            if (isReadOnly_) {
                throw System::InvalidOperationException("Instance is read-only.");
            }
        }

        /** Marks this instance read-only. Used by the factory singletons only. */
        void markReadOnly() { isReadOnly_ = true; }


    };
}
