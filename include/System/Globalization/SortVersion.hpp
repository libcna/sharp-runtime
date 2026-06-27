// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <cstdint>

namespace System::Globalization {

/**
 * @brief Provides information about the version of Unicode used to compare and order strings.
 *
 * C++ counterpart of .NET System.Globalization.SortVersion.
 * Used to ensure that string comparisons are consistent across versions of the runtime
 * and Unicode tables.
 */
class SortVersion {
    int fullVersion_;
    std::array<uint8_t, 16> sortId_ = {};

public:
    /**
     * @brief Constructs a SortVersion with the given full version and a zeroed sort ID.
     *
     * C++ counterpart of .NET SortVersion(int, Guid) — Guid defaults to all zeros.
     * @param fullVersion The full numeric version of the sort tables.
     */
    explicit SortVersion(int fullVersion)
        : fullVersion_(fullVersion) {}

    /**
     * @brief Constructs a SortVersion with the given full version and sort ID.
     *
     * C++ counterpart of .NET SortVersion(int, Guid).
     * @param fullVersion The full numeric version of the sort tables.
     * @param sortId      A 16-byte identifier for the sort tables.
     */
    SortVersion(int fullVersion, const std::array<uint8_t, 16>& sortId)
        : fullVersion_(fullVersion), sortId_(sortId) {}

    /**
     * @brief Gets the full version number of the sort tables.
     *
     * C++ counterpart of .NET SortVersion.FullVersion.
     * @return The full version integer.
     */
    [[nodiscard]] int getFullVersionProperty() const { return fullVersion_; }

    /**
     * @brief Gets the globally unique identifier for the sort tables.
     *
     * C++ counterpart of .NET SortVersion.SortId (mapped from Guid to 16-byte array).
     * @return A const reference to the 16-byte sort ID.
     */
    [[nodiscard]] const std::array<uint8_t, 16>& getSortIdProperty() const { return sortId_; }

    /**
     * @brief Returns true if both SortVersion instances have equal version and sort ID.
     *
     * C++ counterpart of .NET SortVersion.Equals(object).
     * @param o The SortVersion to compare.
     * @return true if both fullVersion and sortId are equal.
     */
    bool operator==(const SortVersion& o) const {
        return fullVersion_ == o.fullVersion_ && sortId_ == o.sortId_;
    }

    /**
     * @brief Returns true if either the version or sort ID differs.
     *
     * C++ counterpart of .NET SortVersion inequality.
     * @param o The SortVersion to compare.
     * @return true if the instances differ.
     */
    bool operator!=(const SortVersion& o) const { return !(*this == o); }
};

} // namespace System::Globalization
