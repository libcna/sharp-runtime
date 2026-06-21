// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/ApplicationId.hpp"
#include "System/Version.hpp"

using System::ApplicationId;
using System::Version;

static ApplicationId makeId() {
    return ApplicationId("token123", "MyApp", Version(1, 2, 3, 4), "amd64", "neutral");
}

TEST(ApplicationIdTests, GetName_CorrectValue) {
    EXPECT_EQ(makeId().getNameProperty(), "MyApp");
}
TEST(ApplicationIdTests, GetVersion_CorrectValue) {
    EXPECT_EQ(makeId().getVersionProperty().ToString(), "1.2.3.4");
}
TEST(ApplicationIdTests, GetProcessorArchitecture) {
    EXPECT_EQ(makeId().getProcessorArchitectureProperty(), "amd64");
}
TEST(ApplicationIdTests, GetCulture) {
    EXPECT_EQ(makeId().getCultureProperty(), "neutral");
}
TEST(ApplicationIdTests, GetPublicKeyToken) {
    EXPECT_EQ(makeId().getPublicKeyTokenProperty(), "token123");
}
TEST(ApplicationIdTests, ToString_ContainsName_New) {
    EXPECT_NE(makeId().ToString().find("MyApp"), std::string::npos);
}
TEST(ApplicationIdTests, ToString_ContainsVersion) {
    EXPECT_NE(makeId().ToString().find("1.2.3.4"), std::string::npos);
}
TEST(ApplicationIdTests, ToString_ContainsCulture) {
    EXPECT_NE(makeId().ToString().find("neutral"), std::string::npos);
}
TEST(ApplicationIdTests, ToString_ContainsArchitecture) {
    EXPECT_NE(makeId().ToString().find("amd64"), std::string::npos);
}
