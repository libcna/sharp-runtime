// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Globalization {

using SharpRuntime::intcs;

/**
 * @brief Enumerates the text elements of a string.
 *
 * C++ counterpart of .NET System.Globalization.TextElementEnumerator.
 * Each text element may span one or more chars (grapheme cluster). This implementation
 * advances by UTF-8 code units: ASCII characters produce single-byte elements, and
 * multi-byte sequences are kept together. Combining-character grouping is not performed.
 */
class TextElementEnumerator {
public:
    /**
     * @brief Constructs a TextElementEnumerator over the given string.
     *
     * C++ counterpart of .NET StringInfo.GetTextElementEnumerator(string).
     * @param str The string to enumerate.
     */
    explicit TextElementEnumerator(const std::string& str)
        : str_(str), pos_(0), elementIndex_(-1), current_{} {}

    /**
     * @brief Advances the enumerator to the next text element.
     *
     * C++ counterpart of .NET TextElementEnumerator.MoveNext().
     * @return true if there is a next element; false if enumeration is complete.
     */
    bool MoveNext() {
        if (pos_ >= str_.size()) return false;
        elementIndex_ = static_cast<intcs>(pos_);
        unsigned char c = static_cast<unsigned char>(str_[pos_]);
        size_t len = 1;
        if      ((c & 0x80) == 0x00) len = 1;
        else if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        current_ = str_.substr(pos_, std::min(len, str_.size() - pos_));
        pos_ += len;
        return true;
    }

    /**
     * @brief Returns the current text element as a string.
     *
     * C++ counterpart of .NET TextElementEnumerator.GetTextElement().
     * @return The current text element.
     * @throws std::runtime_error if MoveNext() has not been called.
     */
    [[nodiscard]] std::string GetTextElement() const {
        if (elementIndex_ < 0) throw std::runtime_error("Enumeration not started");
        return current_;
    }

    /**
     * @brief Gets the current text element.
     *
     * C++ counterpart of .NET TextElementEnumerator.Current.
     * @return The current text element string.
     */
    [[nodiscard]] std::string getCurrent() const { return GetTextElement(); }

    /**
     * @brief Gets the index of the current text element in the original string.
     *
     * C++ counterpart of .NET TextElementEnumerator.ElementIndex.
     * @return The zero-based byte index of the current element, or -1 before the first MoveNext().
     */
    [[nodiscard]] intcs getElementIndexProperty() const { return elementIndex_; }

    /**
     * @brief Resets the enumerator to the beginning of the string.
     *
     * C++ counterpart of .NET TextElementEnumerator.Reset().
     */
    void Reset() { pos_ = 0; elementIndex_ = -1; current_.clear(); }

private:
    std::string str_;
    size_t pos_;
    intcs elementIndex_;
    std::string current_;
};

} // namespace System::Globalization
