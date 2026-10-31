// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <charconv>
#include <compare>
#include <cstddef>
#include <string>
#include <string_view>
#include <system_error>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/FormatException.hpp"
#include "System/OverflowException.hpp"

namespace System {

    using SharpRuntime::intcs;
    using SharpRuntime::shortcs;

    /**
     * @brief Represents a version number with Major, Minor, Build, and Revision components.
     *
     * C++ counterpart of .NET System.Version.
     */
    class Version {
    private:
        intcs Major    = 0;  ///< Major version component.
        intcs Minor    = 0;  ///< Minor version component.
        intcs Build    = -1; ///< Build number; -1 means not specified.
        intcs Revision = -1; ///< Revision number; -1 means not specified.

    public:
        /** @brief Constructs a Version with all components set to their defaults (0.0). */
        constexpr Version() = default;

        /**
         * @brief Constructs a Version with the given major and minor components.
         * @throws System::ArgumentOutOfRangeException if @p major or @p minor is negative.
         */
        constexpr Version(intcs major, intcs minor) : Major(major), Minor(minor) {
            requireNonNegative(major, "major");
            requireNonNegative(minor, "minor");
        }

        /**
         * @brief Constructs a Version with major, minor, and build components.
         * @throws System::ArgumentOutOfRangeException if @p major, @p minor, or @p build is negative.
         */
        constexpr Version(intcs major, intcs minor, intcs build)
            : Major(major), Minor(minor), Build(build) {
            requireNonNegative(major, "major");
            requireNonNegative(minor, "minor");
            requireNonNegative(build, "build");
        }

        /**
         * @brief Constructs a Version with all four components.
         * @throws System::ArgumentOutOfRangeException if any component is negative (unlike the
         *         2-/3-arg overloads, this one validates @p revision too, since it
         *         is explicitly user-supplied here rather than defaulted to -1).
         */
        constexpr Version(intcs major, intcs minor, intcs build, intcs revision)
            : Major(major), Minor(minor), Build(build), Revision(revision) {
            requireNonNegative(major, "major");
            requireNonNegative(minor, "minor");
            requireNonNegative(build, "build");
            requireNonNegative(revision, "revision");
        }

        /** @brief Parses a version from a dot-separated string such as "1.2.3.4". */
        explicit Version(const std::string& versionString) { parse(versionString); }

        /**
         * @brief Parses a version from a dot-separated string.
         *
         * Version(string) and Version.Parse(string) share this parser. Each component
         * follows the NumberStyles.Integer lexical rules used by .NET's
         * Version.TryParseComponent (`Version.cs:430-434`): leading and trailing
         * whitespace bytes 0x09–0x0D and 0x20 are accepted, as is an optional sign.
         * Version then rejects a successfully parsed negative component.
         * @throws System::ArgumentException if the string has fewer than two or more
         *         than four dot-separated components.
         * @throws System::ArgumentOutOfRangeException if a component is negative.
         * @throws System::FormatException if a component is not an integer.
         * @throws System::OverflowException if a component exceeds Int32 range.
         */
        static Version Parse(const std::string& s) { return Version(s); }

        /** @brief Tries to parse a version string without throwing. Returns true on success. */
        static bool TryParse(const std::string& s, Version& result) {
            intcs parts[4] = {0, 0, -1, -1};
            std::size_t bad = 0;
            if (parseCore(s, parts, bad) != ParseError::None) return false;
            result.Major = parts[0];
            result.Minor = parts[1];
            result.Build = parts[2];
            result.Revision = parts[3];
            return true;
        }

        /**
         * @brief Gets the major component.
         * Matches .NET Version.Major (`Version.cs:99-100`).
         */
        [[nodiscard]] intcs getMajorProperty() const {
            return Major;
        }

        /**
         * @brief Gets the minor component.
         * Matches .NET Version.Minor (`Version.cs:102`).
         */
        [[nodiscard]] intcs getMinorProperty() const {
            return Minor;
        }

        /**
         * @brief Gets the build component, or -1 if it is unspecified.
         * Matches .NET Version.Build (`Version.cs:104`).
         */
        [[nodiscard]] intcs getBuildProperty() const {
            return Build;
        }

        /**
         * @brief Gets the revision component, or -1 if it is unspecified.
         * Matches .NET Version.Revision (`Version.cs:106`).
         */
        [[nodiscard]] intcs getRevisionProperty() const {
            return Revision;
        }

        /**
         * @brief Gets the high 16 bits of the Revision component.
         * Matches .NET Version.MajorRevision.
         */
        [[nodiscard]] shortcs getMajorRevisionProperty() const {
            return static_cast<shortcs>(Revision >> 16);
        }

        /**
         * @brief Gets the low 16 bits of the Revision component.
         * Matches .NET Version.MinorRevision.
         */
        [[nodiscard]] shortcs getMinorRevisionProperty() const {
            return static_cast<shortcs>(Revision & 0xFFFF);
        }

        /** @brief Compares this version to another. Returns negative, zero, or positive. */
        [[nodiscard]] constexpr intcs CompareTo(const Version& other) const noexcept {
            return cmp(other);
        }

        /** @brief Returns true if this version has equal components to other. */
        [[nodiscard]] bool Equals(const Version& other) const { return cmp(other) == 0; }

        /** @brief Returns a hash code for this version. */
        [[nodiscard]] intcs GetHashCode() const {
            // Mirror .NET: accumulate 4 components with bit shifts
            intcs hash = 0;
            hash |= (Major & 0x0000000F) << 28;
            hash |= (Minor & 0x000000FF) << 20;
            hash |= (Build & 0x000000FF) << 12;
            hash |= (Revision & 0x00000FFF);
            return hash;
        }

        /**
         * @brief Returns the version as a dot-separated string,
         * truncated at the first unspecified (negative) component.
         *
         * Matching .NET's Version.ToString(), this delegates to ToString(intcs) with
         * a short-circuiting field count: two fields when Build is unspecified, three
         * when Revision is unspecified, and four when all components are defined.
         */
        [[nodiscard]] std::string ToString() const {
            return ToString(defaultFieldCount());
        }

        /**
         * @brief Returns the version string with exactly fieldCount components.
         *
         * A component that this instance does not define — Build on a version built
         * from two components, Revision on a version built from fewer than four — may
         * not be requested. Matching .NET's Version.ToString(int), such a fieldCount is
         * rejected rather than serialized; emitting the -1 sentinel would produce text
         * like "1.2.-1", which is not a valid version representation (SR-AUD-011).
         *
         * The guards test Build and then Revision in .NET's order. This preserves
         * the component-specific upper bounds 2 and 3 reported by
         * Version.TryFormatCore (`Version.cs:204-224`). All public construction and
         * parsing paths preserve the invariant that Revision can be defined only when
         * Build is defined.
         *
         * The rejection message is .NET's own, resolved from the reference tree by ticket
         * #2260: `SR.Format(SR.ArgumentOutOfRange_Bounds_Lower_Upper, "0", bound)`, where
         * `ArgumentOutOfRange_Bounds_Lower_Upper` reads **"Argument must be between {0} and
         * {1}."** (`Strings.resx:1762`). The upper bound is instance-dependent, and this
         * port's three guards already select it exactly as `Version.TryFormatCore` does
         * (`Version.cs:204-224`): the out-of-interval case is tested **first**, so a
         * `fieldCount` of 5 reports bound 4 even on a two-component version — #2260's
         * suspicion that .NET reports 2 there was wrong, and no bound changed.
         *
         * @param fieldCount Number of components to include (0–4, and no greater than
         *        the number of components this instance defines).
         * @throws System::ArgumentException if fieldCount lies outside 0–4, or requests
         *         a component this instance does not define.
         */
        [[nodiscard]] std::string ToString(intcs fieldCount) const {
            // .NET switches on `(uint)fieldCount`, so a negative value lands in the same
            // `> 4` case rather than in one of the component-specific ones. The `< 0` test
            // here is that cast written out.
            if (fieldCount < 0 || fieldCount > 4)
                throw System::ArgumentException("Argument must be between 0 and 4.", "fieldCount");
            if (fieldCount == 0) return "";
            if (fieldCount >= 3 && Build < 0)
                throw System::ArgumentException("Argument must be between 0 and 2.", "fieldCount");
            if (fieldCount >= 4 && Revision < 0)
                throw System::ArgumentException("Argument must be between 0 and 3.", "fieldCount");
            std::string result = std::to_string(Major);
            if (fieldCount >= 2) { result += '.'; result += std::to_string(Minor); }
            if (fieldCount >= 3) { result += '.'; result += std::to_string(Build); }
            if (fieldCount >= 4) { result += '.'; result += std::to_string(Revision); }
            return result;
        }

        /** @brief Provides component-wise equality and lexicographic ordering. */
        friend constexpr auto operator<=>(const Version&, const Version&) noexcept = default;

    private:
        /**
         * @brief The number of components the no-argument ToString() emits.
         *
         * Mirrors .NET's private Version.DefaultFormatFieldCount
         * (`_Build == -1 ? 2 : _Revision == -1 ? 3 : 4`), short-circuiting at the first
         * unspecified component. The result is always 2, 3 or 4, and by construction
         * never names a component this instance does not define, so the ToString(intcs)
         * it feeds can never reject it.
         */
        [[nodiscard]] intcs defaultFieldCount() const {
            if (Build    < 0) return 2;
            if (Revision < 0) return 3;
            return 4;
        }

        static constexpr void requireNonNegative(intcs value, const char* name) {
            if (value < 0)
                throw System::ArgumentOutOfRangeException(name, std::string(name) + " must be greater than or equal to zero.");
        }

        static constexpr bool isWhite(char c) noexcept { return c == ' ' || (c >= 0x09 && c <= 0x0D); }

        constexpr intcs cmp(const Version& o) const noexcept {
            // Uses direct comparison rather than subtraction, matching .NET's actual
            // CompareTo: a subtraction-based comparison would silently overflow
            // (undefined behavior) if components could span the full int32 range.
            if (Major    != o.Major)    return Major    > o.Major    ? 1 : -1;
            if (Minor    != o.Minor)    return Minor    > o.Minor    ? 1 : -1;
            if (Build    != o.Build)    return Build    > o.Build    ? 1 : -1;
            if (Revision != o.Revision) return Revision > o.Revision ? 1 : -1;
            return 0;
        }

        /** Failure category reported by parseCore() and parseComponent(); mapped to a .NET exception type by parse(). */
        enum class ParseError { None, Count, Format, Overflow, Negative };

        /**
         * Parses one component the way .NET's NumberStyles.Integer does: optional white
         * space, an optional sign, digits, optional trailing white space.
         * @param c   the component text, without dots.
         * @param out receives the value; written only on ParseError::None.
         * @return ParseError::None on success, otherwise the failure category.
         */
        static ParseError parseComponent(std::string_view c, intcs& out) noexcept {
            const char* first = c.data();
            const char* const last = c.data() + c.size();
            while (first != last && isWhite(*first)) ++first;
            if (first != last && *first == '+') {
                ++first;
                if (first != last && *first == '-') return ParseError::Format; // "+-1"
            }
            intcs value = 0;
            const auto r = std::from_chars(first, last, value);
            if (r.ec == std::errc::result_out_of_range) return ParseError::Overflow;
            if (r.ec != std::errc()) return ParseError::Format;
            const char* p = r.ptr;
            while (p != last && isWhite(*p)) ++p;
            if (p != last) return ParseError::Format;
            if (value < 0) return ParseError::Negative;
            out = value;
            return ParseError::None;
        }

        /**
         * Splits @p s on '.' into 2–4 components and parses each with parseComponent().
         * @param s        the version text.
         * @param parts    receives the parsed components; entries past the count are untouched.
         * @param badIndex receives the index of the offending component for Format/Overflow/Negative.
         * @return ParseError::None on success, otherwise the first failure found.
         */
        static ParseError parseCore(std::string_view s, intcs (&parts)[4],
                                    std::size_t& badIndex) noexcept {
            // Preserve the empty component produced by a trailing separator.
            // "1.2." and "1.2.3." therefore fail as malformed components, while
            // "1.2.3.4." reaches the five-component limit first. This matches
            // Version.ParseVersion (`Version.cs:358-428`).

            std::string_view comps[4];
            std::size_t n = 0;
            for (;;) {
                if (n == 4) return ParseError::Count; // a fifth component
                const std::size_t dot = s.find('.');
                comps[n++] = s.substr(0, dot);
                if (dot == std::string_view::npos) break;
                s.remove_prefix(dot + 1);
            }
            if (n < 2) return ParseError::Count;

            for (std::size_t i = 0; i < n; ++i) {
                const ParseError e = parseComponent(comps[i], parts[i]);
                if (e != ParseError::None) { badIndex = i; return e; }
            }
            return ParseError::None;
        }

        void parse(std::string_view s) {
            static constexpr const char* kComponentNames[4] = {"major", "minor", "build", "revision"};
            intcs parts[4] = {0, 0, -1, -1};
            std::size_t bad = 0;
            switch (parseCore(s, parts, bad)) {
            case ParseError::None: break;
            case ParseError::Count:
                throw System::ArgumentException("Version string portion was too short or too long.", "version");
            case ParseError::Format:
                throw System::FormatException("Input string was not in a correct format.");
            case ParseError::Overflow:
                throw System::OverflowException("Value was either too large or too small for an Int32.");
            case ParseError::Negative:
                throw System::ArgumentOutOfRangeException(kComponentNames[bad],
                    std::string(kComponentNames[bad]) + " must be greater than or equal to zero.");
            }
            Major = parts[0]; Minor = parts[1]; Build = parts[2]; Revision = parts[3];
        }
    };

} // namespace System
