// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for the System::Version .NET-shape migration.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include "System/Version.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Version;

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(version-major-direct-read): is private within this context
    const Version readMajor(1, 2, 3, 4);
    const auto major = readMajor.Major;
    (void)major;
#else
    const Version readMajor(1, 2, 3, 4);
    const auto major = readMajor.getMajorProperty();
    (void)major;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(version-major-direct-write): is private within this context
    Version writeMajor(1, 2, 3, 4);
    writeMajor.Major = 9;
#else
    Version writeMajor(1, 2, 3, 4);
    writeMajor = Version(9, 2, 3, 4);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(version-minor-direct-read): is private within this context
    const Version readMinor(1, 2, 3, 4);
    const auto minor = readMinor.Minor;
    (void)minor;
#else
    const Version readMinor(1, 2, 3, 4);
    const auto minor = readMinor.getMinorProperty();
    (void)minor;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(version-minor-direct-write): is private within this context
    Version writeMinor(1, 2, 3, 4);
    writeMinor.Minor = 9;
#else
    Version writeMinor(1, 2, 3, 4);
    writeMinor = Version(1, 9, 3, 4);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // NEGATIVE(version-build-direct-read): is private within this context
    const Version readBuild(1, 2, 3, 4);
    const auto build = readBuild.Build;
    (void)build;
#else
    const Version readBuild(1, 2, 3, 4);
    const auto build = readBuild.getBuildProperty();
    (void)build;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 6
    // NEGATIVE(version-build-direct-write): is private within this context
    Version writeBuild(1, 2, 3, 4);
    writeBuild.Build = 9;
#else
    Version writeBuild(1, 2, 3, 4);
    writeBuild = Version(1, 2, 9, 4);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 7
    // NEGATIVE(version-revision-direct-read): is private within this context
    const Version readRevision(1, 2, 3, 4);
    const auto revision = readRevision.Revision;
    (void)revision;
#else
    const Version readRevision(1, 2, 3, 4);
    const auto revision = readRevision.getRevisionProperty();
    (void)revision;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 8
    // NEGATIVE(version-revision-direct-write): is private within this context
    Version writeRevision(1, 2, 3, 4);
    writeRevision.Revision = 9;
#else
    Version writeRevision(1, 2, 3, 4);
    writeRevision = Version(1, 2, 3, 9);
#endif

    return 0;
}