// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// SerializationInfo is a name-to-value store: the one part of .NET's legacy serialization pattern a
// ported type's `(SerializationInfo, StreamingContext)` constructor and `GetObjectData` actually
// need. These cases pin the rules that a type written against .NET depends on -- add once, read by
// the type it was stored as, and a SerializationException for anything else -- rather than only the
// happy path.

#include <gtest/gtest.h>

#include <any>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

#include "System/Runtime/Serialization/SerializationException.hpp"
#include "System/Runtime/Serialization/SerializationInfo.hpp"
#include "System/Runtime/Serialization/StreamingContext.hpp"
#include "System/Runtime/Serialization/StreamingContextStates.hpp"

using System::Runtime::Serialization::SerializationException;
using System::Runtime::Serialization::SerializationInfo;
using System::Runtime::Serialization::StreamingContext;
using System::Runtime::Serialization::StreamingContextStates;

TEST(SerializationInfoTests, StoresAndRetrievesEveryPrimitiveShape) {
    SerializationInfo info;
    info.AddValue("flag", true);
    info.AddValue("count", static_cast<SharpRuntime::intcs>(42));
    info.AddValue("big", static_cast<SharpRuntime::longcs>(-9000000000LL));
    info.AddValue("ratio", 0.5F);
    info.AddValue("precise", 0.125);
    info.AddValue("name", std::string("Ada"));

    EXPECT_TRUE(info.GetBoolean("flag"));
    EXPECT_EQ(info.GetInt32("count"), 42);
    EXPECT_EQ(info.GetInt64("big"), -9000000000LL);
    EXPECT_FLOAT_EQ(info.GetSingle("ratio"), 0.5F);
    EXPECT_DOUBLE_EQ(info.GetDouble("precise"), 0.125);
    EXPECT_EQ(info.GetString("name"), "Ada");
    EXPECT_EQ(info.getMemberCountProperty(), 6);
}

TEST(SerializationInfoTests, ANameMayBeAddedOnlyOnce) {
    SerializationInfo info;
    info.AddValue("count", static_cast<SharpRuntime::intcs>(1));

    // .NET throws SerializationException on a repeat, rather than replacing the value.
    EXPECT_THROW(info.AddValue("count", static_cast<SharpRuntime::intcs>(2)),
                 SerializationException);
    EXPECT_EQ(info.GetInt32("count"), 1);
    EXPECT_EQ(info.getMemberCountProperty(), 1);
}

TEST(SerializationInfoTests, AnEmptyNameIsRefused) {
    SerializationInfo info;
    EXPECT_THROW(info.AddValue("", static_cast<SharpRuntime::intcs>(1)), SerializationException);
    EXPECT_EQ(info.getMemberCountProperty(), 0);
}

TEST(SerializationInfoTests, AnAbsentNameIsASerializationException) {
    SerializationInfo info;
    info.AddValue("present", static_cast<SharpRuntime::intcs>(1));

    EXPECT_THROW((void)info.GetInt32("absent"), SerializationException);
    EXPECT_THROW((void)info.GetValue("absent"), SerializationException);
    EXPECT_THROW((void)info.GetValue<std::string>("absent"), SerializationException);
}

TEST(SerializationInfoTests, ReadingAValueAsAnotherTypeIsASerializationException) {
    SerializationInfo info;
    info.AddValue("count", static_cast<SharpRuntime::intcs>(7));

    EXPECT_THROW((void)info.GetString("count"), SerializationException);
    EXPECT_THROW((void)info.GetInt64("count"), SerializationException)
        << "an Int32 is not an Int64: a value is read as the type it was stored as";
    EXPECT_EQ(info.GetInt32("count"), 7);
}

TEST(SerializationInfoTests, NamesAreMatchedExactlyIncludingCase) {
    SerializationInfo info;
    info.AddValue("Count", static_cast<SharpRuntime::intcs>(3));

    EXPECT_TRUE(info.Contains("Count"));
    EXPECT_FALSE(info.Contains("count"));
    EXPECT_THROW((void)info.GetInt32("count"), SerializationException);

    // Which also means the two are separate names.
    info.AddValue("count", static_cast<SharpRuntime::intcs>(4));
    EXPECT_EQ(info.GetInt32("Count"), 3);
    EXPECT_EQ(info.GetInt32("count"), 4);
}

TEST(SerializationInfoTests, NamesArePreservedInAdditionOrder) {
    SerializationInfo info;
    info.AddValue("first", static_cast<SharpRuntime::intcs>(1));
    info.AddValue("second", std::string("two"));
    info.AddValue("third", true);

    EXPECT_EQ(info.GetNames(), (std::vector<std::string>{"first", "second", "third"}));
}

TEST(SerializationInfoTests, AnExceptionPointerRoundTripsAsAStoredValue) {
    // What an exception's own serialization needs: its inner cause travels with it, and rethrows as
    // the type it was.
    std::exception_ptr cause;
    try {
        throw std::runtime_error("the underlying failure");
    } catch (...) {
        cause = std::current_exception();
    }

    SerializationInfo info;
    info.AddValue("InnerException", cause);

    const auto restored = info.GetValue<std::exception_ptr>("InnerException");
    ASSERT_NE(restored, nullptr);
    try {
        std::rethrow_exception(restored);
        FAIL() << "the restored cause must rethrow";
    } catch (const std::runtime_error& error) {
        EXPECT_EQ(std::string(error.what()), "the underlying failure");
    }
}

TEST(SerializationInfoTests, TheTypeNameAndAssemblyNameRoundTrip) {
    SerializationInfo unnamed;
    EXPECT_TRUE(unnamed.getFullTypeNameProperty().empty());
    EXPECT_TRUE(unnamed.getAssemblyNameProperty().empty());

    SerializationInfo named("Some.Namespace.SomeType", "Some.Assembly");
    EXPECT_EQ(named.getFullTypeNameProperty(), "Some.Namespace.SomeType");
    EXPECT_EQ(named.getAssemblyNameProperty(), "Some.Assembly");

    named.setFullTypeNameProperty("Other.Type");
    named.setAssemblyNameProperty("Other.Assembly");
    EXPECT_EQ(named.getFullTypeNameProperty(), "Other.Type");
    EXPECT_EQ(named.getAssemblyNameProperty(), "Other.Assembly");
}

TEST(SerializationInfoTests, SerializationExceptionCarriesItsMessageAndCause) {
    const SerializationException bare;
    EXPECT_FALSE(bare.getMessageProperty().empty());

    const SerializationException described("a specific problem");
    EXPECT_EQ(described.getMessageProperty(), "a specific problem");
    EXPECT_EQ(described.getInnerExceptionProperty(), nullptr);

    std::exception_ptr cause;
    try {
        throw std::runtime_error("root cause");
    } catch (...) {
        cause = std::current_exception();
    }
    const SerializationException chained("outer", cause);
    EXPECT_EQ(chained.getMessageProperty(), "outer");
    EXPECT_NE(chained.getInnerExceptionProperty(), nullptr);

    // Still a System::Exception, so an ordinary handler catches it.
    EXPECT_THROW({ throw SerializationException("thrown"); }, System::Exception);
}

TEST(StreamingContextTests, CarriesItsStateAndContextObject) {
    const StreamingContext defaulted;
    EXPECT_EQ(defaulted.getStateProperty(), StreamingContextStates::All);
    EXPECT_FALSE(defaulted.getContextProperty().has_value());

    const StreamingContext fileContext(StreamingContextStates::File);
    EXPECT_EQ(fileContext.getStateProperty(), StreamingContextStates::File);

    const StreamingContext withObject(StreamingContextStates::Clone,
                                      std::any(std::string("caller state")));
    EXPECT_EQ(withObject.getStateProperty(), StreamingContextStates::Clone);
    ASSERT_TRUE(withObject.getContextProperty().has_value());
    EXPECT_EQ(std::any_cast<std::string>(withObject.getContextProperty()), "caller state");
}

TEST(StreamingContextTests, StatesCombineAndTestAsFlags) {
    // .NET's StreamingContextStates is a [Flags] enum with these exact values, so a caller that
    // combines or tests them reads the same numbers.
    EXPECT_EQ(static_cast<int>(StreamingContextStates::None), 0);
    EXPECT_EQ(static_cast<int>(StreamingContextStates::File), 4);
    EXPECT_EQ(static_cast<int>(StreamingContextStates::All), 255);

    const StreamingContextStates combined =
        StreamingContextStates::File | StreamingContextStates::Persistence;
    EXPECT_EQ(static_cast<int>(combined), 12);
    EXPECT_EQ(combined & StreamingContextStates::File, StreamingContextStates::File);
    EXPECT_EQ(combined & StreamingContextStates::Clone, StreamingContextStates::None);
}
