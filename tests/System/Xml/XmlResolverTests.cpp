// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <fstream>
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlSecureResolver.hpp"
#include "System/Xml/XmlUrlResolver.hpp"

using namespace System::Xml;

TEST(XmlUrlResolverTests, GetEntity_LocalFile_ReturnsContents) {
    std::string path = "/tmp/sharp_rt_xmlurlresolver_test.txt";
    {
        std::ofstream out(path);
        out << "hello resolver";
    }
    XmlUrlResolver resolver;
    System::Uri uri(std::string("file://") + path);
    std::any result = resolver.GetEntity(uri, "", std::nullopt);
    EXPECT_EQ(std::any_cast<std::string>(result), "hello resolver");
    std::remove(path.c_str());
}

TEST(XmlUrlResolverTests, GetEntity_UnsupportedScheme_Throws) {
    XmlUrlResolver resolver;
    System::Uri uri("http://example.com/foo.xml");
    EXPECT_THROW(resolver.GetEntity(uri, "", std::nullopt), XmlException);
}

TEST(XmlUrlResolverTests, GetEntity_MissingFile_Throws) {
    XmlUrlResolver resolver;
    System::Uri uri(std::string("file:///tmp/sharp_rt_does_not_exist_xyz.txt"));
    EXPECT_THROW(resolver.GetEntity(uri, "", std::nullopt), XmlException);
}

TEST(XmlSecureResolverTests, GetEntity_AlwaysThrows) {
    XmlUrlResolver inner;
    XmlSecureResolver secure(inner, "");
    System::Uri uri("http://example.com/foo.xml");
    EXPECT_THROW(secure.GetEntity(uri, "", std::nullopt), XmlException);
}
