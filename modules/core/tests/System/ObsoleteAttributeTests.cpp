// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include "System/ObsoleteAttribute.hpp"

using System::ObsoleteAttribute;

namespace {

/** A declaration described by an ObsoleteAttribute object and nothing else. */
int legacyDescribedByAnAttributeObject() { return 7; }

/** The same declaration marked with the C++ facility that does emit a diagnostic. */
[[deprecated("Use legacyDescribedByAnAttributeObject() instead.")]]
int legacyMarkedWithDeprecated() { return 7; }

} // namespace

TEST(ObsoleteAttributeTest, DefaultCtor) {
    ObsoleteAttribute a;
    EXPECT_TRUE(a.getMessageProperty().empty());
    EXPECT_FALSE(a.getIsErrorProperty());
}

TEST(ObsoleteAttributeTest, MessageCtor) {
    ObsoleteAttribute a("use NewMethod instead");
    EXPECT_EQ(a.getMessageProperty(), "use NewMethod instead");
    EXPECT_FALSE(a.getIsErrorProperty());
}

TEST(ObsoleteAttributeTest, MessageAndErrorCtor) {
    ObsoleteAttribute a("deprecated", true);
    EXPECT_EQ(a.getMessageProperty(), "deprecated");
    EXPECT_TRUE(a.getIsErrorProperty());
}

TEST(ObsoleteAttributeTest, DiagnosticId) {
    ObsoleteAttribute a("msg");
    EXPECT_TRUE(a.getDiagnosticIdProperty().empty());
    a.setDiagnosticIdProperty("SYSLIB0001");
    EXPECT_EQ(a.getDiagnosticIdProperty(), "SYSLIB0001");
}

TEST(ObsoleteAttributeTest, UrlFormat) {
    ObsoleteAttribute a("msg");
    EXPECT_TRUE(a.getUrlFormatProperty().empty());
    a.setUrlFormatProperty("https://example.com/{0}");
    EXPECT_EQ(a.getUrlFormatProperty(), "https://example.com/{0}");
}

TEST(ObsoleteAttributeTest, IsAttribute) {
    ObsoleteAttribute a("msg");
    EXPECT_NO_THROW({ System::Attribute& ref = a; (void)ref; });
}

// SR-AUD-115 (#2294). The class carries the four .NET metadata values and no
// side channel: no registry pointer, no slot handle, nothing through which a
// constructed attribute could reach a declaration. A pointer-sized member would
// not fit in the padding the declared members already leave, so this trips on
// exactly the shape an attempt to "implement" the attachment would take.
TEST(ObsoleteAttributeTest, CarriesItsFourDeclaredMembersAndNoSideChannel) {
    constexpr std::size_t declared =
        sizeof(System::Attribute) + 3 * sizeof(std::string) + sizeof(bool);
    constexpr std::size_t align = alignof(ObsoleteAttribute);
    constexpr std::size_t rounded = ((declared + align - 1) / align) * align;
    EXPECT_EQ(sizeof(ObsoleteAttribute), rounded);
    EXPECT_TRUE((std::is_base_of_v<System::Attribute, ObsoleteAttribute>));
}

// SR-AUD-115 (#2294). This is a LANGUAGE-BOUNDARY DEMONSTRATION, not a
// behaviour pin: no edit to ObsoleteAttribute.hpp can make it pass or fail, so
// it is deliberately not counted as a caught mutation. It is the audit report's
// missing case -- "they never compile a deprecated declaration" -- made
// executable. An ObsoleteAttribute with isError = true describing
// legacyDescribedByAnAttributeObject() leaves that function as callable as it
// ever was, while [[deprecated]] on legacyMarkedWithDeprecated() is what the
// compiler reacts to. The suppression below is the evidence: it is REQUIRED,
// and deleting it fails this build with
// "error: ... is deprecated ... [-Werror=deprecated-declarations]".
TEST(ObsoleteAttributeTest, AttributeObjectDeprecatesNothingButDeprecatedDoes) {
    const ObsoleteAttribute a("Use the modern overload instead.", true);
    EXPECT_TRUE(a.getIsErrorProperty());
    EXPECT_EQ(legacyDescribedByAnAttributeObject(), 7);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    EXPECT_EQ(legacyMarkedWithDeprecated(), 7);
#pragma GCC diagnostic pop
}

// The audit report's "no test verifies ... copy/move state". Value semantics:
// every component travels, and a copy is independent of its source.
TEST(ObsoleteAttributeTest, CopyAndMovePreserveEveryComponent) {
    ObsoleteAttribute source("gone in 5.0", true);
    source.setDiagnosticIdProperty("SYSLIB0001");
    source.setUrlFormatProperty("https://example.com/{0}");

    ObsoleteAttribute copy(source);
    EXPECT_EQ(copy.getMessageProperty(), "gone in 5.0");
    EXPECT_TRUE(copy.getIsErrorProperty());
    EXPECT_EQ(copy.getDiagnosticIdProperty(), "SYSLIB0001");
    EXPECT_EQ(copy.getUrlFormatProperty(), "https://example.com/{0}");

    copy.setDiagnosticIdProperty("SYSLIB0002");
    EXPECT_EQ(source.getDiagnosticIdProperty(), "SYSLIB0001");

    ObsoleteAttribute moved(std::move(source));
    EXPECT_EQ(moved.getMessageProperty(), "gone in 5.0");
    EXPECT_TRUE(moved.getIsErrorProperty());
    EXPECT_EQ(moved.getUrlFormatProperty(), "https://example.com/{0}");
}

// The audit report's "no test verifies ... UTF-8 diagnostics". The three text
// components are byte-transparent std::string storage: non-ASCII text round
// trips unchanged and is not re-encoded, truncated at a lead byte, or measured
// in characters.
TEST(ObsoleteAttributeTest, TextComponentsAreByteTransparent) {
    const std::string message = "zastaralé – použijte Modern()";
    ObsoleteAttribute a(message);
    a.setUrlFormatProperty("https://example.com/dokumentace/{0}?jazyk=čeština");

    EXPECT_EQ(a.getMessageProperty(), message);
    EXPECT_EQ(a.getMessageProperty().size(), message.size());
    EXPECT_EQ(a.getUrlFormatProperty(),
              "https://example.com/dokumentace/{0}?jazyk=čeština");
}
