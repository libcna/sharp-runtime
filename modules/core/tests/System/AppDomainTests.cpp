// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// The module-owned System::AppDomain fixture, added by ticket #2248. These
// cover the two members #2249 and #2251 repaired and pin the one #2250
// deliberately left in place.
//
// PREMISE CORRECTION to the audit report, which says "no test invokes any of
// these public AppDomain members": an AppDomainTests suite already existed in
// tests/integration/Task42Tests.cpp and did invoke SetData/GetData, ApplyPolicy
// and IsCompatibilitySwitchSet -- and its SetGetData_Stubs_NoThrow asserted
// GetData(...) == nullptr right after a SetData, i.e. it PINNED the stub. It is
// retired and inverted by #2249. The auditor's validation filter was
// AppContextExtraTests.*:AppDomainSetupTests.*, which does not select it.
//
// The suite here is named AppDomainDataPolicyTests rather than AppDomainTests so
// that the two fixtures, which live in different executables, do not share a
// name in filters and logs.
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

TEST(AppDomainDataPolicyTests, GetData_ReadsWhatAppContextStored) {
    AppContext::SetData("AppDomainDataPolicyTests.readsContext", &payloadA);
    EXPECT_EQ(AppDomain::CurrentDomain().GetData("AppDomainDataPolicyTests.readsContext"), &payloadA);
}

TEST(AppDomainDataPolicyTests, SetData_IsVisibleThroughAppContext) {
    AppDomain::CurrentDomain().SetData("AppDomainDataPolicyTests.toContext", &payloadB);
    EXPECT_EQ(AppContext::GetData("AppDomainDataPolicyTests.toContext"), &payloadB);
}

TEST(AppDomainDataPolicyTests, SetData_RoundTripsThroughTheDomainItself) {
    AppDomain& domain = AppDomain::CurrentDomain();
    domain.SetData("AppDomainDataPolicyTests.roundTrip", &payloadA);
    EXPECT_EQ(domain.GetData("AppDomainDataPolicyTests.roundTrip"), &payloadA);
    domain.SetData("AppDomainDataPolicyTests.roundTrip", &payloadB);
    EXPECT_EQ(domain.GetData("AppDomainDataPolicyTests.roundTrip"), &payloadB);
}

TEST(AppDomainDataPolicyTests, GetData_UnknownName_ReturnsNullptr) {
    EXPECT_EQ(AppDomain::CurrentDomain().GetData("AppDomainDataPolicyTests.neverStored"), nullptr);
}

TEST(AppDomainDataPolicyTests, SetData_NullValue_IsStoredAndReadBack) {
    AppDomain& domain = AppDomain::CurrentDomain();
    domain.SetData("AppDomainDataPolicyTests.nullValue", nullptr);
    EXPECT_EQ(domain.GetData("AppDomainDataPolicyTests.nullValue"), nullptr);
}

// ---------------------------------------------------------------------------
// SR-AUD-103, switch half / ticket #2250 (needs_user). This pins the CURRENT
// divergence deliberately: IsCompatibilitySwitchSet does not consult AppContext,
// because forwarding needs a nullable return type and a noexcept drop, both of
// which need approval. If #2250 is approved, this is the test that must change.
// ---------------------------------------------------------------------------

TEST(AppDomainDataPolicyTests, IsCompatibilitySwitchSet_DoesNotYetConsultAppContext) {
    AppContext::SetSwitch("AppDomainDataPolicyTests.switchOn", true);
    ASSERT_TRUE([] {
        bool enabled = false;
        return AppContext::TryGetSwitch("AppDomainDataPolicyTests.switchOn", enabled) && enabled;
    }()) << "positive control: AppContext itself must report the switch as set";
    EXPECT_FALSE(AppDomain::CurrentDomain().IsCompatibilitySwitchSet("AppDomainDataPolicyTests.switchOn"));
}

TEST(AppDomainDataPolicyTests, IsCompatibilitySwitchSet_IsStillNoexcept) {
    // The reference and the argument are bound outside the noexcept operand on
    // purpose: CurrentDomain() is not itself noexcept, and neither is the
    // const char* -> std::string conversion, so either one inside the operand
    // would answer a different question.
    AppDomain& domain = AppDomain::CurrentDomain();
    const std::string name("AppDomainDataPolicyTests.switchOn");
    EXPECT_TRUE(noexcept(domain.IsCompatibilitySwitchSet(name)));
    EXPECT_NO_THROW((void)domain.IsCompatibilitySwitchSet(std::string()));
}

// ---------------------------------------------------------------------------
// SR-AUD-104 / ticket #2251. ApplyPolicy returned every std::string unchanged,
// including the two forms .NET rejects before applying the identity route.
// ---------------------------------------------------------------------------

TEST(AppDomainDataPolicyTests, ApplyPolicy_ValidName_ReturnedUnchanged) {
    const std::string name = "System.Xml, Version=1.0.0.0, Culture=neutral";
    EXPECT_EQ(AppDomain::CurrentDomain().ApplyPolicy(name), name);
}

TEST(AppDomainDataPolicyTests, ApplyPolicy_EmptyName_ThrowsArgumentException) {
    EXPECT_THROW((void)AppDomain::CurrentDomain().ApplyPolicy(std::string()),
                 System::ArgumentException);
}

// The message matters, not only the type. For a const std::string&,
// `assemblyName[0]` with size() == 0 is well-defined and yields the null
// character, so the leading-NUL branch alone would already throw for an empty
// name -- with the wrong message. Asserting the message is what keeps the two
// branches distinguishable, and .NET distinguishes them too.
TEST(AppDomainDataPolicyTests, ApplyPolicy_EmptyName_CarriesTheParameterNameAndTheEmptyMessage) {
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

TEST(AppDomainDataPolicyTests, ApplyPolicy_LeadingNul_ThrowsArgumentException) {
    const std::string leadingNul("\0x", 2);
    ASSERT_EQ(leadingNul.size(), 2u) << "positive control: the argument is not itself empty";
    EXPECT_THROW((void)AppDomain::CurrentDomain().ApplyPolicy(leadingNul),
                 System::ArgumentException);
}

TEST(AppDomainDataPolicyTests, ApplyPolicy_LeadingNul_CarriesTheParameterNameAndTheZeroLengthMessage) {
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
TEST(AppDomainDataPolicyTests, ApplyPolicy_InteriorNul_ReturnedUnchanged) {
    const std::string interiorNul("a\0b", 3);
    const std::string result = AppDomain::CurrentDomain().ApplyPolicy(interiorNul);
    EXPECT_EQ(result, interiorNul);
    EXPECT_EQ(result.size(), 3u);
}

TEST(AppDomainDataPolicyTests, ApplyPolicy_SingleCharacterName_ReturnedUnchanged) {
    EXPECT_EQ(AppDomain::CurrentDomain().ApplyPolicy("A"), "A");
}
