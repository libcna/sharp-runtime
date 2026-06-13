// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <cstdint>

namespace System::Globalization {

/// <summary>Provides information about the version of Unicode used to compare and order strings.</summary>
class SortVersion {
    int fullVersion_;
    std::array<uint8_t, 16> sortId_ = {};

public:
    /// Constructs a SortVersion with the given @p fullVersion and a zeroed sort ID.
    explicit SortVersion(int fullVersion)
        : fullVersion_(fullVersion) {}
    /// Constructs a SortVersion with the given @p fullVersion and @p sortId.
    SortVersion(int fullVersion, const std::array<uint8_t, 16>& sortId)
        : fullVersion_(fullVersion), sortId_(sortId) {}

    /// @return The full version number of the sort tables.
    [[nodiscard]] int getFullVersionProperty() const { return fullVersion_; }
    /// @return The globally unique identifier for the sort tables.
    [[nodiscard]] const std::array<uint8_t, 16>& getSortIdProperty() const { return sortId_; }

    /// @return True if both @p fullVersion and @p sortId are equal.
    bool operator==(const SortVersion& o) const {
        return fullVersion_ == o.fullVersion_ && sortId_ == o.sortId_;
    }
    /// @return True if either @p fullVersion or @p sortId differs.
    bool operator!=(const SortVersion& o) const { return !(*this == o); }
};

} // namespace System::Globalization
