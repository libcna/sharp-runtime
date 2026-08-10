// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IndexOutOfRangeException.hpp"
#include "System/String.hpp"

namespace System::Text
{
    using SharpRuntime::intcs;

    class StringBuilderRuneEnumerator;

    /**
     * <summary>
     * Provides a mutable string buffer for efficient string construction.
     * 
     * Lightweight C++ emulation of .NET System.Text.StringBuilder, intended
     * primarily for source-porting convenience in the SharpRuntime layer.
     * Only a practical subset of the original .NET API is provided.
     * </summary>
     *
     * @note **Every length, index and count on this type is a UTF-8 storage-byte position,
     *       not a managed character position.** The buffer is a `std::string` holding UTF-8,
     *       where .NET's `StringBuilder` holds UTF-16 `char`s. Two measured consequences a
     *       caller must know about:
     *       - `StringBuilder(u8"éA").getLengthProperty()` is **3**, not 2, because
     *         U+00E9 occupies two bytes;
     *       - an index-taking mutation can therefore **split a character**:
     *         `Remove(1, 1)` on that same builder yields the byte pair `c3 41`, which is not
     *         well-formed UTF-8.
     *
     *       Ticket #2012 (SR-AUD-296) states this rather than leaving the member
     *       doc-comments promising characters. Changing the unit — or making the mutations
     *       character-preserving — is the approval-gated ticket **#2015**
     *       (`docs/SystemTextNamespaceReviewPlan.md` §14.3), and is not done here. Callers
     *       that need scalar-wise iteration have `StringBuilderRuneEnumerator`.
     */
    class StringBuilder
    {
    private:
        std::string buffer; ///< Internal text buffer.

    public:
        /** Initializes a new empty instance of the StringBuilder class. */
        StringBuilder();

        /**
         * Initializes a new instance of the StringBuilder class with the specified initial text.
         * @param value Initial text.
         */
        explicit StringBuilder(const std::string& value);

        /** Removes all characters from the current instance. */
        void Clear();

        /**
         * Appends the specified string to this instance.
         * @param value String to append.
         * @return Reference to this instance.
         */
        StringBuilder& Append(const std::string& value);

        /**
         * Appends the specified null-terminated C string to this instance.
         * If @p value is nullptr, nothing is appended.
         * @param value C string to append.
         * @return Reference to this instance.
         */
        StringBuilder& Append(const char* value);

        /**
         * Appends the specified character to this instance.
         * @param value Character to append.
         * @return Reference to this instance.
         */
        StringBuilder& Append(char value);

        /** Appends the specified character @p repeatCount times to this instance. */
        StringBuilder& Append(char value, intcs repeatCount);

        /**
         * Appends the string representation of the specified integer value.
         * @param value Integer value to append.
         * @return Reference to this instance.
         */
        StringBuilder& Append(intcs value);

        /**
         * Appends the string representation of the specified double value.
         * @param value Double value to append.
         * @return Reference to this instance.
         */
        StringBuilder& Append(double value);

        /**
         * Appends the string representation of the specified float value.
         * @param value Float value to append.
         * @return Reference to this instance.
         */
        StringBuilder& Append(float value);

        /**
         * Appends the string representation of the specified boolean value.
         * The appended text is "True" or "False" to match .NET behavior.
         * @param value Boolean value to append.
         * @return Reference to this instance.
         */
        StringBuilder& Append(bool value);

        /**
         * Appends a line terminator (newline) to this instance.
         * @return Reference to this instance.
         */
        StringBuilder& AppendLine();

        /**
         * Appends the specified string followed by a line terminator.
         * @param value String to append.
         * @return Reference to this instance.
         */
        StringBuilder& AppendLine(const std::string& value);

        /**
         * Returns the current contents of this instance as a string.
         * @return The accumulated string.
         */
        [[nodiscard]] std::string ToString() const;

        /**
         * Gets the number of UTF-8 **bytes** in this instance (.NET Length property).
         * @return Number of bytes in the internal buffer -- not the number of managed
         *         characters. `StringBuilder(u8"\u00e9A").getLengthProperty()` is 3.
         *         Ticket #2012 (SR-AUD-296); changing the unit is the gated ticket #2015.
         */
        [[nodiscard]] intcs getLengthProperty() const;

        /** Sets the length of the current instance; truncates or pads with null characters. */
        void setLengthProperty(intcs value);

        /** Returns true if the internal buffer is empty. */
        [[nodiscard]] bool Empty() const;

        /**
         * Copies @p count characters starting at @p sourceIndex into @p destination starting
         * at @p destinationIndex.
         *
         * C++ counterpart of .NET StringBuilder.CopyTo(int, char[], int, int). @p destinationLength
         * is the caller-supplied total capacity of @p destination, used to validate the copy
         * fits (real .NET can infer array length from the CLR array object itself; a raw C++
         * pointer carries no length, so this port requires the caller to state it explicitly).
         * @param sourceIndex Zero-based index in this instance at which copying begins.
         * @param destination Pointer to the destination char buffer.
         * @param destinationLength Total capacity of @p destination.
         * @param destinationIndex Zero-based index in @p destination at which copying begins.
         * @param count Number of characters to copy.
         * @throws System::ArgumentNullException if @p destination is null.
         * @throws System::ArgumentOutOfRangeException if @p count, @p sourceIndex,
         *         @p destinationIndex or @p destinationLength is negative, or @p sourceIndex
         *         is greater than getLengthProperty(). Ticket #2009 (SR-AUD-295) added the
         *         @p destinationLength check: without it the guard below subtracted from an
         *         unvalidated signed capacity, and the resulting overflow *defeated* the
         *         bounds check rather than merely accompanying it.
         * @throws System::ArgumentException if @p sourceIndex + count exceeds
         *         getLengthProperty(), or @p destinationIndex + count exceeds
         *         @p destinationLength.
         */
        void CopyTo(intcs sourceIndex, char* destination, intcs destinationLength,
                    intcs destinationIndex, intcs count) const;

        /**
         * Appends the string representation of the specified 64-bit integer value.
         * @param value Long integer value to append.
         * @return Reference to this instance.
         */
        StringBuilder& Append(SharpRuntime::longcs value);

        /**
         * Inserts the specified string at the given UTF-8 **byte** position.
         * @param index Zero-based BYTE position at which to insert. An index in the middle of
         *        a multi-byte sequence splits the character, producing ill-formed UTF-8;
         *        this is checked against the buffer's byte length, not its character count.
         * @param value String to insert.
         * @return Reference to this instance.
         * @note Ticket #2012 (SR-AUD-296); making the position character-preserving is the
         *       gated ticket #2015 (docs/SystemTextNamespaceReviewPlan.md section 14.3).
         */
        StringBuilder& Insert(intcs index, const std::string& value);

        /**
         * Removes a range of UTF-8 **bytes** from this instance.
         * @param startIndex Zero-based BYTE position of the first byte to remove.
         * @param count Number of BYTES to remove.
         * @return Reference to this instance.
         * @note Measured: `StringBuilder(u8"\u00e9A").Remove(1, 1)` leaves the byte pair
         *       `c3 41`, which is not well-formed UTF-8, because the range is in bytes.
         *       Ticket #2012 (SR-AUD-296); the gated repair is #2015.
         */
        StringBuilder& Remove(intcs startIndex, intcs count);

        /**
         * Replaces all occurrences of @p oldValue with @p newValue.
         * @param oldValue The string to replace.
         * @param newValue The replacement string.
         * @return Reference to this instance.
         */
        StringBuilder& Replace(const std::string& oldValue, const std::string& newValue);

        /** @brief Appends a formatted string (delegates to String::Format overloads). */
        StringBuilder& AppendFormat(const std::string& format, SharpRuntime::intcs arg0)
            { return Append(System::String::Format(format, arg0)); }
        /** @brief Appends a formatted string with a double argument. */
        StringBuilder& AppendFormat(const std::string& format, double arg0)
            { return Append(System::String::Format(format, arg0)); }
        /** @brief Appends a formatted string with a string argument. */
        StringBuilder& AppendFormat(const std::string& format, const std::string& arg0)
            { return Append(System::String::Format(format, arg0)); }
        /** @brief Appends a formatted string with two integer arguments. */
        StringBuilder& AppendFormat(const std::string& format, SharpRuntime::intcs arg0, SharpRuntime::intcs arg1)
            { return Append(System::String::Format(format, arg0, arg1)); }
        /** @brief Appends a formatted string with an integer and a string argument. */
        StringBuilder& AppendFormat(const std::string& format, SharpRuntime::intcs arg0, const std::string& arg1)
            { return Append(System::String::Format(format, arg0, arg1)); }
        /** @brief Appends a formatted string with a string and an integer argument. */
        StringBuilder& AppendFormat(const std::string& format, const std::string& arg0, SharpRuntime::intcs arg1)
            { return Append(System::String::Format(format, arg0, arg1)); }
        /** @brief Appends a formatted string with two string arguments. */
        StringBuilder& AppendFormat(const std::string& format, const std::string& arg0, const std::string& arg1)
            { return Append(System::String::Format(format, arg0, arg1)); }
        /** @brief Appends a formatted string with two double arguments. */
        StringBuilder& AppendFormat(const std::string& format, double arg0, double arg1)
            { return Append(System::String::Format(format, arg0, arg1)); }
        /** @brief Appends a formatted string with a long integer argument. */
        StringBuilder& AppendFormat(const std::string& format, SharpRuntime::longcs arg0)
            { return Append(System::String::Format(format, arg0)); }

        /** @brief Appends elements of @p values joined by @p separator. */
        StringBuilder& AppendJoin(const std::string& separator, const std::vector<std::string>& values)
            { return Append(System::String::Join(separator, values)); }

        /** @brief Appends a formatted string with three integer arguments. */
        StringBuilder& AppendFormat(const std::string& format, intcs arg0, intcs arg1, intcs arg2)
            { return Append(System::String::Format(format, arg0, arg1, arg2)); }
        /** @brief Appends a formatted string with three string arguments. */
        StringBuilder& AppendFormat(const std::string& format,
                                    const std::string& arg0, const std::string& arg1, const std::string& arg2)
            { return Append(System::String::Format(format, arg0, arg1, arg2)); }

        /** @brief Appends the contents of another StringBuilder to this instance. */
        StringBuilder& Append(const StringBuilder& other) { return Append(other.buffer); }

        /** @brief Returns true if the contents of this instance equal those of @p other. */
        [[nodiscard]] bool Equals(const StringBuilder& other) const { return buffer == other.buffer; }

        /** @brief Returns the current capacity of the internal buffer. */
        [[nodiscard]] intcs getCapacityProperty() const { return static_cast<intcs>(buffer.capacity()); }

        /**
         * @brief Ensures the internal buffer has at least @p capacity characters reserved.
         * @throws System::ArgumentOutOfRangeException if @p capacity is negative, matching
         *         StringBuilder.cs's EnsureCapacity(int) (ArgumentOutOfRangeException.
         *         ThrowIfNegative). Without this check, a negative capacity wraps to a huge
         *         size_t and std::string::reserve throws a raw std::length_error instead.
         */
        void EnsureCapacity(intcs capacity) {
            if (capacity < 0)
                throw System::ArgumentOutOfRangeException("capacity", "Non-negative number required.");
            buffer.reserve(static_cast<std::size_t>(capacity));
        }

        /**
         * @brief Returns the character at @p index (.NET's indexer).
         * @throws System::IndexOutOfRangeException if @p index is negative or not less than
         *         getLengthProperty() -- matching StringBuilder.cs, whose indexer throws
         *         IndexOutOfRangeException rather than silently invoking undefined behavior
         *         the way plain std::string::operator[] does for an out-of-range index.
         */
        [[nodiscard]] char operator[](intcs index) const {
            if (index < 0 || static_cast<std::size_t>(index) >= buffer.size())
                throw System::IndexOutOfRangeException();
            return buffer[static_cast<std::size_t>(index)];
        }
        /**
         * @brief Returns a mutable reference to the character at @p index (.NET's indexer).
         * @throws System::IndexOutOfRangeException if @p index is negative or not less than
         *         getLengthProperty().
         */
        char& operator[](intcs index) {
            if (index < 0 || static_cast<std::size_t>(index) >= buffer.size())
                throw System::IndexOutOfRangeException();
            return buffer[static_cast<std::size_t>(index)];
        }

        /**
         * @brief Enumerates the chunks of characters that make up this instance's content.
         *
         * C++ counterpart of .NET System.Text.StringBuilder.ChunkEnumerator.
         *
         * @note Reduced scope: .NET's real `StringBuilder` is a linked list of fixed-size
         * "chunks", and `ChunkEnumerator` walks that list without copying. This runtime's
         * `StringBuilder` is backed by a single `std::string`, so this enumerator always yields
         * exactly one chunk containing the entire current content.
         */
        class ChunkEnumerator {
            struct Sentinel {};
            const std::string* chunk_;
            bool consumed_ = false;
            bool hasCurrent_ = false;

        public:
            /** @brief Initializes an enumerator that will yield @p chunk as its single chunk. */
            explicit ChunkEnumerator(const std::string& chunk) : chunk_(&chunk) {}

            /** @return The current chunk. */
            [[nodiscard]] const std::string& getCurrentProperty() const { return *chunk_; }

            /** @brief Advances to the next chunk. @return false after the single chunk has been yielded once. */
            bool MoveNext() {
                if (consumed_) return false;
                consumed_ = true;
                return true;
            }

            /** @brief Range-based-for support: advances to the first (only) chunk and returns *this. */
            [[nodiscard]] ChunkEnumerator& begin() {
                hasCurrent_ = MoveNext();
                return *this;
            }
            /** @brief Range-based-for support: returns the end sentinel. */
            [[nodiscard]] Sentinel end() const { return Sentinel{}; }
            bool operator!=(Sentinel) const { return hasCurrent_; }
            [[nodiscard]] const std::string& operator*() const { return *chunk_; }
            ChunkEnumerator& operator++() {
                hasCurrent_ = MoveNext();
                return *this;
            }
        };

        /** @brief Returns an enumerator over the chunks of characters in this instance. */
        [[nodiscard]] ChunkEnumerator GetChunks() const { return ChunkEnumerator(buffer); }

        /** @brief Returns an enumerator over the Rune values in this instance's current contents. */
        [[nodiscard]] StringBuilderRuneEnumerator EnumerateRunes() const;
    };
}
