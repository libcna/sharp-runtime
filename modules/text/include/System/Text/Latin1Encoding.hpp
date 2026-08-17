// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
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
         * Copies the UTF-8 storage bytes of @p s through unchanged.
         *
         * Despite the class name this is **not** an ISO-8859-1 conversion: see the class
         * warning above and ticket #2014. `GetBytes(u8"é")` returns `c3 a9`, not `e9`.
         */
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& s) const override {
            std::vector<SharpRuntime::bytecs> result;
            result.reserve(s.size());
            for (unsigned char c : s)
                result.push_back(static_cast<SharpRuntime::bytecs>(c));
            return result;
        }

        /**
         * Copies the byte range through unchanged as if it were already UTF-8.
         *
         * Despite the class name this is **not** an ISO-8859-1 conversion: see the class
         * warning above and ticket #2014. A byte in 0x80–0xFF is emitted verbatim, which can
         * make the returned string ill-formed UTF-8.
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
