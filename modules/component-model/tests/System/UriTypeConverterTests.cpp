// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// System::UriTypeConverter through the TypeConverter API it now implements. The ticket #1999
// cases (an empty string yields null, a malformed string still throws, a relative URI is
// accepted, OriginalString round-trips) are kept in their new spelling.
#include <gtest/gtest.h>
#include <any>
#include <memory>
#include <string>
#include <string_view>
#include "System/ArgumentNullException.hpp"
#include "System/ComponentModel/Design/Serialization/InstanceDescriptor.hpp"
#include "System/ComponentModel/TypeConverter.hpp"
#include "System/ComponentModel/TypeDescriptor.hpp"
#include "System/NotSupportedException.hpp"
#include "System/Reflection/ConstructorInfo.hpp"
#include "System/Type.hpp"
#include "System/UriFormatException.hpp"
#include "System/UriTypeConverter.hpp"

using System::Type;
using System::Uri;
using System::UriKind;
using System::UriTypeConverter;
using System::ComponentModel::TypeConverter;
using System::ComponentModel::Design::Serialization::InstanceDescriptor;

TEST(UriTypeConverterTest, IsATypeConverter) {
    static_assert(std::is_base_of_v<TypeConverter, UriTypeConverter>);
    const UriTypeConverter c;
    const TypeConverter& base = c;
    EXPECT_TRUE(base.CanConvertFrom(Type::From<std::string>()));
}

TEST(UriTypeConverterTest, CanConvertFrom) {
    const UriTypeConverter c;
    EXPECT_TRUE(c.CanConvertFrom(Type::From<std::string>()));
    EXPECT_TRUE(c.CanConvertFrom(Type::From<std::string_view>()));
    EXPECT_TRUE(c.CanConvertFrom(Type::From<const char*>()));
    EXPECT_TRUE(c.CanConvertFrom(Type::From<char*>()));
    EXPECT_TRUE(c.CanConvertFrom(Type::From<Uri>()));
    EXPECT_TRUE(c.CanConvertFrom(Type::From<InstanceDescriptor>()));
    EXPECT_FALSE(c.CanConvertFrom(Type::From<int>()));
}

TEST(UriTypeConverterTest, CanConvertTo) {
    const UriTypeConverter c;
    EXPECT_TRUE(c.CanConvertTo(Type::From<std::string>()));
    EXPECT_TRUE(c.CanConvertTo(Type::From<Uri>()));
    EXPECT_TRUE(c.CanConvertTo(Type::From<InstanceDescriptor>()));
    EXPECT_FALSE(c.CanConvertTo(Type::From<int>()));
}

TEST(UriTypeConverterTest, ConvertFromString) {
    const UriTypeConverter c;
    const std::any result = c.ConvertFromString("http://example.com");
    const auto* uri = std::any_cast<Uri>(&result);
    ASSERT_NE(uri, nullptr);
    EXPECT_EQ(uri->getHostProperty(), "example.com");
    // A string literal boxed straight into ConvertFrom is a string too.
    const std::any literalResult = c.ConvertFrom(std::any("http://example.com"));
    EXPECT_NE(std::any_cast<Uri>(&literalResult), nullptr);
}

TEST(UriTypeConverterTest, Fix1999_ConvertFromEmptyReturnsTheEmptyStateInsteadOfThrowing) {
    // .NET short-circuits the empty string and returns null --
    // "if (string.IsNullOrEmpty(uriString)) { return null; }" (UriTypeConverter.cs:44-47).
    const UriTypeConverter c;
    std::any result;
    EXPECT_NO_THROW(result = c.ConvertFromString(""));
    EXPECT_FALSE(result.has_value());
}

TEST(UriTypeConverterTest, Fix1999_ARelativeUriIsAccepted) {
    const UriTypeConverter c;
    std::any relative;
    EXPECT_NO_THROW(relative = c.ConvertFromString("/path/to/resource"));
    const auto* uri = std::any_cast<Uri>(&relative);
    ASSERT_NE(uri, nullptr);
    EXPECT_FALSE(uri->getIsAbsoluteUriProperty());
    EXPECT_EQ(c.ConvertToString(relative), "/path/to/resource")
        << "and it round-trips, which is why ConvertTo uses OriginalString";
}

TEST(UriTypeConverterTest, Decl1999_OnlyTheEmptyInputIsShortCircuited) {
    // "Let the Uri constructor throw any informative exceptions": a malformed string throws.
    const UriTypeConverter c;
    EXPECT_THROW((void)c.ConvertFromString("http://exa mple.com/"), System::UriFormatException);
}

TEST(UriTypeConverterTest, ConvertFromUriCopiesWithTheSameKind) {
    const UriTypeConverter c;
    const Uri absolute("http://example.com/path");
    const std::any copy = c.ConvertFrom(std::any(absolute));
    const auto* uri = std::any_cast<Uri>(&copy);
    ASSERT_NE(uri, nullptr);
    EXPECT_TRUE(uri->getIsAbsoluteUriProperty());
    EXPECT_EQ(uri->getOriginalStringProperty(), absolute.getOriginalStringProperty());
}

TEST(UriTypeConverterTest, ConvertFromAnythingElseIsNotSupported) {
    const UriTypeConverter c;
    EXPECT_THROW((void)c.ConvertFrom(std::any(42)), System::NotSupportedException);
    EXPECT_THROW((void)c.ConvertFrom(std::any{}), System::NotSupportedException);
}

TEST(UriTypeConverterTest, ConvertToString) {
    const UriTypeConverter c;
    const Uri uri("http://example.com/path");
    const std::string s = c.ConvertToString(std::any(uri));
    EXPECT_NE(s.find("example.com"), std::string::npos);
}

TEST(UriTypeConverterTest, ConvertTo_UsesOriginalString) {
    // Real .NET returns uri.OriginalString, not uri.AbsoluteUri -- AbsoluteUri throws for a
    // relative Uri while OriginalString never does.
    const UriTypeConverter c;
    const Uri uri("http://example.com/path");
    EXPECT_EQ(c.ConvertToString(std::any(uri)), uri.getOriginalStringProperty());
}

TEST(UriTypeConverterTest, ConvertToUriAndToInstanceDescriptor) {
    const UriTypeConverter c;
    const Uri uri("https://test.org/api");
    const std::any asUri = c.ConvertTo(std::any(uri), Type::From<Uri>());
    ASSERT_NE(std::any_cast<Uri>(&asUri), nullptr);
    EXPECT_EQ(std::any_cast<Uri>(asUri).getHostProperty(), "test.org");

    const std::any described = c.ConvertTo(std::any(uri), Type::From<InstanceDescriptor>());
    const auto* descriptor = std::any_cast<InstanceDescriptor>(&described);
    ASSERT_NE(descriptor, nullptr);
    EXPECT_TRUE(descriptor->getIsCompleteProperty());
    ASSERT_EQ(descriptor->getArgumentsProperty().size(), 2u);
    EXPECT_EQ(std::any_cast<std::string>(descriptor->getArgumentsProperty()[0]), "https://test.org/api");
    EXPECT_EQ(std::any_cast<UriKind>(descriptor->getArgumentsProperty()[1]), UriKind::Absolute);
    const auto member = descriptor->getMemberInfoProperty();
    ASSERT_NE(member, nullptr);
    EXPECT_EQ(member->getDeclaringTypeProperty(), Type::From<Uri>());
    EXPECT_EQ(member->getNameProperty(), System::Reflection::ConstructorInfo::ConstructorName);

    // The descriptor reconstructs the Uri. .NET's UriTypeConverter reports the base
    // InstanceDescriptor capability but its ConvertFrom override nevertheless rejects one.
    const std::any rebuilt = descriptor->Invoke();
    EXPECT_EQ(std::any_cast<Uri>(rebuilt).getHostProperty(), "test.org");
    EXPECT_THROW((void)c.ConvertFrom(described), System::NotSupportedException);
}

TEST(UriTypeConverterTest, ConvertToRefusesNullDestinationAndForeignTypes) {
    const UriTypeConverter c;
    const Uri uri("http://example.com/");
    EXPECT_THROW((void)c.ConvertTo(std::any(uri), Type()), System::ArgumentNullException);
    EXPECT_THROW((void)c.ConvertTo(std::any(uri), Type::From<int>()), System::NotSupportedException);
    EXPECT_THROW((void)c.ConvertTo(std::any(42), Type::From<std::string>()), System::NotSupportedException);
    EXPECT_THROW((void)c.ConvertTo(std::any{}, Type::From<std::string>()), System::NotSupportedException);
}

TEST(UriTypeConverterTest, IsValid) {
    const UriTypeConverter c;
    EXPECT_TRUE(c.IsValid(std::any(std::string("http://example.com"))));
    EXPECT_TRUE(c.IsValid(std::any(std::string("/relative"))));
    EXPECT_FALSE(c.IsValid(std::any(std::string("http://exa mple.com/"))));
    EXPECT_TRUE(c.IsValid(std::any(Uri("http://example.com"))));
    EXPECT_FALSE(c.IsValid(std::any(42)));
}

TEST(UriTypeConverterTest, RoundTrip) {
    const UriTypeConverter c;
    const Uri original("https://test.org/api");
    const std::string s = c.ConvertToString(std::any(original));
    const std::any restored = c.ConvertFromString(s);
    ASSERT_NE(std::any_cast<Uri>(&restored), nullptr);
    EXPECT_EQ(std::any_cast<Uri>(restored).getHostProperty(), "test.org");
}

TEST(UriTypeConverterTest, IsTheConverterTypeDescriptorReturnsForUri) {
    // The C++ spelling of Uri's [TypeConverter(typeof(UriTypeConverter))].
    const auto converter = System::ComponentModel::TypeDescriptor::GetConverter(Type::From<Uri>());
    ASSERT_NE(converter, nullptr);
    EXPECT_NE(dynamic_cast<const UriTypeConverter*>(converter.get()), nullptr);
}
