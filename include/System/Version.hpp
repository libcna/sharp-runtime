// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <string>
#include <sstream>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;

    /// <summary>
    /// Represents a version number with Major, Minor, Build, and Revision components.
    ///
    /// Partial C++ counterpart of .NET System.Version.
    /// </summary>
    class Version {
    public:
        intcs Major    = 0; ///< Major version component.
        intcs Minor    = 0; ///< Minor version component.
        intcs Build    = -1; ///< Build number; -1 means not specified.
        intcs Revision = -1; ///< Revision number; -1 means not specified.

        /// Constructs a Version with all components set to their defaults (0.0).
        Version() = default;
        /// Constructs a Version with the given major and minor components.
        Version(intcs major, intcs minor) : Major(major), Minor(minor) {}
        /// Constructs a Version with major, minor, and build components.
        Version(intcs major, intcs minor, intcs build) : Major(major), Minor(minor), Build(build) {}
        /// Constructs a Version with all four components.
        Version(intcs major, intcs minor, intcs build, intcs revision)
            : Major(major), Minor(minor), Build(build), Revision(revision) {}

        /// Parses a version from a dot-separated string such as "1.2.3.4".
        /// @param versionString String to parse; must contain at least two components.
        explicit Version(const std::string& versionString) { parse(versionString); }

        /// Parses a version from a dot-separated string. Throws std::invalid_argument on failure.
        static Version Parse(const std::string& s) {
            try { return Version(s); } catch (...) {
                throw std::invalid_argument("Invalid version string: " + s);
            }
        }

        /// Tries to parse a version string without throwing. Returns true on success.
        static bool TryParse(const std::string& s, Version& result) {
            try { result = Version(s); return true; } catch (...) { return false; }
        }

        /// Compares this version to another. Returns negative, zero, or positive.
        [[nodiscard]] intcs CompareTo(const Version& other) const { return cmp(other); }

        /// Returns true if this version has equal components to other.
        [[nodiscard]] bool Equals(const Version& other) const { return cmp(other) == 0; }

        /// Returns the version as a dot-separated string, omitting unspecified (negative) components.
        /// @return String representation, e.g. "1.2" or "1.2.3.4".
        [[nodiscard]] std::string ToString() const {
            std::ostringstream oss;
            oss << Major << '.' << Minor;
            if (Build    >= 0) oss << '.' << Build;
            if (Revision >= 0) oss << '.' << Revision;
            return oss.str();
        }

        /// Returns true if this version is equal to @p o.
        bool operator==(const Version& o) const { return cmp(o) == 0; }
        /// Returns true if this version is not equal to @p o.
        bool operator!=(const Version& o) const { return cmp(o) != 0; }
        /// Returns true if this version is less than @p o.
        bool operator< (const Version& o) const { return cmp(o) <  0; }
        /// Returns true if this version is less than or equal to @p o.
        bool operator<=(const Version& o) const { return cmp(o) <= 0; }
        /// Returns true if this version is greater than @p o.
        bool operator> (const Version& o) const { return cmp(o) >  0; }
        /// Returns true if this version is greater than or equal to @p o.
        bool operator>=(const Version& o) const { return cmp(o) >= 0; }

    private:
        intcs cmp(const Version& o) const {
            if (Major    != o.Major)    return Major    - o.Major;
            if (Minor    != o.Minor)    return Minor    - o.Minor;
            if (Build    != o.Build)    return Build    - o.Build;
            if (Revision != o.Revision) return Revision - o.Revision;
            return 0;
        }
        void parse(const std::string& s) {
            intcs parts[4] = {0, 0, -1, -1};
            int idx = 0;
            std::istringstream ss(s);
            std::string tok;
            while (idx < 4 && std::getline(ss, tok, '.'))
                parts[idx++] = std::stoi(tok);
            Major = parts[0]; Minor = parts[1]; Build = parts[2]; Revision = parts[3];
        }
    };

} // namespace System
