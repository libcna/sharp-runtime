// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <limits>
#include "System/AppContext.hpp"
#include "System/String.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace {
constexpr const char* Switch = "SharpRuntime.UseNetFrameworkArgumentExceptionMessages";
class FrameworkExceptionCompatibility : public ::testing::Test {
    bool previous_ = false;
    void SetUp() override { System::AppContext::TryGetSwitch(Switch, previous_); }
    void TearDown() override { System::AppContext::SetSwitch(Switch, previous_); }
};
TEST_F(FrameworkExceptionCompatibility, ModernFormatRemainsTheDefaultWhenDisabled) {
    System::AppContext::SetSwitch(Switch, false);
    EXPECT_EQ(System::ArgumentException("bad", "value").getMessageProperty(), "bad (Parameter 'value')");
    EXPECT_EQ(System::ArgumentOutOfRangeException("value", "bad").getParamNameProperty(), "value");
}
TEST_F(FrameworkExceptionCompatibility, FrameworkFormatHasSeparateParameterLine) {
    System::AppContext::SetSwitch(Switch, true);
    EXPECT_EQ(System::ArgumentException("bad", "value").getMessageProperty(), "bad\nParameter name: value");
    EXPECT_EQ(System::ArgumentException("bad", "").getMessageProperty(), "bad");
    EXPECT_EQ(System::ArgumentOutOfRangeException("value", "3", "bad").getMessageProperty(),
              "bad\nParameter name: value\nActual value was 3.");
}
TEST_F(FrameworkExceptionCompatibility, SubstringMatchesMeasuredDotNet4Diagnostic) {
    System::AppContext::SetSwitch(Switch, true);
    try { System::String::Substring("echo", 5); FAIL(); }
    catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getMessageProperty(), "startIndex cannot be larger than length of string.\nParameter name: startIndex");
        EXPECT_EQ(e.getParamNameProperty(), "startIndex");
    }
    try { System::String::Substring("echo", -1); FAIL(); }
    catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getMessageProperty(), "StartIndex cannot be less than zero.\nParameter name: startIndex");
    }
    EXPECT_EQ(System::String::Substring("echo", 4), "");
}
TEST_F(FrameworkExceptionCompatibility, SubstringValidatesStartThenLengthWithoutOverflow) {
    System::AppContext::SetSwitch(Switch, false);
    for (const int length : {-1, std::numeric_limits<int>::max()}) {
        try { System::String::Substring("abc", 1, length); FAIL(); }
        catch (const System::ArgumentOutOfRangeException& e) { EXPECT_EQ(e.getParamNameProperty(), "length"); }
    }
    try { System::String::Substring("abc", 4, -1); FAIL(); }
    catch (const System::ArgumentOutOfRangeException& e) { EXPECT_EQ(e.getParamNameProperty(), "startIndex"); }
    EXPECT_EQ(System::String::Substring("abc", 3, 0), "");
}
TEST_F(FrameworkExceptionCompatibility, ModernSubstringKeepsModernSuffix) {
    System::AppContext::SetSwitch(Switch, false);
    try { System::String::Substring("echo", 5); FAIL(); }
    catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getMessageProperty(), "startIndex cannot be larger than length of string. (Parameter 'startIndex')");
    }
}
}
