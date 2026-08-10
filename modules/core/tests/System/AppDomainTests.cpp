// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// The first dedicated System::AppDomain fixture. Before ticket #2248 the audit
// recorded that no test invoked any of AppDomain's data, switch or policy
// members; these cover the two that #2249 and #2251 repaired, and pin the one
// #2250 deliberately left in place.
#include <gtest/gtest.h>

#include <string>

#include "System/AppContext.hpp"
#include "System/AppDomain.hpp"
#include "System/ArgumentException.hpp"

using System::AppContext;
using System::AppDomain;

namespace {
// Distinct keys per test: AppContext's store is process-wide and there is no
// public way to remove an entry, so tests must not share names.
int payloadA = 1;
int payloadB = 2;
}  // namespace

// ---------------------------------------------------------------------------
// SR-AUD-103, data half / ticket #2249. SetData stored nothing and GetData
// always returned nullptr, so a one-domain AppDomain could neither observe nor
// set the AppContext state it is the same store as.
// ---------------------------------------------------------------------------

TEST(AppDomainTests, GetData_ReadsWhatAppContextStored) {
    AppContext::SetData("AppDomainTests.readsContext", &payloadA);
    EXPECT_EQ(AppDomain::CurrentDomain().GetData("AppDomainTests.readsContext"), &payloadA);
}

TEST(AppDomainTests, SetData_IsVisibleThroughAppContext) {
    AppDomain::CurrentDomain().SetData("AppDomainTests.toContext", &payloadB);
    EXPECT_EQ(AppContext::GetData("AppDomainTests.toContext"), &payloadB);
}

TEST(AppDomainTests, SetData_RoundTripsThroughTheDomainItself) {
    AppDomain& domain = AppDomain::CurrentDomain();
    domain.SetData("AppDomainTests.roundTrip", &payloadA);
    EXPECT_EQ(domain.GetData("AppDomainTests.roundTrip"), &payloadA);
    domain.SetData("AppDomainTests.roundTrip", &payloadB);
    EXPECT_EQ(domain.GetData("AppDomainTests.roundTrip"), &payloadB);
}

TEST(AppDomainTests, GetData_UnknownName_ReturnsNullptr) {
    EXPECT_EQ(AppDomain::CurrentDomain().GetData("AppDomainTests.neverStored"), nullptr);
}

TEST(AppDomainTests, SetData_NullValue_IsStoredAndReadBack) {
    AppDomain& domain = AppDomain::CurrentDomain();
    domain.SetData("AppDomainTests.nullValue", nullptr);
    EXPECT_EQ(domain.GetData("AppDomainTests.nullValue"), nullptr);
}

// ---------------------------------------------------------------------------
// SR-AUD-103, switch half / ticket #2250 (needs_user). This pins the CURRENT
// divergence deliberately: IsCompatibilitySwitchSet does not consult AppContext,
// because forwarding needs a nullable return type and a noexcept drop, both of
// which need approval. If #2250 is approved, this is the test that must change.
// ---------------------------------------------------------------------------

TEST(AppDomainTests, IsCompatibilitySwitchSet_DoesNotYetConsultAppContext) {
    AppContext::SetSwitch("AppDomainTests.switchOn", true);
    ASSERT_TRUE([] {
        bool enabled = false;
        return AppContext::TryGetSwitch("AppDomainTests.switchOn", enabled) && enabled;
    }()) << "positive control: AppContext itself must report the switch as set";
    EXPECT_FALSE(AppDomain::CurrentDomain().IsCompatibilitySwitchSet("AppDomainTests.switchOn"));
}

TEST(AppDomainTests, IsCompatibilitySwitchSet_IsStillNoexcept) {
    // The reference and the argument are bound outside the noexcept operand on
    // purpose: CurrentDomain() is not itself noexcept, and neither is the
    // const char* -> std::string conversion, so either one inside the operand
    // would answer a different question.
    AppDomain& domain = AppDomain::CurrentDomain();
    const std::string name("AppDomainTests.switchOn");
    EXPECT_TRUE(noexcept(domain.IsCompatibilitySwitchSet(name)));
    EXPECT_NO_THROW((void)domain.IsCompatibilitySwitchSet(std::string()));
}

// ---------------------------------------------------------------------------
// SR-AUD-104 / ticket #2251. ApplyPolicy returned every std::string unchanged,
// including the two forms .NET rejects before applying the identity route.
// ---------------------------------------------------------------------------

TEST(AppDomainTests, ApplyPolicy_ValidName_ReturnedUnchanged) {
    const std::string name = "System.Xml, Version=1.0.0.0, Culture=neutral";
    EXPECT_EQ(AppDomain::CurrentDomain().ApplyPolicy(name), name);
}

TEST(AppDomainTests, ApplyPolicy_EmptyName_ThrowsArgumentException) {
    EXPECT_THROW((void)AppDomain::CurrentDomain().ApplyPolicy(std::string()),
                 System::ArgumentException);
}

// The message matters, not only the type. For a const std::string&,
// `assemblyName[0]` with size() == 0 is well-defined and yields the null
// character, so the leading-NUL branch alone would already throw for an empty
// name -- with the wrong message. Asserting the message is what keeps the two
// branches distinguishable, and .NET distinguishes them too.
TEST(AppDomainTests, ApplyPolicy_EmptyName_CarriesTheParameterNameAndTheEmptyMessage) {
    try {
        (void)AppDomain::CurrentDomain().ApplyPolicy(std::string());
        FAIL() << "expected System::ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "assemblyName");
        EXPECT_NE(e.getMessageProperty().find("The value cannot be an empty string."),
                  std::string::npos)
            << "actual: " << e.getMessageProperty();
    }
}

TEST(AppDomainTests, ApplyPolicy_LeadingNul_ThrowsArgumentException) {
    const std::string leadingNul("\0x", 2);
    ASSERT_EQ(leadingNul.size(), 2u) << "positive control: the argument is not itself empty";
    EXPECT_THROW((void)AppDomain::CurrentDomain().ApplyPolicy(leadingNul),
                 System::ArgumentException);
}

TEST(AppDomainTests, ApplyPolicy_LeadingNul_CarriesTheParameterNameAndTheZeroLengthMessage) {
    const std::string leadingNul("\0x", 2);
    try {
        (void)AppDomain::CurrentDomain().ApplyPolicy(leadingNul);
        FAIL() << "expected System::ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "assemblyName");
        EXPECT_NE(e.getMessageProperty().find("String cannot be of zero length."),
                  std::string::npos)
            << "actual: " << e.getMessageProperty();
    }
}

// .NET checks assemblyName[0] only. A NUL anywhere else is a legal argument and
// is returned unchanged; this pins that the repair did not over-reach into a
// general "reject embedded NUL" rule.
TEST(AppDomainTests, ApplyPolicy_InteriorNul_ReturnedUnchanged) {
    const std::string interiorNul("a\0b", 3);
    const std::string result = AppDomain::CurrentDomain().ApplyPolicy(interiorNul);
    EXPECT_EQ(result, interiorNul);
    EXPECT_EQ(result.size(), 3u);
}

TEST(AppDomainTests, ApplyPolicy_SingleCharacterName_ReturnedUnchanged) {
    EXPECT_EQ(AppDomain::CurrentDomain().ApplyPolicy("A"), "A");
}
