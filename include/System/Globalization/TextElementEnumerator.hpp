// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Globalization {

using SharpRuntime::intcs;

/** <summary>Enumerates the text elements of a string, where each element may be one or more chars (grapheme cluster).</summary> */
class TextElementEnumerator {
public:
    explicit TextElementEnumerator(const std::string& str)
        : _str(str), _pos(0), _elementIndex(-1), _current{} {}

    bool MoveNext() {
        if (_pos >= _str.size()) return false;
        _elementIndex = static_cast<intcs>(_pos);
        // Simple: advance by one UTF-8 sequence (handle multi-byte)
        unsigned char c = static_cast<unsigned char>(_str[_pos]);
        size_t len = 1;
        if ((c & 0x80) == 0)       len = 1;
        else if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        _current = _str.substr(_pos, std::min(len, _str.size() - _pos));
        _pos += len;
        return true;
    }

    /** <summary>Returns the current text element as a string.</summary> */
    std::string GetTextElement() const {
        if (_elementIndex < 0) throw std::runtime_error("Enumeration not started");
        return _current;
    }

    /** <summary>Gets the current text element.</summary> */
    std::string getCurrent() const { return GetTextElement(); }

    /** <summary>Gets the index of the current text element in the original string.</summary> */
    intcs getElementIndexProperty() const { return _elementIndex; }

    void Reset() { _pos = 0; _elementIndex = -1; _current.clear(); }

private:
    std::string _str;
    size_t _pos;
    intcs _elementIndex;
    std::string _current;
};

} // namespace System::Globalization
