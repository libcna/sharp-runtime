// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstring>
#include <string>
#include <type_traits>
#include "System/ArgumentNullException.hpp"
#include "System/Span.hpp"

namespace System {

    /**
     * @brief Provides a handler for formatting interpolated strings into a character span.
     *
     * C++ counterpart of .NET System.MemoryExtensions.TryWriteInterpolatedStringHandler.
     * In .NET this is a compiler-generated ref struct; here it is a regular class used
     * manually to build formatted output into a fixed-size char buffer.
     *
     * Usage:
     * @code
     *   char buf[64]; std::size_t written = 0;
     *   TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
     *   h.AppendLiteral("x=");
     *   h.AppendFormatted(42);
     *   written = h.getCharsWrittenProperty();  // "x=42" in buf
     * @endcode
     */
    class TryWriteInterpolatedStringHandler {
    public:
        /**
         * @brief Constructs a handler that writes into the given character buffer.
         *
         * A null @p destination is accepted only when @p destLen is zero, which is
         * this port's spelling of an empty destination; a null paired with a claimed
         * positive capacity is a programming error and is rejected here rather than
         * carried to the first write. See requireUsableDestination().
         *
         * @param destination Pointer to the output buffer.
         * @param destLen     Maximum number of characters the buffer can hold (excluding NUL).
         * @param literalLength Hint: total characters expected from literals (used to pre-check).
         * @param shouldAppend On return, true if the buffer is large enough for the literal hint.
         * @throws System::ArgumentNullException if @p destination is null and @p destLen
         *         is not zero. Note this is deliberately an exception rather than
         *         `shouldAppend = false`: that output reports insufficient *capacity*,
         *         and a destination that does not exist is a different failure.
         */
        TryWriteInterpolatedStringHandler(char* destination, std::size_t destLen,
                                          std::size_t literalLength, bool& shouldAppend)
            : dest_(requireUsableDestination(destination, destLen)),
              destLen_(destLen), pos_(0), success_(true) {
            shouldAppend = success_ = (destLen >= literalLength);
        }

        /**
         * @brief Constructs a handler without a literal-length pre-check.
         * @param destination Pointer to the output buffer.
         * @param destLen     Maximum number of characters the buffer can hold.
         * @throws System::ArgumentNullException if @p destination is null and @p destLen
         *         is not zero.
         */
        TryWriteInterpolatedStringHandler(char* destination, std::size_t destLen)
            : dest_(requireUsableDestination(destination, destLen)),
              destLen_(destLen), pos_(0), success_(true) {}

        // ---------------------------------------------------------------

        /**
         * @brief Appends a string literal into the buffer.
         * @param value The literal to append.
         * @return true if the value fit; false if the buffer is full.
         */
        bool AppendLiteral(const std::string& value) {
            return appendRaw(value.data(), value.size());
        }

        /**
         * @brief C-string overload; see the std::string overload above.
         *
         * A null @p value is rejected rather than treated as an empty literal. In .NET
         * this handler is compiler-generated and `AppendLiteral` receives only literal
         * text, so there is no .NET behaviour to copy and the policy is decided here:
         * the `std::string` overload cannot be null, so `""` is already the way to
         * spell an empty literal, and the `bool` result already means "did it fit".
         * Silently succeeding on null would give that result a second meaning and hide
         * the caller's bug. Ticket #1810 / SR-AUD-132, whose closing sentence asked for
         * exactly this policy to be defined.
         *
         * @param value The literal to append. Must not be null.
         * @return true if the value fit; false if the buffer is full.
         * @throws System::ArgumentNullException if @p value is null.
         */
        bool AppendLiteral(const char* value) {
            if (value == nullptr) throw System::ArgumentNullException("value");
            return appendRaw(value, std::strlen(value));
        }

        /**
         * @brief Appends a formatted value into the buffer.
         * Converts via std::to_string (numeric types) or falls back to
         * the value's ToString() if it inherits from IFormattable.
         */
        template<typename T>
        bool AppendFormatted(const T& value) {
            return AppendLiteral(formatValue(value));
        }

        /**
         * @brief Appends a formatted value with an explicit format string.
         * Format string is currently ignored (stub — passes through to AppendFormatted<T>).
         */
        template<typename T>
        bool AppendFormatted(const T& value, const std::string& /*format*/) {
            return AppendFormatted(value);
        }

        // ---------------------------------------------------------------

        /** @brief Returns the number of characters written so far. */
        [[nodiscard]] std::size_t getCharsWrittenProperty() const { return pos_; }

        /** @brief Returns true if all append operations succeeded. */
        [[nodiscard]] bool getSuccessProperty() const { return success_; }

        /** @brief Returns a std::string containing the characters written so far. */
        [[nodiscard]] std::string getString() const {
            // dest_ can still be null, but only for a zero-capacity handler, where pos_
            // is necessarily 0. std::string(const char*, size_type) requires a valid
            // range, and [nullptr, nullptr) is not one even though the count is zero.
            if (!success_ || dest_ == nullptr) return std::string{};
            return std::string(dest_, pos_);
        }

    private:
        char*       dest_;
        std::size_t destLen_;
        std::size_t pos_;
        bool        success_;

        /**
         * @brief Rejects a null destination that claims a nonzero capacity.
         *
         * The .NET counterpart takes a `Span<char>`, which cannot represent a nonempty
         * null destination at all, so this check restores by validation what the .NET
         * type gets from its parameter type. Before ticket #1810 the pointer and length
         * were stored unchecked: `TryWriteInterpolatedStringHandler(nullptr, 1)` passed
         * the capacity check in appendRaw() and reached
         * `std::memcpy(dest_ + pos_, ...)`, an AddressSanitizer-confirmed **write** to
         * address zero, plus a UBSan "null pointer passed as argument 1, which is
         * declared to never be null" (build-probe/1810_prefix_defects.log cases 1-2).
         *
         * A null paired with a capacity of ZERO stays valid: it is this port's spelling
         * of an empty destination, it already behaved correctly (case 3 refuses every
         * append and reports success=0), and it is the rule tickets #1774 and #1805
         * settled for the same pointer/length shape elsewhere in this repository.
         */
        static char* requireUsableDestination(char* destination, std::size_t destLen) {
            if (destination == nullptr && destLen != 0)
                throw System::ArgumentNullException("destination");
            return destination;
        }

        bool appendRaw(const char* data, std::size_t len) {
            if (!success_) return false;
            // Written as a subtraction, not as `pos_ + len > destLen_`: pos_ and len are
            // both size_t, so the sum can wrap and let an oversized append pass the very
            // check meant to stop it. pos_ <= destLen_ is an invariant of this class --
            // pos_ only ever advances by a len this test has already accepted -- so the
            // right-hand side cannot underflow.
            if (len > destLen_ - pos_) { success_ = false; return false; }
            // memcpy is undefined for a null pointer even with a length of zero, and both
            // ends can legitimately be null here: dest_ for a zero-capacity handler, data
            // for an empty std::string.
            if (len == 0) return true;
            std::memcpy(dest_ + pos_, data, len);
            pos_ += len;
            return true;
        }

        template<typename T>
        static std::string formatValue(const T& v) {
            if constexpr (std::is_same_v<T, std::string>) return v;
            else if constexpr (std::is_same_v<T, const char*>) return std::string(v);
            else if constexpr (std::is_same_v<T, char>) return std::string(1, v);
            else if constexpr (std::is_arithmetic_v<T>) return std::to_string(v);
            else return std::string("[?]");
        }
    };

} // namespace System
