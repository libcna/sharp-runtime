// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2323 (SR-AUD-092). See HttpRequestExceptionFallbackTests for the shared rationale;
// this is the second of the two subclasses that reach `System::Exception`'s fallback.
//
// .NET's JsonException() is `: base() { }` (JsonException.cs:78) and its Message override is
// `_message ?? base.Message` (:141-147), so it reaches Exception's formatted default and names
// THIS type.
#include <gtest/gtest.h>
#include <string>
#include "System/Text/Json/JsonException.hpp"

TEST(JsonExceptionFallbackTests, DefaultCtorNamesThisTypeNotTheBase) {
    const System::Text::Json::JsonException e;
    EXPECT_EQ(e.getMessageProperty(),
              "Exception of type 'System.Text.Json.JsonException' was thrown.");
    EXPECT_NE(e.getMessageProperty(), "Exception of type 'System.Exception' was thrown.")
        << "inheriting the base's string would name the wrong type";
}

TEST(JsonExceptionFallbackTests, AnExplicitlyEmptyMessageStaysEmpty) {
    EXPECT_TRUE(System::Text::Json::JsonException("").getMessageProperty().empty());
    EXPECT_EQ(System::Text::Json::JsonException("boom").getMessageProperty(), "boom");
}
