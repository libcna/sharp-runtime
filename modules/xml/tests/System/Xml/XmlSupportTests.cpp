// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Coverage for the standalone System.Xml enums and exception types. See XmlDomTests.cpp for the
// XmlDocument DOM API and XmlTests.cpp for XmlReader/XmlWriter.
#include <gtest/gtest.h>

#include <any>
#include <stdexcept>
#include "System/Xml/ConformanceLevel.hpp"
#include "System/Xml/DtdProcessing.hpp"
#include "System/Xml/EntityHandling.hpp"
#include "System/Xml/Formatting.hpp"
#include "System/Xml/NamespaceHandling.hpp"
#include "System/Xml/NewLineHandling.hpp"
#include "System/Xml/ReadState.hpp"
#include "System/Xml/ValidationType.hpp"
#include "System/Xml/WhitespaceHandling.hpp"
#include "System/Xml/WriteState.hpp"
#include "System/Xml/XmlDateTimeSerializationMode.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlNamespaceScope.hpp"
#include "System/Xml/XmlNodeChangedAction.hpp"
#include "System/Xml/XmlNodeOrder.hpp"
#include "System/Xml/XmlNodeType.hpp"
#include "System/Xml/XmlOutputMethod.hpp"
#include "System/Xml/Schema/XmlSchemaException.hpp"
#include "System/Xml/Schema/XmlSchemaValidationException.hpp"
#include "System/Xml/XmlSpace.hpp"
#include "System/Xml/XmlTokenizedType.hpp"

using namespace System::Xml;

TEST(XmlEnumTests, ConformanceLevel_Values) {
    EXPECT_EQ(static_cast<int>(ConformanceLevel::Auto), 0);
    EXPECT_EQ(static_cast<int>(ConformanceLevel::Fragment), 1);
    EXPECT_EQ(static_cast<int>(ConformanceLevel::Document), 2);
}

TEST(XmlEnumTests, DtdProcessing_Values) {
    EXPECT_EQ(static_cast<int>(DtdProcessing::Prohibit), 0);
    EXPECT_EQ(static_cast<int>(DtdProcessing::Ignore), 1);
    EXPECT_EQ(static_cast<int>(DtdProcessing::Parse), 2);
}

TEST(XmlEnumTests, EntityHandling_Values) {
    EXPECT_EQ(static_cast<int>(EntityHandling::ExpandEntities), 1);
    EXPECT_EQ(static_cast<int>(EntityHandling::ExpandCharEntities), 2);
}

TEST(XmlEnumTests, Formatting_Values) {
    EXPECT_EQ(static_cast<int>(Formatting::None), 0);
    EXPECT_EQ(static_cast<int>(Formatting::Indented), 1);
}

TEST(XmlEnumTests, NamespaceHandling_FlagsCombine) {
    auto v = NamespaceHandling::Default | NamespaceHandling::OmitDuplicates;
    EXPECT_EQ(static_cast<int>(v), 1);
    EXPECT_EQ(static_cast<int>(v & NamespaceHandling::OmitDuplicates), 1);
}

TEST(XmlEnumTests, NewLineHandling_Values) {
    EXPECT_EQ(static_cast<int>(NewLineHandling::Replace), 0);
    EXPECT_EQ(static_cast<int>(NewLineHandling::Entitize), 1);
    EXPECT_EQ(static_cast<int>(NewLineHandling::None), 2);
}

TEST(XmlEnumTests, ReadState_Values) {
    EXPECT_EQ(static_cast<int>(ReadState::Initial), 0);
    EXPECT_EQ(static_cast<int>(ReadState::Closed), 4);
}

TEST(XmlEnumTests, ValidationType_Values) {
    EXPECT_EQ(static_cast<int>(ValidationType::None), 0);
    EXPECT_EQ(static_cast<int>(ValidationType::Schema), 4);
}

TEST(XmlEnumTests, WhitespaceHandling_Values) {
    EXPECT_EQ(static_cast<int>(WhitespaceHandling::All), 0);
    EXPECT_EQ(static_cast<int>(WhitespaceHandling::None), 2);
}

TEST(XmlEnumTests, WriteState_Values) {
    EXPECT_EQ(static_cast<int>(WriteState::Start), 0);
    EXPECT_EQ(static_cast<int>(WriteState::Error), 6);
}

TEST(XmlEnumTests, XmlDateTimeSerializationMode_Values) {
    EXPECT_EQ(static_cast<int>(XmlDateTimeSerializationMode::Local), 0);
    EXPECT_EQ(static_cast<int>(XmlDateTimeSerializationMode::RoundtripKind), 3);
}

TEST(XmlEnumTests, XmlNamespaceScope_Values) {
    EXPECT_EQ(static_cast<int>(XmlNamespaceScope::All), 0);
    EXPECT_EQ(static_cast<int>(XmlNamespaceScope::Local), 2);
}

TEST(XmlEnumTests, XmlNodeChangedAction_Values) {
    EXPECT_EQ(static_cast<int>(XmlNodeChangedAction::Insert), 0);
    EXPECT_EQ(static_cast<int>(XmlNodeChangedAction::Change), 2);
}

TEST(XmlEnumTests, XmlNodeOrder_Values) {
    EXPECT_EQ(static_cast<int>(XmlNodeOrder::Before), 0);
    EXPECT_EQ(static_cast<int>(XmlNodeOrder::Unknown), 3);
}

TEST(XmlEnumTests, XmlNodeType_FullValueSet) {
    EXPECT_EQ(static_cast<int>(XmlNodeType::None), 0);
    EXPECT_EQ(static_cast<int>(XmlNodeType::Element), 1);
    EXPECT_EQ(static_cast<int>(XmlNodeType::Attribute), 2);
    EXPECT_EQ(static_cast<int>(XmlNodeType::Text), 3);
    EXPECT_EQ(static_cast<int>(XmlNodeType::CDATA), 4);
    EXPECT_EQ(static_cast<int>(XmlNodeType::EntityReference), 5);
    EXPECT_EQ(static_cast<int>(XmlNodeType::Entity), 6);
    EXPECT_EQ(static_cast<int>(XmlNodeType::ProcessingInstruction), 7);
    EXPECT_EQ(static_cast<int>(XmlNodeType::Comment), 8);
    EXPECT_EQ(static_cast<int>(XmlNodeType::Document), 9);
    EXPECT_EQ(static_cast<int>(XmlNodeType::DocumentType), 10);
    EXPECT_EQ(static_cast<int>(XmlNodeType::DocumentFragment), 11);
    EXPECT_EQ(static_cast<int>(XmlNodeType::Notation), 12);
    EXPECT_EQ(static_cast<int>(XmlNodeType::Whitespace), 13);
    EXPECT_EQ(static_cast<int>(XmlNodeType::SignificantWhitespace), 14);
    EXPECT_EQ(static_cast<int>(XmlNodeType::EndElement), 15);
    EXPECT_EQ(static_cast<int>(XmlNodeType::EndEntity), 16);
    EXPECT_EQ(static_cast<int>(XmlNodeType::XmlDeclaration), 17);
}

TEST(XmlEnumTests, XmlOutputMethod_Values) {
    EXPECT_EQ(static_cast<int>(XmlOutputMethod::Xml), 0);
    EXPECT_EQ(static_cast<int>(XmlOutputMethod::AutoDetect), 3);
}

TEST(XmlEnumTests, XmlSpace_Values) {
    EXPECT_EQ(static_cast<int>(XmlSpace::None), 0);
    EXPECT_EQ(static_cast<int>(XmlSpace::Preserve), 2);
}

TEST(XmlEnumTests, XmlTokenizedType_Values) {
    EXPECT_EQ(static_cast<int>(XmlTokenizedType::CDATA), 0);
    EXPECT_EQ(static_cast<int>(XmlTokenizedType::None), 12);
}

// ===========================================================================
// XmlException
// ===========================================================================

TEST(XmlExceptionTests, DefaultCtor_HasDefaultMessage) {
    XmlException ex;
    EXPECT_STREQ(ex.what(), "An XML error has occurred.");
    EXPECT_EQ(ex.getLineNumberProperty(), 0);
    EXPECT_EQ(ex.getLinePositionProperty(), 0);
}

TEST(XmlExceptionTests, MessageCtor_UsesGivenMessage) {
    XmlException ex("bad xml");
    EXPECT_STREQ(ex.what(), "bad xml");
}

TEST(XmlExceptionTests, LineInfoCtor_StoresLineNumberAndPosition) {
    XmlException ex("bad token", 5, 12);
    EXPECT_EQ(ex.getLineNumberProperty(), 5);
    EXPECT_EQ(ex.getLinePositionProperty(), 12);
}

TEST(XmlExceptionTests, SourceUriCtor_StoresSourceUri) {
    XmlException ex("bad token", 1, 1, "file.xml");
    EXPECT_EQ(ex.getSourceUriProperty(), "file.xml");
}

TEST(XmlExceptionTests, IsA_SystemException) {
    XmlException ex("boom");
    EXPECT_NO_THROW({
        try { throw ex; } catch (const System::SystemException&) {}
    });
}

// ===========================================================================
// XmlSchemaException / XmlSchemaValidationException
// ===========================================================================

TEST(XmlSchemaExceptionTests, DefaultCtor_HasSchemaMessageAndHResult) {
    System::Xml::Schema::XmlSchemaException ex;
    EXPECT_STREQ(ex.what(), "A schema error occurred.");
    EXPECT_EQ(ex.getHResultProperty(), static_cast<SharpRuntime::intcs>(0x80131941u));
    EXPECT_EQ(ex.getLineNumberProperty(), 0);
    EXPECT_EQ(ex.getLinePositionProperty(), 0);
    EXPECT_TRUE(ex.getSourceUriProperty().empty());
    EXPECT_EQ(ex.getSourceSchemaObjectProperty(), nullptr);
}

TEST(XmlSchemaExceptionTests, FullCtor_PreservesMessageInnerExceptionAndPosition) {
    auto inner = std::make_exception_ptr(std::runtime_error("inner"));
    System::Xml::Schema::XmlSchemaException ex("schema failure", inner, 7, 19);
    EXPECT_STREQ(ex.what(), "schema failure");
    EXPECT_EQ(ex.getInnerExceptionProperty(), inner);
    EXPECT_EQ(ex.getLineNumberProperty(), 7);
    EXPECT_EQ(ex.getLinePositionProperty(), 19);
}

TEST(XmlSchemaExceptionTests, OtherConstructors_PreserveMessageAndInnerException) {
    System::Xml::Schema::XmlSchemaException messageOnly("schema failure");
    EXPECT_STREQ(messageOnly.what(), "schema failure");

    auto inner = std::make_exception_ptr(std::runtime_error("inner"));
    System::Xml::Schema::XmlSchemaException withInner("schema failure", inner);
    EXPECT_EQ(withInner.getInnerExceptionProperty(), inner);
}

namespace {
using XmlSchemaValidationExceptionBase = System::Xml::Schema::XmlSchemaValidationException;

class TestXmlSchemaValidationException final
    : public XmlSchemaValidationExceptionBase {
public:
    using XmlSchemaValidationExceptionBase::XmlSchemaValidationExceptionBase;
    using XmlSchemaValidationExceptionBase::SetSourceObject;
};
} // namespace

TEST(XmlSchemaValidationExceptionTests, Constructors_PreserveSchemaMetadata) {
    auto inner = std::make_exception_ptr(std::runtime_error("inner"));
    System::Xml::Schema::XmlSchemaValidationException ex("validation failure", inner, 3, 11);
    EXPECT_STREQ(ex.what(), "validation failure");
    EXPECT_EQ(ex.getInnerExceptionProperty(), inner);
    EXPECT_EQ(ex.getLineNumberProperty(), 3);
    EXPECT_EQ(ex.getLinePositionProperty(), 11);
    EXPECT_FALSE(ex.getSourceObjectProperty().has_value());
}

TEST(XmlSchemaValidationExceptionTests, OtherConstructors_PreserveBaseExceptionBehavior) {
    System::Xml::Schema::XmlSchemaValidationException defaultException;
    EXPECT_STREQ(defaultException.what(), "A schema error occurred.");

    System::Xml::Schema::XmlSchemaValidationException messageOnly("validation failure");
    EXPECT_STREQ(messageOnly.what(), "validation failure");

    auto inner = std::make_exception_ptr(std::runtime_error("inner"));
    System::Xml::Schema::XmlSchemaValidationException withInner("validation failure", inner);
    EXPECT_EQ(withInner.getInnerExceptionProperty(), inner);
}

TEST(XmlSchemaValidationExceptionTests, SetSourceObject_ExposesTypeErasedObject) {
    int source = 42;
    TestXmlSchemaValidationException ex("validation failure");
    ex.SetSourceObject(&source);
    EXPECT_EQ(*std::any_cast<int*>(&ex.getSourceObjectProperty()), &source);
}
