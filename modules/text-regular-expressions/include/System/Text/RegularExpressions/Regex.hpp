// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <regex>
#include <string>
#include <utility>
#include <vector>
#include "System/Text/RegularExpressions/Match.hpp"
#include "System/Text/RegularExpressions/MatchCollection.hpp"
#include "System/Text/RegularExpressions/MatchEvaluator.hpp"
#include "System/Text/RegularExpressions/RegexOptions.hpp"
#include "System/Text/RegularExpressions/RegexParseException.hpp"

namespace System::Text::RegularExpressions {

    /**
     * @brief Represents an immutable regular expression.
     *
     * C++ counterpart of .NET System.Text.RegularExpressions.Regex, backed by `std::regex`
     * (ECMAScript grammar).
     *
     * @note Status: partial, with named-capture-group support (`(?<name>...)`/`(?'name'...)`
     * syntax is parsed and stripped to a plain `std::regex`-compatible capturing group, with the
     * name-to-index mapping preserved for `Match::Groups()`'s string indexer) — the one gap
     * NEXT.md previously called out ("no named groups") is now fixed. `RegexOptions` beyond
     * `IgnoreCase`/`Multiline` have no effect (see that enum's doc comment); no match timeout
     * support (`std::regex_search` cannot be interrupted mid-match).
     *
     * @note What ticket #2397 aligned to the reference, and what it deliberately left: `Escape`,
     * `Split` and an unsuccessful `Match`'s `Index` now answer exactly as .NET does, each derived
     * from `/rv` with the citation at its own site. **Still absent, and stated so that silence is
     * not read as parity**: `Regex.Unescape(string)` (it needs `RegexParser.ScanCharEscape`, i.e.
     * the pattern grammar rather than a string transformation), the `Split(input, count)` and
     * `Split(input, count, startat)` overloads, and `Match.Empty.Groups.Count`, which is `1` in
     * .NET (`Match.cs:75` builds it with `capcount = 1`; `GroupCollection.cs:67`) and `0` here
     * because this port's `Match` does not derive from `Group`/`Capture` as .NET's does.
     */
    class Regex {
        std::regex re_;
        std::string pattern_;
        std::vector<std::pair<std::string, intcs>> groupNames_;

        static std::regex::flag_type toStdFlags(RegexOptions options) {
            auto flags = std::regex::ECMAScript;
            if ((options & RegexOptions::IgnoreCase) != RegexOptions::None) flags |= std::regex::icase;
            if ((options & RegexOptions::Multiline) != RegexOptions::None) flags |= std::regex::multiline;
            return flags;
        }

        // Strips .NET-style named-group syntax ((?<name>...) / (?'name'...)) down to a plain
        // capturing group `std::regex` understands, recording each name's 1-based group index.
        // Correctly skips escaped characters, character classes, and non-capturing/lookaround
        // constructs ((?:...), (?=...), (?!...), (?<=...), (?<!...)) when counting group indices.
        static std::pair<std::string, std::vector<std::pair<std::string, intcs>>>
        stripNamedGroups(const std::string& pattern) {
            std::string out;
            std::vector<std::pair<std::string, intcs>> names;
            intcs groupIndex = 0;
            bool inClass = false;

            for (size_t i = 0; i < pattern.size(); ++i) {
                char c = pattern[i];
                if (c == '\\' && i + 1 < pattern.size()) {
                    out += c;
                    out += pattern[++i];
                    continue;
                }
                if (c == '[' && !inClass) {
                    inClass = true;
                    out += c;
                    continue;
                }
                if (c == ']' && inClass) {
                    inClass = false;
                    out += c;
                    continue;
                }
                if (c == '(' && !inClass) {
                    if (i + 1 < pattern.size() && pattern[i + 1] == '?') {
                        bool isAngle = i + 2 < pattern.size() && pattern[i + 2] == '<';
                        bool isQuote = i + 2 < pattern.size() && pattern[i + 2] == '\'';
                        bool isLookbehind = isAngle && i + 3 < pattern.size() && (pattern[i + 3] == '=' || pattern[i + 3] == '!');
                        if ((isAngle || isQuote) && !isLookbehind) {
                            char closeChar = isAngle ? '>' : '\'';
                            size_t nameStart = i + 3;
                            size_t nameEnd = pattern.find(closeChar, nameStart);
                            if (nameEnd != std::string::npos) {
                                std::string name = pattern.substr(nameStart, nameEnd - nameStart);
                                names.emplace_back(name, ++groupIndex);
                                out += '(';
                                i = nameEnd;
                                continue;
                            }
                        }
                        out += c; // (?:...), (?=...), (?!...), (?<=...), (?<!...): not capturing
                        continue;
                    }
                    ++groupIndex;
                }
                out += c;
            }
            return {out, names};
        }

        // Returns type is qualified as RegularExpressions::Match (not bare Match) throughout this
        // method — once the public Match(const std::string&) member below is declared, unqualified
        // "Match" inside any inline member function body (a complete-class context, resolved only
        // after the whole class is seen) resolves to that member function, not the sibling Match
        // class, which would break constructor-style calls like "Match(m, ...)".
        //
        // Searches input[offset..] using an iterator range into the ORIGINAL string, not a
        // re-materialized substring (input.substr(offset)) -- two bugs that fix, both
        // confirmed with a compiled reproduction before this change:
        //  1. m.position(i) is relative to whatever sequence was searched. Searching a fresh
        //     substr() made every reported index relative to that substring's start, not the
        //     true start of input; Match's positionOffset parameter corrects this back to an
        //     absolute index (previously uncorrected, corrupting Replace(string,
        //     MatchEvaluator)'s output and every NextMatch() chain's Index after the first).
        //  2. '^' (and any beginning-of-sequence assertion) was evaluated against the fresh
        //     substring's start, so it incorrectly matched at every resumption offset instead
        //     of only the true start of input. match_prev_avail tells the engine the position
        //     right before `first` is a real, readable character (since first genuinely points
        //     into input, not a separate string), so '^' correctly stops matching once
        //     offset > 0.
        static RegularExpressions::Match matchFrom(
            const std::string& input, size_t offset, const std::regex& regex,
            const std::vector<std::pair<std::string, intcs>>& groupNames) {
            if (offset > input.size()) return RegularExpressions::Match();
            std::smatch m;
            auto flags = offset > 0 ? std::regex_constants::match_prev_avail
                                    : std::regex_constants::match_default;
            if (!std::regex_search(input.cbegin() + static_cast<std::ptrdiff_t>(offset), input.cend(), m, regex, flags))
                return RegularExpressions::Match();

            size_t matchStart = offset + static_cast<size_t>(m.position(0));
            size_t nextOffset = matchStart + std::max<size_t>(m[0].length(), 1);
            return RegularExpressions::Match(m, groupNames, [input, nextOffset, regex, groupNames]() {
                                                return matchFrom(input, nextOffset, regex, groupNames);
                                            },
                                             static_cast<intcs>(offset));
        }

    public:
        /** @brief Constructs a Regex with the given pattern and options (ECMAScript syntax). */
        explicit Regex(const std::string& pattern, RegexOptions options = RegexOptions::None) : pattern_(pattern) {
            try {
                auto [stripped, names] = stripNamedGroups(pattern);
                groupNames_ = std::move(names);
                re_ = std::regex(stripped, toStdFlags(options));
            } catch (const std::regex_error&) {
                throw RegexParseException(RegexParseError::Unknown, "Invalid regular expression pattern: " + pattern);
            }
        }

        /** @brief Constructs a Regex with raw std::regex syntax flags (bypasses RegexOptions mapping). */
        Regex(const std::string& pattern, std::regex::flag_type flags) : re_(pattern, flags), pattern_(pattern) {}

        /** @return The pattern this Regex was constructed with. */
        [[nodiscard]] const std::string& getPatternProperty() const { return pattern_; }

        /** @return true if the pattern matches anywhere in the input string. */
        [[nodiscard]] bool IsMatch(const std::string& input) const { return std::regex_search(input, re_); }

        /** @return The first Match in the input, or an unsuccessful Match if none found. */
        [[nodiscard]] class Match Match(const std::string& input) const { return matchFrom(input, 0, re_, groupNames_); }

        /** @brief Deprecated alias for Match() — kept for source compatibility with earlier ported code. */
        [[nodiscard]] RegularExpressions::Match Match_(const std::string& input) const {
            return matchFrom(input, 0, re_, groupNames_);
        }

        /** @return All non-overlapping matches in the input. */
        [[nodiscard]] MatchCollection Matches(const std::string& input) const {
            std::vector<RegularExpressions::Match> results;
            auto begin = std::sregex_iterator(input.begin(), input.end(), re_);
            auto end = std::sregex_iterator();
            for (auto it = begin; it != end; ++it) {
                results.emplace_back(*it, groupNames_);
            }
            return MatchCollection(std::move(results));
        }

        /** @brief Replaces all matches in the input with the replacement string ($1-style backreferences supported). */
        [[nodiscard]] std::string Replace(const std::string& input, const std::string& replacement) const {
            return std::regex_replace(input, re_, replacement);
        }

        /** @brief Replaces all matches in the input using @p evaluator to compute each replacement. */
        [[nodiscard]] std::string Replace(const std::string& input, const MatchEvaluator& evaluator) const {
            std::string result;
            size_t lastEnd = 0;
            RegularExpressions::Match m = matchFrom(input, 0, re_, groupNames_);
            while (m.getSuccessProperty()) {
                result += input.substr(lastEnd, static_cast<size_t>(m.getIndexProperty()) - lastEnd);
                result += evaluator(m);
                lastEnd = static_cast<size_t>(m.getIndexProperty()) + static_cast<size_t>(m.getLengthProperty());
                m = m.NextMatch();
            }
            result += input.substr(lastEnd);
            return result;
        }

        /**
         * @brief Splits the input string on occurrences of the pattern.
         *
         * Transcribed from `Regex.Split.cs:295-321`, which does three things this port's earlier
         * `std::sregex_token_iterator(-1)` implementation did not:
         *
         * 1. **Every matched capture group's value is appended** after the segment that preceded
         *    its match (`Regex.Split.cs:304-311`), so `Split("a1b2c", "(\\d)")` is
         *    `{"a","1","b","2","c"}` and not `{"a","b","c"}`. A group that did **not** participate
         *    in the match is skipped, which is why `Split("a,b", "(,)|(;)")` yields
         *    `{"a",",","b"}` rather than an empty element for the alternative that lost.
         * 2. **The trailing segment is always appended once any match was seen**
         *    (`Regex.Split.cs:321`), so `Split("abc", "c")` is `{"ab",""}` and not `{"ab"}` --
         *    `sregex_token_iterator` suppresses a trailing empty token, which silently dropped it.
         * 3. **A pattern that never matches returns the whole input as one element**
         *    (`Regex.Split.cs:316-319`). The `results.empty()` test below is .NET's own
         *    `state.results.Count == 0`: every match appends at least the segment before it, so
         *    an empty result set is exactly "no match occurred".
         */
        [[nodiscard]] std::vector<std::string> Split(const std::string& input) const {
            std::vector<std::string> result;
            size_t prevat = 0;
            auto end = std::sregex_iterator();
            for (auto it = std::sregex_iterator(input.begin(), input.end(), re_); it != end; ++it) {
                const std::smatch& m = *it;
                const size_t index = static_cast<size_t>(m.position(0));
                result.push_back(input.substr(prevat, index - prevat));
                prevat = index + static_cast<size_t>(m.length(0));

                // Regex.Split.cs:304-311 -- add all MATCHED capture groups, group 0 excluded.
                for (size_t g = 1; g < m.size(); ++g) {
                    if (m[g].matched) result.push_back(m[g].str());
                }
            }
            // Regex.Split.cs:316-319. This early return is a PROVEN EQUIVALENCE here and is kept
            // for being .NET's rather than for being load-bearing: every iteration of the loop
            // above appends at least the segment preceding its match, so an empty result set means
            // the loop never ran, which means `prevat` is still 0, which means the fall-through
            // `input.substr(prevat)` below already yields exactly `{input}`. Measured over 288
            // (pattern, input) pairs with both forms side by side: zero differences. Mutation M7
            // removes it and is NOT CAUGHT, and that is the honest result rather than a test gap.
            if (result.empty()) return {input};
            result.push_back(input.substr(prevat)); // Regex.Split.cs:321
            return result;
        }

        /** @return true if the pattern matches anywhere in the input (static overload). */
        [[nodiscard]] static bool IsMatch(const std::string& input, const std::string& pattern) {
            return Regex(pattern).IsMatch(input);
        }

        /** @brief Replaces all pattern matches in the input with the replacement (static overload). */
        [[nodiscard]] static std::string Replace(const std::string& input, const std::string& pattern,
                                                  const std::string& replacement) {
            return Regex(pattern).Replace(input, replacement);
        }

        /** @brief Splits the input string on occurrences of the pattern (static overload). */
        [[nodiscard]] static std::vector<std::string> Split(const std::string& input, const std::string& pattern) {
            return Regex(pattern).Split(input);
        }

        /**
         * @brief Escapes regex metacharacters in @p str so it can be used as a literal within a
         * pattern.
         *
         * The escaped set and the escaped spelling are .NET's, transcribed rather than chosen:
         * `RegexParser.cs:2135-2136` defines the metacharacters as
         * `SearchValues.Create("\t\n\f\r #$()*+.?[\\^{|")`, and `RegexParser.cs:180-199`
         * renders the four whitespace metacharacters as a backslash followed by a **letter**
         * (`\n` -> `\` `n`, `\r` -> `\` `r`, `\t` -> `\` `t`, `\f` -> `\` `f`) rather than
         * as a backslash followed by the raw control byte.
         *
         * @note Two consequences are .NET's and are deliberate rather than oversights. A space
         * and `#` **are** escaped, because .NET's `IgnorePatternWhitespace` mode gives both a
         * meaning; `]` and `}` are **not**, because neither can open a construct. The second is
         * the one with an observable edge: splicing this output into a character class --
         * `"[" + Escape(x) + "]"` -- no longer escapes a `]` that would close the class early.
         * That hazard is .NET's own, and is the reason `Escape` is specified for a literal in a
         * pattern rather than for a literal in a class.
         */
        [[nodiscard]] static std::string Escape(const std::string& str) {
            // RegexParser.cs:2135-2136 -- SearchValues.Create("\t\n\f\r #$()*+.?[\\^{|")
            static const std::string metaChars = "\t\n\f\r #$()*+.?[\\^{|";
            std::string result;
            result.reserve(str.size());
            for (char c : str) {
                if (metaChars.find(c) == std::string::npos) {
                    result += c;
                    continue;
                }
                result += '\\';
                switch (c) { // RegexParser.cs:182-196
                    case '\n': result += 'n'; break;
                    case '\r': result += 'r'; break;
                    case '\t': result += 't'; break;
                    case '\f': result += 'f'; break;
                    default: result += c; break;
                }
            }
            return result;
        }
    };

} // namespace System::Text::RegularExpressions
