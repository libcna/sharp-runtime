// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2323 (SR-AUD-092). The companion of
// modules/core/tests/System/ExceptionNewTests.cpp's ExceptionFallbackMessageTests, in its own
// module because Core.Base does not depend on Net.Http and a test is not a reason to add a
// public component edge (#2354's rule).
//
// .NET's Exception.Message is `_message ?? SR.Format(SR.Exception_WasThrown, GetClassName())`
// (Exception.cs:61,65) -- "Exception of type '{0}' was thrown." (Strings.resx:2333) -- and .NET's
// HttpRequestException() is `{ }` (HttpRequestException.cs:10-11), so it reaches that fallback
// and {0} names THIS type. {0} is reflection, which this port permanently lacks, so it is
// resolved statically here rather than inherited from the base: a message naming the wrong type
// would be a lie, where the empty message this replaced was merely an absence.
#include <gtest/gtest.h>
#include <string>
#include "System/Net/Http/HttpRequestException.hpp"

TEST(HttpRequestExceptionFallbackTests, DefaultCtorNamesThisTypeNotTheBase) {
    const System::Net::Http::HttpRequestException e;
    EXPECT_EQ(e.getMessageProperty(),
              "Exception of type 'System.Net.Http.HttpRequestException' was thrown.");
    EXPECT_NE(e.getMessageProperty(), "Exception of type 'System.Exception' was thrown.")
        << "inheriting the base's string would name the wrong type";
}

TEST(HttpRequestExceptionFallbackTests, AnExplicitlyEmptyMessageStaysEmpty) {
    // .NET's fallback fires on a NULL message, not an empty one.
    EXPECT_TRUE(System::Net::Http::HttpRequestException("").getMessageProperty().empty());
    EXPECT_EQ(System::Net::Http::HttpRequestException("boom").getMessageProperty(), "boom");
}
