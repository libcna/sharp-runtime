// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Globalization {

using SharpRuntime::bytecs;
using SharpRuntime::intcs;

/// <summary>Represents the result of mapping a string to its sort key for culture-sensitive comparison.</summary>
class SortKey {
public:
    SortKey() = default;
    SortKey(const std::string& originalString, const std::vector<bytecs>& keyData)
        : _string(originalString), _keyData(keyData) {}

    /// <summary>Gets the original string used to create this SortKey.</summary>
    [[nodiscard]] const std::string& getOriginalStringProperty() const { return _string; }

    /// <summary>Gets the byte array representing the current SortKey.</summary>
    [[nodiscard]] std::vector<bytecs> getKeyDataProperty() const { return _keyData; }

    /// <summary>Compares two SortKey objects.</summary>
    static intcs Compare(const SortKey& sortkey1, const SortKey& sortkey2) {
        // Lexicographic byte comparison
        size_t n = std::min(sortkey1._keyData.size(), sortkey2._keyData.size());
        for (size_t i = 0; i < n; ++i) {
            if (sortkey1._keyData[i] < sortkey2._keyData[i]) return -1;
            if (sortkey1._keyData[i] > sortkey2._keyData[i]) return  1;
        }
        if (sortkey1._keyData.size() < sortkey2._keyData.size()) return -1;
        if (sortkey1._keyData.size() > sortkey2._keyData.size()) return  1;
        return 0;
    }

    bool operator==(const SortKey& other) const {
        return _string == other._string && _keyData == other._keyData;
    }

    std::string ToString() const { return "SortKey - " + _string; }

private:
    std::string _string;
    std::vector<bytecs> _keyData;
};

} // namespace System::Globalization
