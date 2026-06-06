// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <regex>
#include <string>
#include "System/Text/RegularExpressions/Match.hpp"
#include "System/Text/RegularExpressions/MatchCollection.hpp"

namespace System::Text::RegularExpressions {

    /**
     * @brief Represents an immutable regular expression.
     *
     * Wraps std::regex. Partial C++ counterpart of .NET System.Text.RegularExpressions.Regex.
     *
     * @note Status: Partial — basic matching/replace; named groups not supported.
     */
    class Regex {
        std::regex re_;
        std::string pattern_;
    public:
        explicit Regex(const std::string& pattern)
            : re_(pattern, std::regex::ECMAScript), pattern_(pattern) {}
        Regex(const std::string& pattern, std::regex::flag_type flags)
            : re_(pattern, flags), pattern_(pattern) {}

        [[nodiscard]] bool IsMatch(const std::string& input) const {
            return std::regex_search(input, re_);
        }

        [[nodiscard]] Match Match_(const std::string& input) const {
            std::smatch m;
            if (std::regex_search(input, m, re_)) return Match(m);
            return Match::Empty();
        }

        [[nodiscard]] MatchCollection Matches(const std::string& input) const {
            std::vector<Match> results;
            auto begin = std::sregex_iterator(input.begin(), input.end(), re_);
            auto end   = std::sregex_iterator();
            for (auto it = begin; it != end; ++it)
                results.emplace_back(*it);
            return MatchCollection(std::move(results));
        }

        [[nodiscard]] std::string Replace(const std::string& input, const std::string& replacement) const {
            return std::regex_replace(input, re_, replacement);
        }

        [[nodiscard]] static bool IsMatch(const std::string& input, const std::string& pattern) {
            return Regex(pattern).IsMatch(input);
        }

        [[nodiscard]] static std::string Replace(const std::string& input,
                                                  const std::string& pattern,
                                                  const std::string& replacement) {
            return Regex(pattern).Replace(input, replacement);
        }

        [[nodiscard]] static std::vector<std::string> Split(const std::string& input, const std::string& pattern) {
            std::vector<std::string> result;
            std::regex re(pattern);
            std::sregex_token_iterator it(input.begin(), input.end(), re, -1), end;
            for (; it != end; ++it) result.push_back(it->str());
            return result;
        }
    };

} // namespace System::Text::RegularExpressions
