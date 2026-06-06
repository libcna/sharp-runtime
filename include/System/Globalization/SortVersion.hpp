// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <cstdint>

namespace System::Globalization {

    class SortVersion {
        int fullVersion_;
        std::array<uint8_t, 16> sortId_ = {};

    public:
        explicit SortVersion(int fullVersion)
            : fullVersion_(fullVersion) {}
        SortVersion(int fullVersion, const std::array<uint8_t, 16>& sortId)
            : fullVersion_(fullVersion), sortId_(sortId) {}

        [[nodiscard]] int getFullVersionProperty() const { return fullVersion_; }
        [[nodiscard]] const std::array<uint8_t, 16>& getSortIdProperty() const { return sortId_; }

        bool operator==(const SortVersion& o) const {
            return fullVersion_ == o.fullVersion_ && sortId_ == o.sortId_;
        }
        bool operator!=(const SortVersion& o) const { return !(*this == o); }
    };

} // namespace System::Globalization
