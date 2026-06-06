// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <vector>
#include "System/Text/RegularExpressions/Match.hpp"

namespace System::Text::RegularExpressions {

    /**
     * @brief Represents the set of successful matches found by iteratively applying a regular expression.
     *
     * Partial C++ counterpart of .NET System.Text.RegularExpressions.MatchCollection.
     *
     * @note Status: Implemented
     */
    class MatchCollection {
        std::vector<Match> matches_;
    public:
        MatchCollection() = default;
        explicit MatchCollection(std::vector<Match> m) : matches_(std::move(m)) {}

        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(matches_.size()); }
        [[nodiscard]] const Match& operator[](intcs i) const { return matches_[i]; }

        auto begin() const { return matches_.begin(); }
        auto end()   const { return matches_.end(); }
    };

} // namespace System::Text::RegularExpressions
