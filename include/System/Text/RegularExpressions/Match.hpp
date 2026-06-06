// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include <regex>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Text::RegularExpressions {

    using SharpRuntime::intcs;

    /**
     * @brief Represents the results from a single regular expression match.
     *
     * Partial C++ counterpart of .NET System.Text.RegularExpressions.Match.
     *
     * @note Status: Partial
     */
    class Match {
        std::smatch match_;
        bool success_;
    public:
        Match() : success_(false) {}
        explicit Match(const std::smatch& m) : match_(m), success_(m.ready() && !m.empty()) {}

        [[nodiscard]] bool getSuccessProperty() const { return success_; }
        [[nodiscard]] std::string getValueProperty() const { return success_ ? match_[0].str() : ""; }
        [[nodiscard]] intcs getIndexProperty() const  { return success_ ? static_cast<intcs>(match_.position(0)) : -1; }
        [[nodiscard]] intcs getLengthProperty() const { return success_ ? static_cast<intcs>(match_[0].length()) : 0; }

        [[nodiscard]] std::string Group(intcs index) const {
            if (!success_ || index >= static_cast<intcs>(match_.size())) return "";
            return match_[index].str();
        }

        static const Match& Empty() { static Match m; return m; }
    };

} // namespace System::Text::RegularExpressions
