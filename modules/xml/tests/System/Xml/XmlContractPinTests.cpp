// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// System::Xml PINS — docs/SystemXmlNamespaceReviewPlan.md §10 (layout) and §12 ("pins" row),
// which are closure criteria 3, 4 and the layout clause of §19 for the namespace review #2073.
// This file remediates nothing. It records, as checked assertions, two kinds of fact:
//
//   1. LAYOUT. §10 requires that #2074–#2079 and #2081 change no public type's sizeof/alignof,
//      and requires the check to use PROBE STRUCTS rather than literal byte counts — because
//      these types hold std::string/std::map/std::unique_ptr whose sizes differ between
//      libstdc++ and libc++, and the platform policy requires the MinGW, Emscripten and
//      Apple-Clang builds to keep compiling. A probe struct with the same members in the same
//      order is toolchain-independent: it moves exactly when the real type moves.
//
//   2. DEFERRED BEHAVIOUR. #2080, #2082 and #2083 are deferred-verification tickets: the
//      reference tree /rv/tmp/runtime/ is ABSENT, so narrowing or widening what they describe
//      would be a guess. Each is pinned here at its MEASURED current behaviour, so no answer
//      can land silently and a future ticket that changes one must say so.
#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "System/TimeSpan.hpp"
#include "System/Xml/XmlConvert.hpp"
#include "System/Xml/XmlDocument.hpp"
#include "System/Xml/XmlElement.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlNodeChangedEventArgs.hpp"
#include "System/Xml/XmlNamespaceManager.hpp"
#include "System/Xml/XmlNameTable.hpp"
#include "System/Xml/XmlReader.hpp"
#include "System/Xml/XmlWriter.hpp"

using System::TimeSpan;
using System::Xml::XmlConvert;
using System::Xml::XmlDocument;
using System::Xml::XmlNamespaceManager;
using System::Xml::XmlNodeChangedAction;
using System::Xml::XmlNodeChangedEventArgs;
using System::Xml::XmlReader;
using System::Xml::XmlWriter;

// ===========================================================================
// §10 — layout pins, via probe structs (never literal byte counts)
// ===========================================================================

namespace {

/// XmlReader and XmlWriter each hold exactly one std::unique_ptr to an opaque state struct.
/// #2076 added a `bool closed` INSIDE XmlWriterState (defined in the .cpp) and #2078 added no
/// member at all — neither may have reached the public object.
struct OnePointerProbe {
    std::unique_ptr<int> p;
};

/// XmlNamespaceManager's declared members, in declaration order. #2077 replaced one
/// expression; it must not have touched the object.
struct NamespaceManagerProbe {
    virtual ~NamespaceManagerProbe() = default; // XmlNamespaceManager : IXmlNamespaceResolver
    std::shared_ptr<System::Xml::XmlNameTable>                     nameTable;
    std::vector<std::unordered_map<std::string, std::string>>      scopes;
};

/// XmlNodeChangedEventArgs' declared members, in declaration order. #2079 dispatches this type
/// but must not have changed it — it is passed by reference across a public std::function.
struct NodeChangedEventArgsProbe {
    XmlNodeChangedAction    action;
    void*                   node;
    void*                   oldParent;
    void*                   newParent;
    std::string             oldValue;
    std::string             newValue;
};

} // namespace

static_assert(sizeof(XmlReader) == sizeof(OnePointerProbe),
              "XmlReader must stay a single owning pointer to its opaque state. #2078 added a "
              "guard, not a member; a member here is an object-layout change.");
static_assert(alignof(XmlReader) == alignof(OnePointerProbe), "XmlReader alignment moved");

static_assert(sizeof(XmlWriter) == sizeof(OnePointerProbe),
              "XmlWriter must stay a single owning pointer to its opaque state. #2076's "
              "`closed` flag lives INSIDE XmlWriterState, which is defined in the .cpp.");
static_assert(alignof(XmlWriter) == alignof(OnePointerProbe), "XmlWriter alignment moved");

static_assert(sizeof(XmlNamespaceManager) == sizeof(NamespaceManagerProbe),
              "XmlNamespaceManager layout moved; #2077 changed one expression only");
static_assert(alignof(XmlNamespaceManager) == alignof(NamespaceManagerProbe),
              "XmlNamespaceManager alignment moved");

static_assert(sizeof(XmlNodeChangedEventArgs) == sizeof(NodeChangedEventArgsProbe),
              "XmlNodeChangedEventArgs crosses a public std::function boundary; #2079 began "
              "dispatching it and must not have changed its shape");
static_assert(alignof(XmlNodeChangedEventArgs) == alignof(NodeChangedEventArgsProbe),
              "XmlNodeChangedEventArgs alignment moved");

TEST(XmlContractPinTests, PublicObjectLayoutsAreUnchangedByTheCompatibleQueue) {
    // The static_asserts above are the real guard (they fail at COMPILE time). This test exists
    // so the pin is visible in the suite's inventory rather than only in the build log.
    EXPECT_EQ(sizeof(XmlReader), sizeof(OnePointerProbe));
    EXPECT_EQ(sizeof(XmlWriter), sizeof(OnePointerProbe));
    EXPECT_EQ(sizeof(XmlNamespaceManager), sizeof(NamespaceManagerProbe));
    EXPECT_EQ(sizeof(XmlNodeChangedEventArgs), sizeof(NodeChangedEventArgsProbe));
}

TEST(XmlContractPinTests, TheSixNodeChangeHandlersAreStillPublicDataMembers) {
    // §6.6's corrected premise, as a checked fact: #2079 is compatible precisely BECAUSE these
    // are assignable public fields rather than accessors. If one ever becomes a method, this
    // stops compiling — which is the point.
    XmlDocument d;
    d.NodeInserting = nullptr;
    d.NodeInserted  = nullptr;
    d.NodeRemoving  = nullptr;
    d.NodeRemoved   = nullptr;
    d.NodeChanging  = nullptr;
    d.NodeChanged   = nullptr;
    SUCCEED();
}

// ===========================================================================
// #2080 (SR-AUD-354) — DEFERRED VERIFICATION: XmlConvert's TimeSpan lexical space
// ===========================================================================
//
// SR-AUD-354 is correct: XmlConvert uses the framework's colon form, not the XSD duration
// lexical space. The repair is NOT decidable in this container. xs:duration carries years and
// months, which have no fixed length in ticks; .NET resolves that with a specific documented
// approximation, and choosing a different one produces silently wrong durations rather than a
// visible error. Three sub-questions have no repository-contained answer: the year/month
// conversion factors, whether the native "1.00:00:00" form stays accepted (a widening
// question), and the exact exception identity. All four measured behaviours are pinned here.

TEST(XmlContractPinTests, Pin2080_XsdDurationIsRejectedAndTheColonFormIsAccepted) {
    EXPECT_ANY_THROW((void)XmlConvert::ToTimeSpan("P1D"));
    EXPECT_ANY_THROW((void)XmlConvert::ToTimeSpan("PT1H30M"));
    EXPECT_NO_THROW((void)XmlConvert::ToTimeSpan("1.00:00:00")); // the native form is accepted
    EXPECT_EQ(XmlConvert::ToTimeSpan("1.00:00:00"), TimeSpan::FromDays(1));
}

TEST(XmlContractPinTests, Pin2080_ToStringEmitsTheColonFormNotAnXsdDuration) {
    const std::string emitted = XmlConvert::ToString(TimeSpan::FromDays(1));
    EXPECT_NE(emitted.find("00:00:00"), std::string::npos) << "emitted: " << emitted;
    EXPECT_EQ(emitted.find('P'), std::string::npos)
        << "an xs:duration would start with 'P'; emitted: " << emitted;
}

// ===========================================================================
// #2082 RESOLVED — an undeclared entity reference is rejected
// ===========================================================================
//
// Measured (plan §7): "<r>&nope;</r>" was ACCEPTED and round-tripped as "<r>&amp;nope;</r>" --
// the reference was silently reinterpreted as literal text AND re-escaped, so the DOCUMENT'S OWN
// TEXT CHANGED. The deferral said "narrowing parser acceptance needs reference evidence this
// container lacks", which was right; the reference now supplies it. .NET throws
// XmlException("Reference to undeclared entity '{0}'.") -- XmlTextReaderImpl.cs:3829,
// Strings.resx Xml_UndeclaredEntity.

TEST(XmlContractPinTests, Fix2082_AnUndeclaredEntityIsRejected) {
    XmlDocument d;
    EXPECT_THROW(d.LoadXml("<r>&nope;</r>"), System::Xml::XmlException);
    // ...in an attribute value too, which is the other place a reference may appear.
    EXPECT_THROW(d.LoadXml("<r a='&nope;'/>"), System::Xml::XmlException);
}

TEST(XmlContractPinTests, Fix2082_ThePredefinedFiveAndCharacterReferencesAreUntouched) {
    // The invariance row. Rejecting an undeclared entity must not reject a legal one, and the
    // five predefined names plus both character-reference forms are the legal set.
    XmlDocument d;
    EXPECT_NO_THROW(d.LoadXml("<r>&amp;&lt;&gt;&quot;&apos;</r>"));
    EXPECT_NO_THROW(d.LoadXml("<r>&#65;&#x41;</r>"));
    // A '&' that begins no complete reference is a DIFFERENT well-formedness rule and is
    // deliberately left to the parser, not to this check.
    EXPECT_NO_THROW(d.LoadXml("<r>a &amp; b</r>"));
}

TEST(XmlContractPinTests, Fix2082_ADECLAREDEntityIsStillAcceptedBecauseUndeclaredIsTheRule) {
    // The first cut of this check rejected anything not predefined, and the repository's own
    // billion-laughs pin caught it immediately: that document DECLARES two entities and then
    // references one. "Undeclared" and "not predefined" are different sets, and the check has
    // to mean the first.
    XmlDocument d;
    EXPECT_NO_THROW(d.LoadXml("<!DOCTYPE r [<!ENTITY greeting \"hello\">]><r>&greeting;</r>"));
    // ...and an UNdeclared one is still rejected even when a DOCTYPE is present.
    EXPECT_THROW(d.LoadXml("<!DOCTYPE r [<!ENTITY greeting \"hello\">]><r>&other;</r>"),
                 System::Xml::XmlException);
}

TEST(XmlContractPinTests, Fix2082_WhereAnAmpersandIsNotMarkupIsSkipped) {
    // A comment, a CDATA section and a processing instruction all carry '&' as ordinary data.
    // Scanning them would reject documents that are perfectly well-formed.
    XmlDocument d;
    EXPECT_NO_THROW(d.LoadXml("<r><!-- &nope; --></r>"));
    EXPECT_NO_THROW(d.LoadXml("<r><![CDATA[&nope;]]></r>"));
    EXPECT_NO_THROW(d.LoadXml("<?pi &nope; ?><r/>"));
}

// ===========================================================================
// #2083 RESOLVED — an undeclared namespace prefix is rejected
// ===========================================================================
//
// Measured (plan §7): "<p:r/>" was ACCEPTED, never resolved, and round-tripped unchanged. .NET
// throws XmlException("'{0}' is an undeclared prefix.") -- XmlTextReaderImpl.cs:7787,
// Strings.resx Xml_UnknownNs.

TEST(XmlContractPinTests, Fix2083_AnUndeclaredPrefixIsRejected) {
    XmlDocument d;
    EXPECT_THROW(d.LoadXml("<p:r/>"), System::Xml::XmlException);
    EXPECT_THROW(d.LoadXml("<r><p:child/></r>"), System::Xml::XmlException) << "a nested element too";
    EXPECT_THROW(d.LoadXml("<r p:a='1'/>"), System::Xml::XmlException) << "an attribute's prefix too";
}

TEST(XmlContractPinTests, Fix2083_ADeclaredPrefixIsAccepted) {
    XmlDocument d;
    EXPECT_NO_THROW(d.LoadXml("<p:r xmlns:p='urn:x'/>"))
        << "a declaration is in scope for the start-tag it appears on (XML Names 1.0 3)";
    EXPECT_NO_THROW(d.LoadXml("<r xmlns:p='urn:x'><p:child/></r>")) << "and for descendants";
    EXPECT_NO_THROW(d.LoadXml("<r xmlns:p='urn:x' p:a='1'/>")) << "and for attributes";
    EXPECT_NO_THROW(d.LoadXml("<r xmlns='urn:d'><child/></r>")) << "a default declaration";
    EXPECT_NO_THROW(d.LoadXml("<r/>")) << "no prefix at all";
}

TEST(XmlContractPinTests, Fix2083_TheReservedPrefixesNeedNoDeclaration) {
    // XML Names 1.0 3: "xml" is bound by the specification itself and MUST NOT be declared;
    // "xmlns" is reserved. Requiring a declaration for either would reject conforming
    // documents -- xml:lang and xml:space are the everyday cases.
    XmlDocument d;
    EXPECT_NO_THROW(d.LoadXml("<r xml:lang='en'/>"));
    EXPECT_NO_THROW(d.LoadXml("<r xml:space='preserve'/>"));
}

TEST(XmlContractPinTests, Fix2083_ADeclarationDoesNotEscapeItsElement) {
    // Scope is the element it appears on and its descendants -- not its siblings. A declaration
    // that leaked would make this document look well-formed when it is not.
    XmlDocument d;
    EXPECT_THROW(d.LoadXml("<r><a xmlns:p='urn:x'/><p:b/></r>"), System::Xml::XmlException);
}

// ===========================================================================
// §7's two SECURITY POSITIVES — recorded as non-findings, pinned so they stay true
// ===========================================================================

TEST(XmlContractPinTests, InternalEntitiesAreNeverExpanded_NoBillionLaughsExposure) {
    // A parity gap (DTD entities unsupported), NOT a vulnerability — and the compatible queue
    // must not have turned it into one.
    XmlDocument d;
    ASSERT_NO_THROW(d.LoadXml(
        "<!DOCTYPE r [<!ENTITY a \"aaaaaaaaaa\"><!ENTITY b \"&a;&a;&a;&a;&a;\">]><r>&b;</r>"));
    auto* root = d.getDocumentElementProperty();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->getInnerTextProperty(), "&b;") << "the reference stays inert literal text";
}

TEST(XmlContractPinTests, NestingDepthIsBounded_NoStackOverflowThroughLoadXml) {
    std::string deep;
    for (int i = 0; i < 2000; ++i) deep += "<a>";
    for (int i = 0; i < 2000; ++i) deep += "</a>";
    XmlDocument d;
    EXPECT_ANY_THROW(d.LoadXml(deep)) << "the substrate must keep enforcing a depth bound";
}

TEST(XmlContractPinTests, ADuplicateAttributeAndAnEmbeddedNulAreStillRejected) {
    {
        XmlDocument d;
        EXPECT_ANY_THROW(d.LoadXml("<r a='1' a='2'/>"));
    }
    {
        XmlDocument d;
        EXPECT_ANY_THROW(d.LoadXml(std::string("<r>a\0b</r>", 10)));
    }
}
