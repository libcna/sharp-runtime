// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <sstream>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;

    /**
     * @brief Represents the version number of an assembly, operating system,
     * or the common language runtime.
     *
     * Partial C++ counterpart of .NET System.Version.
     *
     * @note Status: Implemented
     */
    class Version {
    public:
        intcs Major    = 0;
        intcs Minor    = 0;
        intcs Build    = -1;
        intcs Revision = -1;

        Version() = default;
        Version(intcs major, intcs minor) : Major(major), Minor(minor) {}
        Version(intcs major, intcs minor, intcs build) : Major(major), Minor(minor), Build(build) {}
        Version(intcs major, intcs minor, intcs build, intcs revision)
            : Major(major), Minor(minor), Build(build), Revision(revision) {}

        explicit Version(const std::string& versionString) { parse(versionString); }

        [[nodiscard]] std::string ToString() const {
            std::ostringstream oss;
            oss << Major << '.' << Minor;
            if (Build    >= 0) oss << '.' << Build;
            if (Revision >= 0) oss << '.' << Revision;
            return oss.str();
        }

        bool operator==(const Version& o) const { return cmp(o) == 0; }
        bool operator!=(const Version& o) const { return cmp(o) != 0; }
        bool operator< (const Version& o) const { return cmp(o) <  0; }
        bool operator<=(const Version& o) const { return cmp(o) <= 0; }
        bool operator> (const Version& o) const { return cmp(o) >  0; }
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
