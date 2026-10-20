// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <tinyxml2/tinyxml2.h>

namespace tinyxml2 = ::SharpRuntime::Vendor::tinyxml2;

TEST(VendoredTinyXml2ProcessingInstructionTests, InstructionsAreAcceptedAtEveryLegalPosition) {
    const char* const inputs[] = {
        "<?p d?><r/>",
        "<r><?p d?></r>",
        "<r/><?p d?>",
        "<!--c--><?p d?><r/>",
        "<?xml version=\"1.0\"?><?p d?><r/>",
        "<?before?><r>x<?inside d?><child/><?after-child?></r><?after?>",
        "<r><?xml-stylesheet href=\"x\"?></r>"
    };
    for (const char* input : inputs) {
        SCOPED_TRACE(input);
        tinyxml2::XMLDocument document;
        EXPECT_EQ(document.Parse(input), tinyxml2::XML_SUCCESS);
        EXPECT_NE(document.RootElement(), nullptr);
    }
}

TEST(VendoredTinyXml2ProcessingInstructionTests, XmlDeclarationIsStillReservedAndFirst) {
    const char* const invalid[] = {
        "<r><?xml version=\"1.0\"?></r>",
        "<r/><?xml version=\"1.0\"?>",
        "<?XML version=\"1.0\"?><r/>",
        "<?Xml version=\"1.0\"?><r/>",
        "<?p d?><?xml version=\"1.0\"?><r/>",
        "<?xml version=\"1.0\"?><?xml version=\"1.0\"?><r/>",
        "<!--c--><?xml version=\"1.0\"?><r/>",
        "<r><?p d</r>",
        "<r><?" "?></r>",
        "<r><? p?></r>",
        "<r><?1bad?></r>",
        "<r><?p?data?></r>"
    };
    for (const char* input : invalid) {
        SCOPED_TRACE(input);
        tinyxml2::XMLDocument document;
        EXPECT_EQ(document.Parse(input), tinyxml2::XML_ERROR_PARSING_DECLARATION);
    }
}

TEST(VendoredTinyXml2ProcessingInstructionTests, InstructionDataAndAdjacentTextArePreserved) {
    tinyxml2::XMLDocument document;
    ASSERT_EQ(document.Parse("<r>left<?p d &amp; x?>right</r>"), tinyxml2::XML_SUCCESS);
    const auto* left = document.RootElement()->FirstChild();
    ASSERT_NE(left, nullptr);
    ASSERT_NE(left->ToText(), nullptr);
    EXPECT_STREQ(left->Value(), "left");
    const auto* instruction = left->NextSibling();
    ASSERT_NE(instruction, nullptr);
    ASSERT_NE(instruction->ToDeclaration(), nullptr);
    EXPECT_STREQ(instruction->Value(), "p d &amp; x");
    const auto* right = instruction->NextSibling();
    ASSERT_NE(right, nullptr);
    ASSERT_NE(right->ToText(), nullptr);
    EXPECT_STREQ(right->Value(), "right");
    EXPECT_EQ(right->NextSibling(), nullptr);
}

TEST(VendoredTinyXml2ProcessingInstructionTests, LiteralMarkupAndByteBomKeepTheirMeaning) {
    tinyxml2::XMLDocument document;
    ASSERT_EQ(document.Parse("\xEF\xBB\xBF<r a=\"&lt;?p d?>\"><!--<?p d?>-->"
                             "<![CDATA[<?p d?>]]></r>"), tinyxml2::XML_SUCCESS);
    EXPECT_TRUE(document.HasBOM());
    const auto* root = document.RootElement();
    ASSERT_NE(root, nullptr);
    EXPECT_STREQ(root->Attribute("a"), "<?p d?>");
    const auto* comment = root->FirstChild();
    ASSERT_NE(comment, nullptr);
    ASSERT_NE(comment->ToComment(), nullptr);
    EXPECT_STREQ(comment->Value(), "<?p d?>");
    const auto* text = comment->NextSibling();
    ASSERT_NE(text, nullptr);
    ASSERT_NE(text->ToText(), nullptr);
    EXPECT_TRUE(text->ToText()->CData());
    EXPECT_STREQ(text->Value(), "<?p d?>");
}
