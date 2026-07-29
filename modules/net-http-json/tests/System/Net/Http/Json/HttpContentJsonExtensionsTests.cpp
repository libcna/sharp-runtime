// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/ArgumentNullException.hpp"
#include "System/Net/Http/Json/HttpContentJsonExtensions.hpp"
#include "System/Net/Http/Json/JsonContent.hpp"
#include <memory>
#include <string>

using namespace System::Net::Http;
using namespace System::Net::Http::Json;

TEST(HttpContentJsonExtensionsTests, ReadFromJson_ParsesContent) {
    auto content = std::make_shared<JsonContent>("{\"answer\":42}");
    auto doc = HttpContentJsonExtensions::ReadFromJson(content);
    ASSERT_TRUE(doc != nullptr);
    EXPECT_EQ(doc->getRootElementProperty().GetProperty("answer").GetInt32(), 42);
}

TEST(HttpContentJsonExtensionsTests, ReadFromJsonAsync_ParsesContent) {
    auto content = std::make_shared<JsonContent>("{\"ok\":true}");
    auto task = HttpContentJsonExtensions::ReadFromJsonAsync(content);
    auto doc = task.getResultProperty();
    ASSERT_TRUE(doc != nullptr);
    EXPECT_TRUE(doc->getRootElementProperty().GetProperty("ok").GetBoolean());
}

// ===========================================================================
// Null content (ticket #1814 / SR-AUD-236)
// ===========================================================================
//
// Both entry points dereferenced the shared_ptr with no validity check: an empty
// one was a UBSan "member access within null pointer" followed by an ASan SEGV on
// address 0x0. Both now throw ArgumentNullException("content"), matching .NET's
// ArgumentNullException.ThrowIfNull(content) at the top of every public overload.

TEST(HttpContentJsonExtensionsTests, ReadFromJson_NullContent_ThrowsArgumentNull) {
    EXPECT_THROW(HttpContentJsonExtensions::ReadFromJson(std::shared_ptr<HttpContent>{}),
                 System::ArgumentNullException);
}

// The placement is what this case pins. .NET's ReadFromJsonAsync overloads are not
// async methods: they validate and only then call ReadFromJsonAsyncCore, so a null
// argument throws AT THE CALL. Here the guard must fire before the task is created,
// so the exception is never merely stored on a task the caller may not observe.
TEST(HttpContentJsonExtensionsTests, ReadFromJsonAsync_NullContent_ThrowsSynchronouslyAtTheCall) {
    EXPECT_THROW(HttpContentJsonExtensions::ReadFromJsonAsync(std::shared_ptr<HttpContent>{}),
                 System::ArgumentNullException);
}

TEST(HttpContentJsonExtensionsTests, ReadFromJson_NullContent_NamesTheContentParameter) {
    try {
        HttpContentJsonExtensions::ReadFromJson(std::shared_ptr<HttpContent>{});
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_NE(std::string(e.what()).find("content"), std::string::npos);
    }
}

TEST(HttpContentJsonExtensionsTests, ReadFromJsonAsync_NullContent_NamesTheContentParameter) {
    try {
        HttpContentJsonExtensions::ReadFromJsonAsync(std::shared_ptr<HttpContent>{});
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_NE(std::string(e.what()).find("content"), std::string::npos);
    }
}

// A rejected call must leave nothing behind and stay repeatable; before the fix the
// second iteration was unreachable because the first killed the process.
TEST(HttpContentJsonExtensionsTests, NullContent_RejectionIsRepeatable) {
    for (int i = 0; i < 3; ++i) {
        EXPECT_THROW(HttpContentJsonExtensions::ReadFromJson(std::shared_ptr<HttpContent>{}),
                     System::ArgumentNullException);
        EXPECT_THROW(HttpContentJsonExtensions::ReadFromJsonAsync(std::shared_ptr<HttpContent>{}),
                     System::ArgumentNullException);
    }
}

// Valid content on both entry points must be unaffected by the guards, including
// content whose body is the JSON literal `null` -- an empty shared_ptr and a
// document that parses to null are different things and must stay different.
TEST(HttpContentJsonExtensionsTests, NullGuard_DoesNotRejectContentHoldingJsonNull) {
    auto content = std::make_shared<JsonContent>("null");
    auto doc = HttpContentJsonExtensions::ReadFromJson(content);
    ASSERT_TRUE(doc != nullptr);
    auto task = HttpContentJsonExtensions::ReadFromJsonAsync(content);
    EXPECT_TRUE(task.getResultProperty() != nullptr);
}

// Empty content is not null content: it must still reach the parser and fail there
// the way it always did, rather than being absorbed by the new guard.
TEST(HttpContentJsonExtensionsTests, NullGuard_DoesNotSwallowEmptyContent) {
    auto content = std::make_shared<JsonContent>("");
    try {
        HttpContentJsonExtensions::ReadFromJson(content);
        FAIL() << "expected the parser to reject empty content";
    } catch (const System::ArgumentNullException&) {
        // A pass on a bare EXPECT_THROW(..., std::exception) would not distinguish the
        // parser's own rejection from the new guard misfiring on non-null content.
        FAIL() << "empty content must reach the parser, not the null-content guard";
    } catch (const std::exception&) {
        SUCCEED();
    }
}
