// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/UriBuilder.hpp"

using System::UriBuilder;

TEST(UriBuilderTest, DefaultCtor) {
    UriBuilder b;
    EXPECT_EQ(b.getSchemeProperty(), "http");
    EXPECT_EQ(b.getPathProperty(), "/");
}

TEST(UriBuilderTest, CtorFromString) {
    UriBuilder b("http://example.com/path");
    EXPECT_EQ(b.getSchemeProperty(), "http");
    EXPECT_EQ(b.getHostProperty(), "example.com");
}

TEST(UriBuilderTest, SetScheme) {
    UriBuilder b;
    b.setSchemeProperty("https");
    EXPECT_EQ(b.getSchemeProperty(), "https");
}

TEST(UriBuilderTest, SetHost) {
    UriBuilder b;
    b.setHostProperty("example.org");
    EXPECT_EQ(b.getHostProperty(), "example.org");
}

TEST(UriBuilderTest, SetPath) {
    UriBuilder b;
    b.setPathProperty("/api/v1");
    EXPECT_EQ(b.getPathProperty(), "/api/v1");
}

TEST(UriBuilderTest, SetQuery) {
    UriBuilder b;
    b.setQueryProperty("?key=value");
    EXPECT_FALSE(b.getQueryProperty().empty());
}

TEST(UriBuilderTest, ToUri) {
    UriBuilder b;
    b.setSchemeProperty("https");
    b.setHostProperty("example.com");
    b.setPathProperty("/test");
    auto uri = b.getUriProperty();
    EXPECT_NE(uri.getAbsoluteUriProperty().find("example.com"), std::string::npos);
}

TEST(UriBuilderTest, ToStringNotEmpty) {
    UriBuilder b;
    b.setHostProperty("localhost");
    EXPECT_FALSE(b.ToString().empty());
}
