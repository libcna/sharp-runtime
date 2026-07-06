// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
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
