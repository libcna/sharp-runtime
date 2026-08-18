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
#include <any>
#include <optional>

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
    // #2255 typed the store; these forwarders carry a std::any now.
    AppContext::SetData("AppDomainDataPolicyTests.readsContext", &payloadA);
    EXPECT_EQ(&payloadA, std::any_cast<int*>(
                             AppDomain::CurrentDomain().GetData("AppDomainDataPolicyTests.readsContext")));
}

TEST(AppDomainDataPolicyTests, SetData_IsVisibleThroughAppContext) {
    AppDomain::CurrentDomain().SetData("AppDomainDataPolicyTests.toContext", &payloadB);
    EXPECT_EQ(&payloadB,
              std::any_cast<int*>(AppContext::GetData("AppDomainDataPolicyTests.toContext")));
}

TEST(AppDomainDataPolicyTests, SetData_RoundTripsThroughTheDomainItself) {
    AppDomain& domain = AppDomain::CurrentDomain();
    domain.SetData("AppDomainDataPolicyTests.roundTrip", &payloadA);
    EXPECT_EQ(&payloadA, std::any_cast<int*>(domain.GetData("AppDomainDataPolicyTests.roundTrip")));
    domain.SetData("AppDomainDataPolicyTests.roundTrip", &payloadB);
    EXPECT_EQ(&payloadB, std::any_cast<int*>(domain.GetData("AppDomainDataPolicyTests.roundTrip")));
}

TEST(AppDomainDataPolicyTests, GetData_UnknownName_ReturnsAnEmptyAny) {
    EXPECT_FALSE(
        AppDomain::CurrentDomain().GetData("AppDomainDataPolicyTests.neverStored").has_value());
}

TEST(AppDomainDataPolicyTests, SetData_NullValue_IsStoredAndReadBack) {
    AppDomain& domain = AppDomain::CurrentDomain();
    // #2255: a stored null POINTER now HAS a value, where the void* store made it
    // indistinguishable from an absent key.
    domain.SetData("AppDomainDataPolicyTests.nullValue", static_cast<int*>(nullptr));
    const std::any stored = domain.GetData("AppDomainDataPolicyTests.nullValue");
    EXPECT_TRUE(stored.has_value());
    EXPECT_TRUE(std::any_cast<int*>(stored) == nullptr);
}

// ---------------------------------------------------------------------------
// SR-AUD-103, switch half / ticket #2250 — SHIPPED, and both pins below inverted
//
// IsCompatibilitySwitchSet used to `return false` unconditionally, without consulting the switch
// registry at all -- so a switch a caller had explicitly SET TO TRUE still reported as unset.
// .NET's is `AppContext.TryGetSwitch(value, out bool result) ? result : default(bool?)`
// (`AppDomain.cs:171-174`).
//
// Following it needed two approval-bound changes TOGETHER, and SA-10 covers both: the return type
// had to become nullable, because a C++ bool cannot distinguish an explicitly-FALSE switch from
// an UNSET one -- which is the whole reason .NET's is `bool?` -- and the noexcept had to go,
// because AppContext::TryGetSwitch raises for an empty name and takes a mutex whose lock() can
// throw. Forwarding from a noexcept member would have turned both into std::terminate, so the
// drop is the only safe way to forward at all rather than a stylistic relaxation.
// ---------------------------------------------------------------------------

TEST(AppDomainDataPolicyTests, Fix2250_IsCompatibilitySwitchSetConsultsAppContext) {
    AppContext::SetSwitch("AppDomainDataPolicyTests.switchOn", true);
    ASSERT_TRUE([] {
        bool enabled = false;
        return AppContext::TryGetSwitch("AppDomainDataPolicyTests.switchOn", enabled) && enabled;
    }()) << "positive control: AppContext itself must report the switch as set";
    EXPECT_EQ(std::optional<bool>(true),
              AppDomain::CurrentDomain().IsCompatibilitySwitchSet("AppDomainDataPolicyTests.switchOn"));
}

TEST(AppDomainDataPolicyTests, Fix2250_ExplicitlyFalseIsNotUnset) {
    // THE DISTINCTION THE NULLABLE RETURN EXISTS FOR, and the reason a bool could not have
    // carried the repair: these two states used to be one, and both used to read as `false`.
    AppContext::SetSwitch("AppDomainDataPolicyTests.switchOff", false);
    AppDomain& domain = AppDomain::CurrentDomain();

    EXPECT_EQ(std::optional<bool>(false),
              domain.IsCompatibilitySwitchSet("AppDomainDataPolicyTests.switchOff"));
    EXPECT_EQ(std::nullopt,
              domain.IsCompatibilitySwitchSet("AppDomainDataPolicyTests.neverSet"));
    EXPECT_NE(domain.IsCompatibilitySwitchSet("AppDomainDataPolicyTests.switchOff"),
              domain.IsCompatibilitySwitchSet("AppDomainDataPolicyTests.neverSet"))
        << "explicitly false and unset were indistinguishable before #2250";
}

TEST(AppDomainDataPolicyTests, Fix2250_ItIsNoLongerNoexceptAndTheDiagnosticReachesTheCaller) {
    // The reference and the argument are bound outside the noexcept operand on purpose:
    // CurrentDomain() is not itself noexcept, and neither is the const char* -> std::string
    // conversion, so either one inside the operand would answer a different question.
    AppDomain& domain = AppDomain::CurrentDomain();
    const std::string name("AppDomainDataPolicyTests.switchOn");
    EXPECT_FALSE(noexcept(domain.IsCompatibilitySwitchSet(name)));

    // An empty name used to be swallowed by the unconditional `false`. It now reaches the caller
    // as AppContext::TryGetSwitch's own diagnostic -- which is what the noexcept drop buys, and
    // why keeping the noexcept would have meant std::terminate instead.
    EXPECT_THROW((void)domain.IsCompatibilitySwitchSet(std::string()), System::ArgumentException);
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

// #2252 resolved this message against the reference tree, and it is NOT the one #2251
// assumed. AppDomain.cs:104-110 throws ArgumentException(SR.Argument_EmptyString,
// nameof(assemblyName)) for the leading NUL -- the SAME resource its empty-name check
// reaches through ArgumentException.ThrowIfNullOrEmpty -- and Argument_StringZeroLength,
// the .NET Framework-era string #2251 quoted, does not exist anywhere in the tree.
// So the two rejections are deliberately indistinguishable by message, and the test that
// used to assert they differed is what was wrong.
TEST(AppDomainDataPolicyTests, ApplyPolicy_LeadingNul_CarriesTheParameterNameAndTheEmptyStringMessage) {
    const std::string leadingNul("\0x", 2);
    try {
        (void)AppDomain::CurrentDomain().ApplyPolicy(leadingNul);
        FAIL() << "expected System::ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "assemblyName");
        EXPECT_NE(e.getMessageProperty().find("The value cannot be an empty string."),
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
