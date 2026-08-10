// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Text/Encoding.hpp"

namespace System::Text {

    /**
     * @brief Converts a sequence of encoded bytes into a set of characters.
     *
     * Wraps an Encoding instance. Partial C++ counterpart of .NET System.Text.Decoder.
     *
     * @note Status: Partial — stateless; no support for split multi-byte sequences across calls.
     */
    class Decoder {
        std::shared_ptr<Encoding> encoding_;
    public:
        /** Constructs a Decoder wrapping the given Encoding. */
        explicit Decoder(std::shared_ptr<Encoding> encoding) : encoding_(std::move(encoding)) {}

        /** @brief Decodes a byte buffer into a string. */
        [[nodiscard]] std::string GetString(const SharpRuntime::bytecs* bytes,
                                            SharpRuntime::intcs         index,
                                            SharpRuntime::intcs         count) const {
            return encoding_->GetString(bytes, index, count);
        }

        /**
         * Decodes a byte vector to a string, with optional offset and count.
         *
         * A negative @p count means "the rest of the vector from @p index".
         *
         * @throws System::ArgumentOutOfRangeException if @p index is negative or greater than
         *         the vector's size.
         */
        [[nodiscard]] std::string GetString(const std::vector<SharpRuntime::bytecs>& bytes,
                                            SharpRuntime::intcs index = 0,
                                            SharpRuntime::intcs count = -1) const {
            // Ticket #2007 (SR-AUD-286): the derived length is `size - index`, so a NEGATIVE
            // index made it LONGER than the vector -- measured, GetString(vec, -1) over a
            // four-element vector read five bytes. The encoding's own validator would now
            // reject it, but only after the caller's own metadata had been inflated; the
            // diagnostic belongs to this overload's own parameter.
            if (index < 0 || index > static_cast<SharpRuntime::intcs>(bytes.size())) {
                throw System::ArgumentOutOfRangeException("index");
            }
            SharpRuntime::intcs len = (count < 0) ? static_cast<SharpRuntime::intcs>(bytes.size()) - index : count;
            return encoding_->GetString(bytes.data(), index, len);
        }

        /** @brief Resets decoder state (no-op for stateless encoding). */
        void Reset() {}
    };

} // namespace System::Text
