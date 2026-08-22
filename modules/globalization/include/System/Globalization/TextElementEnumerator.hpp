// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/detail/Utf8Scalar.hpp"

namespace System::Globalization {

using SharpRuntime::intcs;

namespace detail {

/**
 * @brief Byte length of one scalar-based text element in this module's documented subset.
 *
 * Valid UTF-8 is advanced one Unicode scalar at a time. A malformed byte is one element of
 * length one, so no caller can swallow adjacent input or return a dangling continuation byte.
 * Full extended-grapheme clustering remains outside this module's declared subset.
 */
[[nodiscard]] inline std::size_t Utf8TextElementLength(const std::string& text,
                                                       std::size_t offset) noexcept {
    std::uint32_t codePoint = 0;
    std::size_t length = 0;
    return System::detail::TryDecodeUtf8Scalar(text, offset, codePoint, length) ? length : 1;
}

/** True when @p offset is one of the scalar/malformed-byte boundaries produced above. */
[[nodiscard]] inline bool IsUtf8TextElementBoundary(const std::string& text,
                                                    std::size_t offset) noexcept {
    if (offset > text.size()) return false;
    std::size_t current = 0;
    while (current < offset) current += Utf8TextElementLength(text, current);
    return current == offset;
}

} // namespace detail

/**
 * @brief Enumerates the text elements of a string.
 *
 * C++ counterpart of .NET System.Globalization.TextElementEnumerator.
 * This practical-subset implementation advances by Unicode scalar values encoded as UTF-8:
 * ASCII scalars produce single-byte elements, valid multi-byte scalars are kept together, and a
 * malformed byte is exposed as a one-byte element. Extended-grapheme clustering (combining
 * sequences, emoji ZWJ sequences and regional-indicator pairs) is intentionally not performed.
 */
class TextElementEnumerator {
public:
    /**
     * @brief Constructs a TextElementEnumerator over the given string.
     *
     * C++ counterpart of .NET StringInfo.GetTextElementEnumerator(string).
     * @param str The string to enumerate.
     */
    explicit TextElementEnumerator(const std::string& str) : TextElementEnumerator(str, 0) {}

    /**
     * @brief Constructs an enumerator whose first MoveNext begins at a UTF-8 byte boundary.
     * @param str The complete original string, retained so ElementIndex remains an original-string
     *            byte index.
     * @param startIndex The byte boundary of the first element, or str.size() for an empty range.
     * @throws System::ArgumentOutOfRangeException if @p startIndex is negative, beyond the string,
     *         or in the middle of a valid UTF-8 scalar.
     */
    TextElementEnumerator(const std::string& str, intcs startIndex) : str_(str) {
        if (startIndex < 0 || static_cast<std::size_t>(startIndex) > str_.size() ||
            !detail::IsUtf8TextElementBoundary(str_, static_cast<std::size_t>(startIndex))) {
            throw System::ArgumentOutOfRangeException("startIndex");
        }
        initialOffset_ = static_cast<std::size_t>(startIndex);
        Reset();
    }

    /**
     * @brief Advances the enumerator to the next text element.
     *
     * C++ counterpart of .NET TextElementEnumerator.MoveNext().
     * @return true if there is a next element; false if enumeration is complete.
     */
    bool MoveNext() {
        long long newOffset = offset_ + length_;
        offset_ = newOffset;
        length_ = 0;
        if (newOffset < 0 || static_cast<size_t>(newOffset) >= str_.size()) return false;

        const size_t off = static_cast<size_t>(newOffset);
        length_ = static_cast<long long>(detail::Utf8TextElementLength(str_, off));
        return true;
    }

    /**
     * @brief Returns the current text element as a string.
     *
     * C++ counterpart of .NET TextElementEnumerator.GetTextElement().
     * @return The current text element.
     * @throws System::InvalidOperationException if enumeration has either not started or has
     *         already finished (MoveNext() has not been called, or has returned false).
     */
    [[nodiscard]] std::string GetTextElement() const {
        VerifyStarted();
        return str_.substr(static_cast<size_t>(offset_), static_cast<size_t>(length_));
    }

    /**
     * @brief Gets the current text element.
     *
     * C++ counterpart of .NET TextElementEnumerator.Current.
     * @return The current text element string.
     */
    [[nodiscard]] std::string getCurrentProperty() const { return GetTextElement(); }

    /**
     * @brief Gets the index of the current text element in the original string.
     *
     * C++ counterpart of .NET TextElementEnumerator.ElementIndex.
     * @return The zero-based byte index of the current element.
     * @throws System::InvalidOperationException if enumeration has either not started or has
     *         already finished.
     */
    [[nodiscard]] intcs getElementIndexProperty() const {
        VerifyStarted();
        return static_cast<intcs>(offset_);
    }

    /**
     * @brief Resets the enumerator to the beginning of the string.
     *
     * C++ counterpart of .NET TextElementEnumerator.Reset().
     */
    void Reset() {
        // offset_ starts at str_.size() (out of range) and length_ is set so that
        // offset_ + length_ == initialOffset_, meaning the first MoveNext() begins there.
        // This mirrors .NET's TextElementEnumerator.Reset(), which relies on signed-int
        // wraparound the same way. Also ensures GetTextElement()/ElementIndex throw until
        // MoveNext() runs.
        offset_ = static_cast<long long>(str_.size());
        length_ = static_cast<long long>(initialOffset_) - offset_;
    }

private:
    std::string str_;
    std::size_t initialOffset_ = 0;
    long long offset_ = 0;
    long long length_ = 0;

    void VerifyStarted() const {
        if (offset_ < 0 || static_cast<size_t>(offset_) >= str_.size()) {
            throw System::InvalidOperationException("Enumeration has either not started or has already finished.");
        }
    }
};

} // namespace System::Globalization
