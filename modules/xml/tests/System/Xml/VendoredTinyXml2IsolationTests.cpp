// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Pins that the vendored tinyxml2 leaves every global name a consumer's own tinyxml2 uses free
// (docs/VendoredTinyXml2.md). This translation unit plays that consumer: it has "already
// included" an upstream tinyxml2.h -- upstream's include guard is defined -- and declares its own
// ::tinyxml2 with a layout unlike the vendored one. Before the vendored copy moved into
// SharpRuntime::Vendor::tinyxml2 this file could not compile: the guard hid the vendored header,
// and ::tinyxml2::XMLDocument would have been defined twice. Split across two translation units
// the same clash compiled and linked silently, and one copy's code ran on the other's layout.
#define TINYXML2_INCLUDED

#include <gtest/gtest.h>
#include <type_traits>
#include <utility>
#include "System/Xml/XmlDocument.hpp"

namespace tinyxml2 {
    class XMLDocument {
    public:
        int consumerMarker = 42;
    };
    static const int TIXML2_MAJOR_VERSION = 10;
}

TEST(VendoredTinyXml2IsolationTests, ConsumerTinyXml2CoexistsWithTheVendoredCopy) {
    static_assert(!std::is_same_v<tinyxml2::XMLDocument,
                                  SharpRuntime::Vendor::tinyxml2::XMLDocument>);
    static_assert(std::is_same_v<decltype(std::declval<System::Xml::XmlDocument&>().getNativeDocument()),
                                 SharpRuntime::Vendor::tinyxml2::XMLDocument&>);

    const tinyxml2::XMLDocument consumerDocument;
    EXPECT_EQ(consumerDocument.consumerMarker, 42);
    EXPECT_EQ(tinyxml2::TIXML2_MAJOR_VERSION, 10);
    EXPECT_EQ(SharpRuntime::Vendor::tinyxml2::TIXML2_MAJOR_VERSION, 11);

    System::Xml::XmlDocument document;
    document.LoadXml("<root><child/></root>");
    const auto* root = document.getNativeDocument().RootElement();
    ASSERT_NE(root, nullptr);
    EXPECT_STREQ(root->Name(), "root");
}
