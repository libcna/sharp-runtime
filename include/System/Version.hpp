// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;
    using SharpRuntime::shortcs;

    /**
     * @brief Represents a version number with Major, Minor, Build, and Revision components.
     *
     * C++ counterpart of .NET System.Version.
     */
    class Version {
    public:
        intcs Major    = 0;  ///< Major version component.
        intcs Minor    = 0;  ///< Minor version component.
        intcs Build    = -1; ///< Build number; -1 means not specified.
        intcs Revision = -1; ///< Revision number; -1 means not specified.

        /** @brief Constructs a Version with all components set to their defaults (0.0). */
        Version() = default;

        /**
         * @brief Constructs a Version with the given major and minor components.
         * @throws std::invalid_argument if @p major or @p minor is negative.
         */
        Version(intcs major, intcs minor) : Major(major), Minor(minor) {
            requireNonNegative(major, "major");
            requireNonNegative(minor, "minor");
        }

        /**
         * @brief Constructs a Version with major, minor, and build components.
         * @throws std::invalid_argument if @p major, @p minor, or @p build is negative.
         */
        Version(intcs major, intcs minor, intcs build) : Major(major), Minor(minor), Build(build) {
            requireNonNegative(major, "major");
            requireNonNegative(minor, "minor");
            requireNonNegative(build, "build");
        }

        /**
         * @brief Constructs a Version with all four components.
         * @throws std::invalid_argument if any component is negative (unlike the
         *         2-/3-arg overloads, this one validates @p revision too, since it
         *         is explicitly user-supplied here rather than defaulted to -1).
         */
        Version(intcs major, intcs minor, intcs build, intcs revision)
            : Major(major), Minor(minor), Build(build), Revision(revision) {
            requireNonNegative(major, "major");
            requireNonNegative(minor, "minor");
            requireNonNegative(build, "build");
            requireNonNegative(revision, "revision");
        }

        /** @brief Parses a version from a dot-separated string such as "1.2.3.4". */
        explicit Version(const std::string& versionString) { parse(versionString); }

        /** @brief Parses a version from a dot-separated string. Throws std::invalid_argument on failure. */
        static Version Parse(const std::string& s) {
            try { return Version(s); }
            catch (...) { throw std::invalid_argument("Invalid version string: " + s); }
        }

        /** @brief Tries to parse a version string without throwing. Returns true on success. */
        static bool TryParse(const std::string& s, Version& result) {
            try { result = Version(s); return true; } catch (...) { return false; }
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
        [[nodiscard]] intcs CompareTo(const Version& other) const { return cmp(other); }

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
         * omitting unspecified (negative) components.
         */
        [[nodiscard]] std::string ToString() const {
            std::ostringstream oss;
            oss << Major << '.' << Minor;
            if (Build    >= 0) oss << '.' << Build;
            if (Revision >= 0) oss << '.' << Revision;
            return oss.str();
        }

        /**
         * @brief Returns the version string with exactly fieldCount components.
         * @param fieldCount Number of components to include (1–4).
         * @throws std::invalid_argument if fieldCount is out of range.
         */
        [[nodiscard]] std::string ToString(intcs fieldCount) const {
            if (fieldCount < 0 || fieldCount > 4)
                throw std::invalid_argument("fieldCount must be 0-4");
            if (fieldCount == 0) return "";
            std::ostringstream oss;
            oss << Major;
            if (fieldCount >= 2) oss << '.' << Minor;
            if (fieldCount >= 3) oss << '.' << Build;
            if (fieldCount >= 4) oss << '.' << Revision;
            return oss.str();
        }

        /** @brief Returns true if this version is equal to o. */
        bool operator==(const Version& o) const { return cmp(o) == 0; }
        /** @brief Returns true if this version is not equal to o. */
        bool operator!=(const Version& o) const { return cmp(o) != 0; }
        /** @brief Returns true if this version is less than o. */
        bool operator< (const Version& o) const { return cmp(o) <  0; }
        /** @brief Returns true if this version is less than or equal to o. */
        bool operator<=(const Version& o) const { return cmp(o) <= 0; }
        /** @brief Returns true if this version is greater than o. */
        bool operator> (const Version& o) const { return cmp(o) >  0; }
        /** @brief Returns true if this version is greater than or equal to o. */
        bool operator>=(const Version& o) const { return cmp(o) >= 0; }

    private:
        static void requireNonNegative(intcs value, const char* name) {
            if (value < 0)
                throw std::invalid_argument(std::string(name) + " must be greater than or equal to zero.");
        }

        intcs cmp(const Version& o) const {
            // Uses direct comparison rather than subtraction, matching .NET's actual
            // CompareTo: a subtraction-based comparison would silently overflow
            // (undefined behavior) if components could span the full int32 range.
            if (Major    != o.Major)    return Major    > o.Major    ? 1 : -1;
            if (Minor    != o.Minor)    return Minor    > o.Minor    ? 1 : -1;
            if (Build    != o.Build)    return Build    > o.Build    ? 1 : -1;
            if (Revision != o.Revision) return Revision > o.Revision ? 1 : -1;
            return 0;
        }
        void parse(const std::string& s) {
            std::vector<std::string> toks;
            std::istringstream ss(s);
            std::string tok;
            while (std::getline(ss, tok, '.')) toks.push_back(tok);
            // .NET requires at least "major.minor" and at most 4 dot-separated components.
            if (toks.size() < 2 || toks.size() > 4)
                throw std::invalid_argument("Version string portion was too short or too long.");

            intcs parts[4] = {0, 0, -1, -1};
            for (std::size_t i = 0; i < toks.size(); ++i) {
                std::size_t consumed = 0;
                int v = std::stoi(toks[i], &consumed);
                if (consumed != toks[i].size() || v < 0)
                    throw std::invalid_argument("Version's parameters must be greater than or equal to zero.");
                parts[i] = static_cast<intcs>(v);
            }
            Major = parts[0]; Minor = parts[1]; Build = parts[2]; Revision = parts[3];
        }
    };

} // namespace System
