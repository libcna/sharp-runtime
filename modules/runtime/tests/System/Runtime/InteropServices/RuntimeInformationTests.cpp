// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include "System/ArgumentException.hpp"
#include "System/Runtime/InteropServices/Architecture.hpp"
#include "System/Runtime/InteropServices/OSPlatform.hpp"
#include "System/Runtime/InteropServices/RuntimeInformation.hpp"

using namespace System::Runtime::InteropServices;

TEST(ArchitectureTests, ValuesAreDistinct) {
    EXPECT_NE(Architecture::X86, Architecture::X64);
    EXPECT_NE(Architecture::Arm, Architecture::Arm64);
}

TEST(OSPlatformTests, WellKnownValues_AreDistinct) {
    EXPECT_NE(OSPlatform::Windows, OSPlatform::Linux);
    EXPECT_NE(OSPlatform::Linux, OSPlatform::OSX);
    EXPECT_NE(OSPlatform::OSX, OSPlatform::FreeBSD);
}

TEST(OSPlatformTests, Create_CaseInsensitiveEquality) {
    auto a = OSPlatform::Create("Linux");
    EXPECT_EQ(a, OSPlatform::Linux);
}

TEST(OSPlatformTests, Create_Empty_Throws) {
    EXPECT_THROW(OSPlatform::Create(""), System::ArgumentException);
}

TEST(OSPlatformTests, ToString_ReturnsName) {
    EXPECT_EQ(OSPlatform::Linux.ToString(), "LINUX");
}

// GetHashCode() must satisfy the Equals/GetHashCode contract: values that compare Equals()
// (case-insensitively) must hash equal.
TEST(OSPlatformTests, GetHashCode_MatchesForCaseInsensitivelyEqualValues) {
    auto a = OSPlatform::Create("Linux");
    EXPECT_EQ(a.GetHashCode(), OSPlatform::Linux.GetHashCode());
}

// Replaces GetHashCode_DiffersForDistinctValues: two distinct platforms are permitted to hash
// equally (docs/HashAssertionContractRule.md R2). The distinction that is guaranteed is on
// Equals, and the hash direction the contract owns is the case-insensitive agreement pinned by
// GetHashCode_MatchesForCaseInsensitivelyEqualValues above.
TEST(OSPlatformTests, DistinctPlatformsAreUnequal) {
    EXPECT_NE(OSPlatform::Linux, OSPlatform::Windows);
    EXPECT_FALSE(OSPlatform::Linux.Equals(OSPlatform::Windows));
}

TEST(RuntimeInformationTests, IsOSPlatform_MatchesLinuxOnThisSandbox) {
    EXPECT_TRUE(RuntimeInformation::IsOSPlatform(OSPlatform::Linux));
    EXPECT_FALSE(RuntimeInformation::IsOSPlatform(OSPlatform::Windows));
}

TEST(RuntimeInformationTests, OSDescription_IsNonEmpty) {
    EXPECT_FALSE(RuntimeInformation::getOSDescriptionProperty().empty());
}

TEST(RuntimeInformationTests, ProcessArchitecture_MatchesOSArchitecture) {
    EXPECT_EQ(RuntimeInformation::getProcessArchitectureProperty(), RuntimeInformation::getOSArchitectureProperty());
}

// OSArchitecture queries the real kernel architecture via uname() (matching real .NET's
// Interop.Sys.GetOSArchitecture()), not just an alias for ProcessArchitecture -- on this x86_64
// Linux sandbox it must resolve to X64 specifically, not merely "equal to whatever
// ProcessArchitecture happens to return" (which would pass vacuously if both were wrong the same way).
TEST(RuntimeInformationTests, OSArchitecture_IsX64OnThisSandbox) {
    EXPECT_EQ(RuntimeInformation::getOSArchitectureProperty(), Architecture::X64);
}

// ===========================================================================
// #1983 -- OSArchitecture reports the OS, and an unknown target is a build error.
//
// The ticket was blocked on "three independent absences, ALL of which must be
// resolved before any code is written". Two of the three are gone: a MinGW-w64
// cross-compiler is present (x86_64-w64-mingw32-g++) and /rv/tmp/runtime is
// present. The third -- a mixed-bitness Windows host on which to OBSERVE the
// difference -- remains, and it gates runtime observation rather than
// implementation. That is exactly the position #2378 was in, and this ticket
// takes its evidence pattern: cross-compile the Windows arm and prove by symbol
// inspection that it is confined, then state the runtime limit.
// ===========================================================================

TEST(RuntimeInformationTests, Fix1983_OSArchitectureIsAValidEnumeratorAndAgreesHere) {
    const Architecture os = RuntimeInformation::getOSArchitectureProperty();
    const Architecture process = RuntimeInformation::getProcessArchitectureProperty();

    // Every value must be one this port declares -- a fabricated or garbage answer fails here.
    const std::vector<Architecture> known{
        Architecture::X86,   Architecture::X64,         Architecture::Arm,
        Architecture::Arm64, Architecture::Wasm,        Architecture::S390x,
        Architecture::LoongArch64, Architecture::Armv6, Architecture::Ppc64le,
        Architecture::RiscV64};
    EXPECT_NE(std::find(known.begin(), known.end(), os), known.end());
    EXPECT_NE(std::find(known.begin(), known.end(), process), known.end());

    // On a NON-WOW64 host the two agree, and this container is one. That is the control the
    // Windows repair needs: the two are allowed to differ, and here they must not, so a repair
    // that started reporting something unrelated shows up immediately.
    EXPECT_EQ(os, process)
        << "this host is not mixed-bitness, so the OS and process architectures must agree";
}

TEST(RuntimeInformationTests, Fix1983_TheProcessArchitectureIsTheCompilationTarget) {
    // ProcessArchitecture is a statement about the compilation target, not a runtime query --
    // which is why .NET's ends in `#error Unknown Architecture` rather than a fallback, and why
    // this port's `return Architecture::X64` for an unrecognised target was a fabrication. The
    // #error itself is verified at COMPILE time (see the migration note); what can be asserted
    // here is that the answer matches the target this suite was built for.
#if defined(__x86_64__)
    EXPECT_EQ(RuntimeInformation::getProcessArchitectureProperty(), Architecture::X64);
#elif defined(__aarch64__)
    EXPECT_EQ(RuntimeInformation::getProcessArchitectureProperty(), Architecture::Arm64);
#else
    SUCCEED() << "no assertion for this target; the #error guarantees the list covers it";
#endif
}
