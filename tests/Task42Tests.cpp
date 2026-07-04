// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Task 42: Timer (fixed), Object, Type, String, Byte, UInt64, AppContext, AppDomain,
// GC, Debugger, Comparer (non-generic), Generic::Comparer, Generic::EqualityComparer,
// Stream, TextReader, TextWriter, NonCryptographicHashAlgorithm (via Crc32),
// JsonElement (direct), EncodingProvider, TimeZone, SharpRuntimeHelper typedefs,
// Action/Func/Predicate, MarshalByRefObject, ThreadStart, ApplicationId,
// IsolatedStorage, GenericMathInterfaces, KeyedCollection.
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "System/Threading/Timer.hpp"
#include "System/Object.hpp"
#include "System/Type.hpp"
#include "System/String.hpp"
#include "System/Byte.hpp"
#include "System/UInt64.hpp"
#include "System/AppContext.hpp"
#include "System/AppDomain.hpp"
#include "System/GC.hpp"
#include "System/Diagnostics/Debugger.hpp"
#include "System/Collections/Comparer.hpp"
#include "System/Collections/Generic/Comparer.hpp"
#include "System/IO/Stream.hpp"
#include "System/IO/TextReader.hpp"
#include "System/IO/TextWriter.hpp"
#include "System/IO/Hashing/NonCryptographicHashAlgorithm.hpp"
#include "System/IO/Hashing/Crc32.hpp"
#include "System/IO/Hashing/XxHash32.hpp"
#include "System/IO/Hashing/XxHash64.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorage.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageScope.hpp"
#include "System/Text/Json/JsonElement.hpp"
#include "System/Text/Json/JsonValueKind.hpp"
#include "System/Text/EncodingProvider.hpp"
#include "System/Text/Encoding.hpp"
#include "System/TimeZone.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Action.hpp"
#include "System/Func.hpp"
#include "System/Predicate.hpp"
#include "System/MarshalByRefObject.hpp"
#include "System/Threading/ThreadStart.hpp"
#include "System/ApplicationId.hpp"
#include "System/Numerics/GenericMathInterfaces.hpp"
#include "System/Collections/ObjectModel/KeyedCollection.hpp"
#include "System/Version.hpp"

// ===========================================================================
// Timer — fixed shared_ptr<State>, no dangling-this
// ===========================================================================

TEST(TimerTests, FiresCallbackBeforeDispose) {
    std::atomic<int> count{0};
    {
        System::Threading::Timer t(
            [](void* s){ (*static_cast<std::atomic<int>*>(s))++; },
            &count, 0, 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(55));
        // at 0 ms first fire, then every 20 ms → expect ≥2 firings in 55 ms
        EXPECT_GE(count.load(), 2);
    }
    // After Timer is destroyed, thread still holds shared_ptr — no UB.
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
}

TEST(TimerTests, DisposeStopsFiring) {
    std::atomic<int> count{0};
    auto t = std::make_unique<System::Threading::Timer>(
        [](void* s){ (*static_cast<std::atomic<int>*>(s))++; },
        &count, 0, 10);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    t->Dispose();
    int snapshot = count.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(count.load(), snapshot);
}

TEST(TimerTests, FiresOnceWhenPeriodZero) {
    std::atomic<int> count{0};
    {
        System::Threading::Timer t(
            [](void* s){ (*static_cast<std::atomic<int>*>(s))++; },
            &count, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
    EXPECT_EQ(count.load(), 1);
}

TEST(TimerTests, ChangeUpdatesInterval) {
    std::atomic<int> count{0};
    System::Threading::Timer t(
        [](void* s){ (*static_cast<std::atomic<int>*>(s))++; },
        &count, 0, 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    t.Change(-1, -1);   // disable
    int snapshot = count.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    // After disabling, the count should not grow much (allow one extra for in-flight)
    EXPECT_LE(count.load(), snapshot + 1);
}

// ===========================================================================
// Object — abstract base: concrete stub for testing
// ===========================================================================

namespace {
    struct TestObject : System::Object {
        const std::string& GetTypeName() const override {
            static const std::string name = "TestObject";
            return name;
        }
    };
}

TEST(ObjectTests, ToString_ReturnsTypeName) {
    TestObject o;
    EXPECT_EQ(o.ToString(), "TestObject");
}

TEST(ObjectTests, Equals_SameInstance) {
    TestObject o;
    EXPECT_TRUE(o.Equals(&o));
}

TEST(ObjectTests, Equals_DifferentInstances) {
    TestObject a, b;
    EXPECT_FALSE(a.Equals(&b));
}

TEST(ObjectTests, StaticEquals_NullNull) {
    EXPECT_TRUE(System::Object::Equals(nullptr, nullptr));
}

TEST(ObjectTests, StaticEquals_NullAndObject) {
    TestObject o;
    EXPECT_FALSE(System::Object::Equals(nullptr, &o));
    EXPECT_FALSE(System::Object::Equals(&o, nullptr));
}

TEST(ObjectTests, StaticEquals_SamePointer) {
    TestObject o;
    EXPECT_TRUE(System::Object::Equals(&o, &o));
}

TEST(ObjectTests, ReferenceEquals_SamePointer) {
    TestObject o;
    EXPECT_TRUE(System::Object::ReferenceEquals(&o, &o));
}

TEST(ObjectTests, ReferenceEquals_DifferentPointers) {
    TestObject a, b;
    EXPECT_FALSE(System::Object::ReferenceEquals(&a, &b));
}

TEST(ObjectTests, GetHashCode_NonNegative) {
    TestObject o;
    EXPECT_GE(o.GetHashCode(), 0);
}

TEST(ObjectTests, GetHashCode_ConsistentForSameObject) {
    TestObject o;
    EXPECT_EQ(o.GetHashCode(), o.GetHashCode());
}

// ===========================================================================
// Type
// ===========================================================================

TEST(TypeTests, DefaultCtor_IsNull) {
    System::Type t;
    EXPECT_EQ(t.getName(), "(null)");
}

TEST(TypeTests, FromInt_NotNull) {
    auto t = System::Type::From<int>();
    EXPECT_NE(t.getName(), "(null)");
}

TEST(TypeTests, SameType_Equal) {
    auto a = System::Type::From<int>();
    auto b = System::Type::From<int>();
    EXPECT_EQ(a, b);
}

TEST(TypeTests, DifferentTypes_NotEqual) {
    auto a = System::Type::From<int>();
    auto b = System::Type::From<double>();
    EXPECT_NE(a, b);
}

TEST(TypeTests, Hash_SameType) {
    auto a = System::Type::From<int>();
    auto b = System::Type::From<int>();
    std::hash<System::Type> h;
    EXPECT_EQ(h(a), h(b));
}

TEST(TypeTests, GetTypeInfo_NotNull) {
    auto t = System::Type::From<std::string>();
    EXPECT_NE(t.getTypeInfo(), nullptr);
}

TEST(TypeTests, GetNameProperty_NotNull) {
    auto t = System::Type::From<int>();
    EXPECT_NE(t.getNameProperty(), "(null)");
}

TEST(TypeTests, GetFullNameProperty_NotNull) {
    auto t = System::Type::From<double>();
    EXPECT_NE(t.getFullNameProperty(), "(null)");
}

TEST(TypeTests, ToString_NotNull) {
    auto t = System::Type::From<float>();
    EXPECT_NE(t.ToString(), "(null)");
}

TEST(TypeTests, GetHashCode_NonZero) {
    auto t = System::Type::From<int>();
    EXPECT_NE(t.GetHashCode(), 0u);
}

TEST(TypeTests, GetHashCode_SameTypeConsistent) {
    auto a = System::Type::From<std::string>();
    auto b = System::Type::From<std::string>();
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

TEST(TypeTests, DefaultCtor_GetHashCode_Zero) {
    System::Type t;
    EXPECT_EQ(t.GetHashCode(), 0u);
}

TEST(TypeTests, GetIsValueTypeProperty_False) {
    auto t = System::Type::From<int>();
    EXPECT_FALSE(t.getIsValueTypeProperty());
}

TEST(TypeTests, GetIsClassProperty_True) {
    auto t = System::Type::From<std::string>();
    EXPECT_TRUE(t.getIsClassProperty());
}

TEST(TypeTests, GetIsAbstractProperty_False) {
    auto t = System::Type::From<int>();
    EXPECT_FALSE(t.getIsAbstractProperty());
}

TEST(TypeTests, GetIsSealedProperty_False) {
    auto t = System::Type::From<int>();
    EXPECT_FALSE(t.getIsSealedProperty());
}

TEST(TypeTests, GetIsInterfaceProperty_False) {
    auto t = System::Type::From<int>();
    EXPECT_FALSE(t.getIsInterfaceProperty());
}

// ===========================================================================
// String (static utility)
// ===========================================================================

TEST(StringTests, IsEmpty_Empty) {
    EXPECT_TRUE(System::String::IsEmpty(""));
}

TEST(StringTests, IsEmpty_NonEmpty) {
    EXPECT_FALSE(System::String::IsEmpty("hello"));
}

TEST(StringTests, IsNullOrEmpty_Empty) {
    EXPECT_TRUE(System::String::IsNullOrEmpty(""));
}

TEST(StringTests, IsNullOrEmpty_NonEmpty) {
    EXPECT_FALSE(System::String::IsNullOrEmpty("x"));
}

TEST(StringTests, StartsWith_True) {
    EXPECT_TRUE(System::String::StartsWith("hello world", "hello"));
}

TEST(StringTests, StartsWith_False) {
    EXPECT_FALSE(System::String::StartsWith("hello world", "world"));
}

TEST(StringTests, Split_Basic) {
    auto parts = System::String::Split("a,b,c", ',');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
}

TEST(StringTests, Split_NoDelimiter) {
    auto parts = System::String::Split("abc", ',');
    ASSERT_EQ(parts.size(), 1u);
    EXPECT_EQ(parts[0], "abc");
}

TEST(StringTests, Format_IntArg) {
    std::string result = System::String::Format("val={0}", 42);
    EXPECT_EQ(result, "val=42");
}

TEST(StringTests, Format_StringArg) {
    std::string result = System::String::Format("hi {0}", std::string("world"));
    EXPECT_EQ(result, "hi world");
}

TEST(StringTests, ToString_PadsToWidth) {
    std::string r = System::String::ToString(7, 3, '0');
    EXPECT_EQ(r, "007");
}


// ===========================================================================
// Byte
// ===========================================================================

TEST(ByteTests, MaxMinValue) {
    EXPECT_EQ(System::Byte::MaxValue, 255u);
    EXPECT_EQ(System::Byte::MinValue, 0u);
}

TEST(ByteTests, Parse_Valid) {
    EXPECT_EQ(System::Byte::Parse("200"), 200u);
}

TEST(ByteTests, Parse_OutOfRange) {
    EXPECT_THROW(System::Byte::Parse("300"), std::out_of_range);
}

TEST(ByteTests, TryParse_Success) {
    SharpRuntime::bytecs v = 0;
    EXPECT_TRUE(System::Byte::TryParse("128", v));
    EXPECT_EQ(v, 128u);
}

TEST(ByteTests, TryParse_Failure) {
    SharpRuntime::bytecs v = 99;
    EXPECT_FALSE(System::Byte::TryParse("xyz", v));
    EXPECT_EQ(v, 0u);
}

TEST(ByteTests, ToString) {
    EXPECT_EQ(System::Byte::ToString(42u), "42");
}

// ===========================================================================
// UInt64
// ===========================================================================

TEST(UInt64Tests, MaxMinValue) {
    EXPECT_EQ(System::UInt64::MaxValue, std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(System::UInt64::MinValue, 0u);
}

TEST(UInt64Tests, Parse_Valid) {
    EXPECT_EQ(System::UInt64::Parse("12345678901234"), 12345678901234ULL);
}

TEST(UInt64Tests, TryParse_Success) {
    SharpRuntime::ulongcs v = 0;
    EXPECT_TRUE(System::UInt64::TryParse("9876543210", v));
    EXPECT_EQ(v, 9876543210ULL);
}

TEST(UInt64Tests, TryParse_Failure) {
    SharpRuntime::ulongcs v = 1;
    EXPECT_FALSE(System::UInt64::TryParse("notanumber", v));
    EXPECT_EQ(v, 0u);
}

TEST(UInt64Tests, ToString) {
    EXPECT_EQ(System::UInt64::ToString(0ULL), "0");
    EXPECT_EQ(System::UInt64::ToString(18446744073709551615ULL), "18446744073709551615");
}
TEST(UInt64Tests, ToString_Hex)           { EXPECT_EQ(System::UInt64::ToString(255ULL, std::string("X")), "FF"); }
TEST(UInt64Tests, ToString_Hex_Padded)    { EXPECT_EQ(System::UInt64::ToString(255ULL, std::string("X4")), "00FF"); }
TEST(UInt64Tests, ToString_D_Padded)      { EXPECT_EQ(System::UInt64::ToString(7ULL,   std::string("D5")), "00007"); }
TEST(UInt64Tests, ToString_G)             { EXPECT_EQ(System::UInt64::ToString(42ULL,  std::string("G")), "42"); }
TEST(UInt64Tests, CompareTo_Less)         { EXPECT_LT(System::UInt64::CompareTo(1ULL, 2ULL), 0); }
TEST(UInt64Tests, CompareTo_Equal)        { EXPECT_EQ(System::UInt64::CompareTo(5ULL, 5ULL), 0); }
TEST(UInt64Tests, CompareTo_Greater)      { EXPECT_GT(System::UInt64::CompareTo(9ULL, 3ULL), 0); }
TEST(UInt64Tests, Equals_True)            { EXPECT_TRUE(System::UInt64::Equals(42ULL, 42ULL)); }
TEST(UInt64Tests, Equals_False)           { EXPECT_FALSE(System::UInt64::Equals(1ULL, 2ULL)); }
TEST(UInt64Tests, GetHashCode_NonZero)    { EXPECT_NE(System::UInt64::GetHashCode(0x0001000200030004ULL), 0); }
TEST(UInt64Tests, Max)                    { EXPECT_EQ(System::UInt64::Max(10ULL, 20ULL), 20ULL); }
TEST(UInt64Tests, Min)                    { EXPECT_EQ(System::UInt64::Min(10ULL, 20ULL), 10ULL); }
TEST(UInt64Tests, Clamp_InRange)          { EXPECT_EQ(System::UInt64::Clamp(50ULL, 10ULL, 100ULL), 50ULL); }
TEST(UInt64Tests, Clamp_BelowMin)         { EXPECT_EQ(System::UInt64::Clamp(5ULL,  10ULL, 100ULL), 10ULL); }
TEST(UInt64Tests, Clamp_AboveMax)         { EXPECT_EQ(System::UInt64::Clamp(200ULL,10ULL, 100ULL), 100ULL); }
TEST(UInt64Tests, Sign_Zero)              { EXPECT_EQ(System::UInt64::Sign(0ULL), 0); }
TEST(UInt64Tests, Sign_Positive)          { EXPECT_EQ(System::UInt64::Sign(7ULL), 1); }
TEST(UInt64Tests, DivRem) {
    auto [q, r] = System::UInt64::DivRem(17ULL, 5ULL);
    EXPECT_EQ(q, 3ULL); EXPECT_EQ(r, 2ULL);
}
TEST(UInt64Tests, IsEvenInteger)          { EXPECT_TRUE(System::UInt64::IsEvenInteger(4ULL)); EXPECT_FALSE(System::UInt64::IsEvenInteger(3ULL)); }
TEST(UInt64Tests, IsOddInteger)           { EXPECT_TRUE(System::UInt64::IsOddInteger(7ULL));  EXPECT_FALSE(System::UInt64::IsOddInteger(8ULL)); }
TEST(UInt64Tests, IsPow2_True)            { EXPECT_TRUE(System::UInt64::IsPow2(1024ULL)); }
TEST(UInt64Tests, IsPow2_False)           { EXPECT_FALSE(System::UInt64::IsPow2(1000ULL)); }
TEST(UInt64Tests, IsPow2_Zero)            { EXPECT_FALSE(System::UInt64::IsPow2(0ULL)); }
TEST(UInt64Tests, Log2)                   { EXPECT_EQ(System::UInt64::Log2(1024ULL), 10ULL); }
TEST(UInt64Tests, Log2_Zero_IsZero)       { EXPECT_EQ(System::UInt64::Log2(0ULL), 0ULL); } // .NET's Log2(0) contract: returns 0, does not throw/wrap
TEST(UInt64Tests, LeadingZeroCount)       { EXPECT_EQ(System::UInt64::LeadingZeroCount(1ULL), 63ULL); }
TEST(UInt64Tests, TrailingZeroCount)      { EXPECT_EQ(System::UInt64::TrailingZeroCount(8ULL), 3ULL); }
TEST(UInt64Tests, TrailingZeroCount_Zero) { EXPECT_EQ(System::UInt64::TrailingZeroCount(0ULL), 64ULL); }
TEST(UInt64Tests, PopCount)               { EXPECT_EQ(System::UInt64::PopCount(0xFFULL), 8ULL); }
TEST(UInt64Tests, RotateLeft)             { EXPECT_EQ(System::UInt64::RotateLeft(1ULL, 1), 2ULL); }
TEST(UInt64Tests, RotateRight)            { EXPECT_EQ(System::UInt64::RotateRight(2ULL, 1), 1ULL); }

// ===========================================================================
// AppContext
// ===========================================================================

TEST(AppContextTests, BaseDirectoryProperty_NonEmpty) {
    const auto& dir = System::AppContext::getBaseDirectoryProperty();
    EXPECT_FALSE(dir.empty());
    EXPECT_EQ(dir.back(), '/');
}

TEST(AppContextTests, BaseDirectoryProperty_MatchesAppDomain) {
    EXPECT_EQ(System::AppContext::getBaseDirectoryProperty(),
              System::AppDomain::CurrentDomain().getBaseDirectoryProperty());
}

TEST(AppContextTests, TargetFrameworkName_IsEmpty) {
    EXPECT_TRUE(System::AppContext::getTargetFrameworkNameProperty().empty());
}

TEST(AppContextTests, SetGetData_RoundTrip) {
    int dummy = 42;
    System::AppContext::SetData("task42_key", &dummy);
    void* got = System::AppContext::GetData("task42_key");
    EXPECT_EQ(got, &dummy);
}

TEST(AppContextTests, GetData_MissingKey_ReturnsNull) {
    EXPECT_EQ(System::AppContext::GetData("__nonexistent_key_task42__"), nullptr);
}

TEST(AppContextTests, SetSwitch_TryGetSwitch) {
    System::AppContext::SetSwitch("task42.switch", true);
    bool enabled = false;
    bool found = System::AppContext::TryGetSwitch("task42.switch", enabled);
    EXPECT_TRUE(found);
    EXPECT_TRUE(enabled);
}

TEST(AppContextTests, SetSwitch_False_TryGetSwitch) {
    System::AppContext::SetSwitch("task42.switch.off", false);
    bool enabled = true;
    bool found = System::AppContext::TryGetSwitch("task42.switch.off", enabled);
    EXPECT_TRUE(found);
    EXPECT_FALSE(enabled);
}

TEST(AppContextTests, TryGetSwitch_Missing) {
    bool enabled = true;
    bool found = System::AppContext::TryGetSwitch("__no_such_switch_task42__", enabled);
    EXPECT_FALSE(found);
    EXPECT_FALSE(enabled);
}

TEST(AppContextTests, SetSwitch_EmptyName_Throws) {
    EXPECT_THROW(System::AppContext::SetSwitch("", true), std::invalid_argument);
}

TEST(AppContextTests, TryGetSwitch_EmptyName_Throws) {
    bool e = false;
    EXPECT_THROW(System::AppContext::TryGetSwitch("", e), std::invalid_argument);
}

// ===========================================================================
// AppDomain
// ===========================================================================

TEST(AppDomainTests, CurrentDomain_IsSingleton) {
    EXPECT_EQ(&System::AppDomain::CurrentDomain(), &System::AppDomain::CurrentDomain());
}

TEST(AppDomainTests, FriendlyName_NotEmpty) {
    EXPECT_FALSE(System::AppDomain::CurrentDomain().getFriendlyNameProperty().empty());
}

TEST(AppDomainTests, BaseDirectory_NotEmpty) {
    EXPECT_FALSE(System::AppDomain::CurrentDomain().getBaseDirectoryProperty().empty());
}

TEST(AppDomainTests, BaseDirectory_EndsWithSlash) {
    EXPECT_EQ(System::AppDomain::CurrentDomain().getBaseDirectoryProperty().back(), '/');
}

TEST(AppDomainTests, SetGetData_Stubs_NoThrow) {
    int x = 1;
    EXPECT_NO_THROW(System::AppDomain::CurrentDomain().SetData("k", &x));
    EXPECT_EQ(System::AppDomain::CurrentDomain().GetData("k"), nullptr);
}

TEST(AppDomainTests, Id_IsOne) {
    EXPECT_EQ(System::AppDomain::CurrentDomain().getIdProperty(), 1);
}

TEST(AppDomainTests, IsFullyTrusted_True) {
    EXPECT_TRUE(System::AppDomain::CurrentDomain().getIsFullyTrustedProperty());
}

TEST(AppDomainTests, IsHomogenous_True) {
    EXPECT_TRUE(System::AppDomain::CurrentDomain().getIsHomogenousProperty());
}

TEST(AppDomainTests, IsDefaultAppDomain_True) {
    EXPECT_TRUE(System::AppDomain::CurrentDomain().IsDefaultAppDomain());
}

TEST(AppDomainTests, IsFinalizingForUnload_False) {
    EXPECT_FALSE(System::AppDomain::CurrentDomain().IsFinalizingForUnload());
}

TEST(AppDomainTests, RelativeSearchPath_Empty) {
    EXPECT_TRUE(System::AppDomain::CurrentDomain().getRelativeSearchPathProperty().empty());
}

TEST(AppDomainTests, DynamicDirectory_Empty) {
    EXPECT_TRUE(System::AppDomain::CurrentDomain().getDynamicDirectoryProperty().empty());
}

TEST(AppDomainTests, ApplyPolicy_ReturnsInput) {
    EXPECT_EQ(System::AppDomain::CurrentDomain().ApplyPolicy("MyAssembly"), "MyAssembly");
}

TEST(AppDomainTests, ToString_ContainsFriendlyName) {
    std::string s = System::AppDomain::CurrentDomain().ToString();
    EXPECT_NE(s.find("DefaultDomain"), std::string::npos);
}

TEST(AppDomainTests, ShadowCopyFiles_False) {
    EXPECT_FALSE(System::AppDomain::CurrentDomain().getShadowCopyFilesProperty());
}

TEST(AppDomainTests, IsCompatibilitySwitchSet_ReturnsFalse) {
    EXPECT_FALSE(System::AppDomain::CurrentDomain().IsCompatibilitySwitchSet("SomeSwitch"));
}

TEST(AppDomainTests, GetCurrentThreadId_Positive) {
    EXPECT_GT(System::AppDomain::GetCurrentThreadId(), 0);
}

TEST(AppDomainTests, SetDynamicBase_NoThrow) {
    EXPECT_NO_THROW(System::AppDomain::CurrentDomain().SetDynamicBase("/tmp"));
}

TEST(AppDomainTests, AppendPrivatePath_NoThrow) {
    EXPECT_NO_THROW(System::AppDomain::CurrentDomain().AppendPrivatePath("/tmp"));
}

TEST(AppDomainTests, ClearPrivatePath_NoThrow) {
    EXPECT_NO_THROW(System::AppDomain::CurrentDomain().ClearPrivatePath());
}

TEST(AppDomainTests, ClearShadowCopyPath_NoThrow) {
    EXPECT_NO_THROW(System::AppDomain::CurrentDomain().ClearShadowCopyPath());
}

TEST(AppDomainTests, SetShadowCopyFiles_NoThrow) {
    EXPECT_NO_THROW(System::AppDomain::CurrentDomain().SetShadowCopyFiles());
}

TEST(AppDomainTests, SetShadowCopyPath_NoThrow) {
    EXPECT_NO_THROW(System::AppDomain::CurrentDomain().SetShadowCopyPath("/tmp"));
}

TEST(AppDomainTests, SetCachePath_NoThrow) {
    EXPECT_NO_THROW(System::AppDomain::CurrentDomain().SetCachePath("/tmp"));
}

TEST(AppDomainTests, AddRemoveDomainUnload_NoThrow) {
    auto handler = [](void*, System::EventArgs&) {};
    EXPECT_NO_THROW(System::AppDomain::CurrentDomain().add_DomainUnload(handler));
    EXPECT_NO_THROW(System::AppDomain::CurrentDomain().remove_DomainUnload(handler));
}

// ===========================================================================
// GC
// ===========================================================================

TEST(GCTests, Collect_NoThrow) {
    EXPECT_NO_THROW(System::GC::Collect());
    EXPECT_NO_THROW(System::GC::Collect(1));
}

TEST(GCTests, GetTotalMemory_ReturnsZero) {
    EXPECT_EQ(System::GC::GetTotalMemory(false), 0LL);
}

TEST(GCTests, MaxGeneration) {
    EXPECT_EQ(System::GC::MaxGeneration(), 2);
}

TEST(GCTests, GetGeneration_ReturnsZero) {
    int x = 0;
    EXPECT_EQ(System::GC::GetGeneration(&x), 0);
}

TEST(GCTests, KeepAlive_NoThrow) {
    int v = 5;
    EXPECT_NO_THROW(System::GC::KeepAlive(v));
}

TEST(GCTests, WaitForPendingFinalizers_NoThrow) {
    EXPECT_NO_THROW(System::GC::WaitForPendingFinalizers());
}

// ===========================================================================
// Debugger
// ===========================================================================

TEST(DebuggerTests, IsAttached_ReturnsFalse) {
    EXPECT_FALSE(System::Diagnostics::Debugger::getIsAttachedProperty());
}

TEST(DebuggerTests, Launch_ReturnsFalse) {
    EXPECT_FALSE(System::Diagnostics::Debugger::Launch());
}

TEST(DebuggerTests, IsLogging_ReturnsFalse) {
    EXPECT_FALSE(System::Diagnostics::Debugger::IsLogging());
}

TEST(DebuggerTests, Log_NoThrow) {
    EXPECT_NO_THROW(System::Diagnostics::Debugger::Log(0, "cat", "msg"));
}

// ===========================================================================
// Collections::Comparer (non-generic)
// ===========================================================================

TEST(NonGenericComparerTests, Default_IsSingleton) {
    EXPECT_EQ(&System::Collections::Comparer::Default(),
              &System::Collections::Comparer::Default());
}

TEST(NonGenericComparerTests, Compare_SamePointer_Zero) {
    int x = 0;
    EXPECT_EQ(System::Collections::Comparer::Default().Compare(&x, &x), 0);
}

TEST(NonGenericComparerTests, Compare_NullNull_Zero) {
    EXPECT_EQ(System::Collections::Comparer::Default().Compare(nullptr, nullptr), 0);
}

TEST(NonGenericComparerTests, Compare_NullVsNonNull) {
    int x = 0;
    EXPECT_LT(System::Collections::Comparer::Default().Compare(nullptr, &x), 0);
    EXPECT_GT(System::Collections::Comparer::Default().Compare(&x, nullptr), 0);
}

// ===========================================================================
// Collections::Generic::Comparer<T>
// ===========================================================================

TEST(GenericComparerTests, Default_IntLess) {
    auto& c = System::Collections::Generic::Comparer<int>::Default();
    EXPECT_LT(c.Compare(1, 2), 0);
}

TEST(GenericComparerTests, Default_IntEqual) {
    auto& c = System::Collections::Generic::Comparer<int>::Default();
    EXPECT_EQ(c.Compare(5, 5), 0);
}

TEST(GenericComparerTests, Default_IntGreater) {
    auto& c = System::Collections::Generic::Comparer<int>::Default();
    EXPECT_GT(c.Compare(10, 3), 0);
}

TEST(GenericComparerTests, Default_StringCompare) {
    auto& c = System::Collections::Generic::Comparer<std::string>::Default();
    EXPECT_LT(c.Compare("apple", "banana"), 0);
    EXPECT_EQ(c.Compare("same", "same"), 0);
    EXPECT_GT(c.Compare("z", "a"), 0);
}

// ===========================================================================
// Collections::Generic::EqualityComparer<T>
// ===========================================================================

TEST(EqualityComparerTests, Default_IntEquals) {
    auto& eq = System::Collections::Generic::EqualityComparer<int>::Default();
    EXPECT_TRUE(eq.Equals(42, 42));
    EXPECT_FALSE(eq.Equals(1, 2));
}

TEST(EqualityComparerTests, Default_StringEquals) {
    auto& eq = System::Collections::Generic::EqualityComparer<std::string>::Default();
    EXPECT_TRUE(eq.Equals("hello", "hello"));
    EXPECT_FALSE(eq.Equals("hello", "world"));
}

TEST(EqualityComparerTests, Default_GetHashCode_Consistent) {
    auto& eq = System::Collections::Generic::EqualityComparer<int>::Default();
    EXPECT_EQ(eq.GetHashCode(7), eq.GetHashCode(7));
}

TEST(EqualityComparerTests, Create_CustomEquals) {
    using EC = System::Collections::Generic::EqualityComparer<int>;
    auto custom = EC::Create([](const int& a, const int& b) { return (a % 2) == (b % 2); });
    EXPECT_TRUE(custom->Equals(2, 4));  // both even
    EXPECT_FALSE(custom->Equals(1, 2)); // odd vs even
}

TEST(EqualityComparerTests, Create_CustomHashCode) {
    using EC = System::Collections::Generic::EqualityComparer<int>;
    auto custom = EC::Create(
        [](const int& a, const int& b) { return a == b; },
        [](const int& v) { return static_cast<size_t>(v * 31); });
    EXPECT_EQ(custom->GetHashCode(5), static_cast<size_t>(155));
}

TEST(EqualityComparerTests, Default_ReturnsSameInstanceEveryCall) {
    using EC = System::Collections::Generic::EqualityComparer<int>;
    const EC& a = EC::Default();
    const EC& b = EC::Default();
    EXPECT_EQ(&a, &b);
}

// ===========================================================================
// IO::Stream — abstract base (concrete stub)
// ===========================================================================

namespace {
    class MinimalStream : public System::IO::Stream {
        std::vector<uint8_t> data_;
        int pos_ = 0;
    public:
        explicit MinimalStream(std::vector<uint8_t> d) : data_(std::move(d)) {}
        System::IO::intcs Read(System::IO::bytecs buf[], System::IO::intcs offset, System::IO::intcs count) override {
            int avail = static_cast<int>(data_.size()) - pos_;
            int n = std::min(count, avail);
            for (int i = 0; i < n; ++i) buf[offset + i] = data_[pos_++];
            return n;
        }
        void Close() override {}
        System::IO::intcs getLengthProperty() const override { return static_cast<int>(data_.size()); }
    };
}

TEST(StreamTests, CanRead_DefaultTrue) {
    MinimalStream s({1, 2, 3});
    EXPECT_TRUE(s.getCanReadProperty());
}

TEST(StreamTests, CanWrite_DefaultFalse) {
    MinimalStream s({});
    EXPECT_FALSE(s.getCanWriteProperty());
}

TEST(StreamTests, Read_ReturnsBytes) {
    MinimalStream s({10, 20, 30});
    uint8_t buf[3] = {};
    int n = s.Read(buf, 0, 3);
    EXPECT_EQ(n, 3);
    EXPECT_EQ(buf[0], 10u);
    EXPECT_EQ(buf[2], 30u);
}

TEST(StreamTests, Length) {
    MinimalStream s({1, 2, 3, 4});
    EXPECT_EQ(s.getLengthProperty(), 4);
}

TEST(StreamTests, Flush_NoThrow) {
    MinimalStream s({});
    EXPECT_NO_THROW(s.Flush());
}

// ===========================================================================
// IO::TextReader — default implementations
// ===========================================================================

TEST(TextReaderTests, Peek_Default_MinusOne) {
    System::IO::TextReader r;
    EXPECT_EQ(r.Peek(), -1);
}

TEST(TextReaderTests, Read_Default_MinusOne) {
    System::IO::TextReader r;
    EXPECT_EQ(r.Read(), -1);
}

TEST(TextReaderTests, ReadLine_Default_Empty) {
    System::IO::TextReader r;
    EXPECT_EQ(r.ReadLine(), "");
}

TEST(TextReaderTests, ReadToEnd_Default_Empty) {
    System::IO::TextReader r;
    EXPECT_EQ(r.ReadToEnd(), "");
}

TEST(TextReaderTests, Close_NoThrow) {
    System::IO::TextReader r;
    EXPECT_NO_THROW(r.Close());
}

// ===========================================================================
// IO::TextWriter — concrete stub
// ===========================================================================

namespace {
    class StringTextWriter : public System::IO::TextWriter {
    public:
        std::string output;
        using System::IO::TextWriter::Write;
        void Write(const std::string& v) override { output += v; }
    };
}

TEST(TextWriterTests, WriteString) {
    StringTextWriter w;
    w.Write(std::string("hello"));
    EXPECT_EQ(w.output, "hello");
}

TEST(TextWriterTests, WriteInt) {
    StringTextWriter w;
    w.Write(static_cast<SharpRuntime::intcs>(42));
    EXPECT_EQ(w.output, "42");
}

TEST(TextWriterTests, WriteBool_True) {
    StringTextWriter w;
    w.Write(true);
    EXPECT_EQ(w.output, "True");
}

TEST(TextWriterTests, WriteBool_False) {
    StringTextWriter w;
    w.Write(false);
    EXPECT_EQ(w.output, "False");
}

TEST(TextWriterTests, WriteChar) {
    StringTextWriter w;
    w.Write('X');
    EXPECT_EQ(w.output, "X");
}

TEST(TextWriterTests, WriteLine_AppendsNewline) {
    StringTextWriter w;
    w.WriteLine(std::string("hi"));
    EXPECT_NE(w.output.find("hi"), std::string::npos);
    EXPECT_GT(w.output.size(), 2u);
}

TEST(TextWriterTests, Flush_NoThrow) {
    StringTextWriter w;
    EXPECT_NO_THROW(w.Flush());
}

// ===========================================================================
// IO::Hashing::NonCryptographicHashAlgorithm (via Crc32 / XxHash32 / XxHash64)
// ===========================================================================

TEST(NonCryptographicHashTests, Crc32_InheritsBase) {
    System::IO::Hashing::Crc32 c;
    EXPECT_EQ(c.getHashLengthInBytesProperty(), 4);
}

TEST(NonCryptographicHashTests, XxHash32_InheritsBase) {
    System::IO::Hashing::XxHash32 h;
    EXPECT_EQ(h.getHashLengthInBytesProperty(), 4);
}

TEST(NonCryptographicHashTests, XxHash64_InheritsBase) {
    System::IO::Hashing::XxHash64 h;
    EXPECT_EQ(h.getHashLengthInBytesProperty(), 8);
}

TEST(NonCryptographicHashTests, Crc32_GetCurrentHash_CorrectLength) {
    System::IO::Hashing::Crc32 c;
    auto& base = static_cast<System::IO::Hashing::NonCryptographicHashAlgorithm&>(c);
    auto hash = base.GetCurrentHash();
    EXPECT_EQ(hash.size(), 4u);
}

TEST(NonCryptographicHashTests, Crc32_Append_Reset) {
    System::IO::Hashing::Crc32 c;
    auto& base = static_cast<System::IO::Hashing::NonCryptographicHashAlgorithm&>(c);
    std::vector<uint8_t> data = {1, 2, 3};
    c.Append(data);
    auto h1 = base.GetCurrentHash();
    c.Reset();
    auto h2 = base.GetCurrentHash();
    EXPECT_NE(h1, h2);
}

TEST(NonCryptographicHashTests, GetHashAndReset_ResetsAfter) {
    System::IO::Hashing::Crc32 c;
    auto& base = static_cast<System::IO::Hashing::NonCryptographicHashAlgorithm&>(c);
    std::vector<uint8_t> data = {0xAB, 0xCD};
    c.Append(data);
    auto h1 = base.GetHashAndReset();
    auto h2 = base.GetCurrentHash(); // after reset
    EXPECT_NE(h1, h2);
}

// ===========================================================================
// IO::IsolatedStorage — abstract base stub
// ===========================================================================

namespace {
    class ConcreteIsolatedStorage : public System::IO::IsolatedStorage::IsolatedStorage {
    public:
        void Remove() override {}
    };
}

TEST(IsolatedStorageTests, AvailableFreeSpace_Zero) {
    ConcreteIsolatedStorage s;
    EXPECT_EQ(s.getAvailableFreeSpaceProperty(), 0LL);
}

TEST(IsolatedStorageTests, Quota_Zero) {
    ConcreteIsolatedStorage s;
    EXPECT_EQ(s.getQuotaProperty(), 0LL);
}

TEST(IsolatedStorageTests, Scope_DefaultNone) {
    ConcreteIsolatedStorage s;
    EXPECT_EQ(s.getScopeProperty(), System::IO::IsolatedStorage::IsolatedStorageScope::None);
}

TEST(IsolatedStorageTests, Remove_NoThrow) {
    ConcreteIsolatedStorage s;
    EXPECT_NO_THROW(s.Remove());
}

// ===========================================================================
// Text::Json::JsonElement (direct tests)
// ===========================================================================

TEST(JsonElementTests, DefaultCtor_KindUndefined) {
    System::Text::Json::JsonElement e;
    EXPECT_EQ(e.getValueKindProperty(), System::Text::Json::JsonValueKind::Undefined);
}

TEST(JsonElementTests, StringElement_GetString) {
    System::Text::Json::JsonElement e(System::Text::Json::JsonValueKind::String, "hello");
    EXPECT_EQ(e.GetString(), "hello");
}

TEST(JsonElementTests, NumberElement_GetInt32) {
    System::Text::Json::JsonElement e(System::Text::Json::JsonValueKind::Number, "99");
    EXPECT_EQ(e.GetInt32(), 99);
}

TEST(JsonElementTests, NumberElement_GetInt64) {
    System::Text::Json::JsonElement e(System::Text::Json::JsonValueKind::Number, "9876543210");
    EXPECT_EQ(e.GetInt64(), 9876543210LL);
}

TEST(JsonElementTests, NumberElement_GetDouble) {
    System::Text::Json::JsonElement e(System::Text::Json::JsonValueKind::Number, "3.14");
    EXPECT_NEAR(e.GetDouble(), 3.14, 1e-9);
}

TEST(JsonElementTests, TrueElement_GetBoolean) {
    System::Text::Json::JsonElement t(System::Text::Json::JsonValueKind::True);
    EXPECT_TRUE(t.GetBoolean());
}

TEST(JsonElementTests, FalseElement_GetBoolean) {
    System::Text::Json::JsonElement f(System::Text::Json::JsonValueKind::False);
    EXPECT_FALSE(f.GetBoolean());
}

TEST(JsonElementTests, GetRawText) {
    System::Text::Json::JsonElement e(System::Text::Json::JsonValueKind::Number, "42");
    EXPECT_EQ(e.GetRawText(), "42");
}

TEST(JsonElementTests, TryGetProperty_Found) {
    System::Text::Json::JsonElement obj;
    auto child = std::make_shared<System::Text::Json::JsonElement>(
        System::Text::Json::JsonValueKind::String, "val");
    obj.addPropertyForTesting("key", child);
    System::Text::Json::JsonElement out;
    EXPECT_TRUE(obj.TryGetProperty("key", out));
    EXPECT_EQ(out.GetString(), "val");
}

TEST(JsonElementTests, TryGetProperty_NotFound) {
    System::Text::Json::JsonElement obj;
    System::Text::Json::JsonElement out;
    EXPECT_FALSE(obj.TryGetProperty("missing", out));
}

TEST(JsonElementTests, GetProperty_Throws_WhenMissing) {
    System::Text::Json::JsonElement obj;
    EXPECT_THROW((void)obj.GetProperty("x"), std::runtime_error);
}

TEST(JsonElementTests, EnumerateArray_Empty) {
    System::Text::Json::JsonElement arr(System::Text::Json::JsonValueKind::Array);
    EXPECT_TRUE(arr.EnumerateArray().empty());
}

TEST(JsonElementTests, EnumerateArray_OneItem) {
    System::Text::Json::JsonElement arr(System::Text::Json::JsonValueKind::Array);
    arr.addArrayItemForTesting(std::make_shared<System::Text::Json::JsonElement>(
        System::Text::Json::JsonValueKind::Number, "1"));
    EXPECT_EQ(arr.EnumerateArray().size(), 1u);
}

TEST(JsonElementTests, GetString_WrongKind_Throws) {
    System::Text::Json::JsonElement e(System::Text::Json::JsonValueKind::Number, "5");
    EXPECT_THROW((void)e.GetString(), std::runtime_error);
}

// ===========================================================================
// Text::EncodingProvider — abstract interface stub
// ===========================================================================

namespace {
    class StubEncodingProvider : public System::Text::EncodingProvider {
    public:
        std::shared_ptr<System::Text::Encoding> GetEncoding(int /*cp*/) override { return nullptr; }
        std::shared_ptr<System::Text::Encoding> GetEncoding(const std::string& /*name*/) override { return nullptr; }
    };
}

TEST(EncodingProviderTests, ConcreteSubclass_GetEncodingInt_ReturnsNull) {
    StubEncodingProvider p;
    EXPECT_EQ(p.GetEncoding(1252), nullptr);
}

TEST(EncodingProviderTests, ConcreteSubclass_GetEncodingName_ReturnsNull) {
    StubEncodingProvider p;
    EXPECT_EQ(p.GetEncoding("utf-8"), nullptr);
}

// ===========================================================================
// TimeZone — abstract base (concrete stub, no call to CurrentTimeZone)
// ===========================================================================

namespace {
    class UtcTimeZoneStub : public System::TimeZone {
        std::string stdName_ = "UTC";
        std::string dstName_ = "UTC";
    public:
        const std::string& getStandardNameProperty()  const override { return stdName_; }
        const std::string& getDaylightNameProperty()  const override { return dstName_; }
        System::TimeSpan GetUtcOffset(const System::DateTime&) const override {
            return System::TimeSpan(0);
        }
        bool IsDaylightSavingTime(const System::DateTime&) const override { return false; }
    };
}

TEST(TimeZoneTests, StandardName_Accessible) {
    UtcTimeZoneStub tz;
    EXPECT_EQ(tz.getStandardNameProperty(), "UTC");
}

TEST(TimeZoneTests, DaylightName_Accessible) {
    UtcTimeZoneStub tz;
    EXPECT_EQ(tz.getDaylightNameProperty(), "UTC");
}

TEST(TimeZoneTests, IsDaylightSavingTime_FalseForUTC) {
    UtcTimeZoneStub tz;
    System::DateTime dt;
    EXPECT_FALSE(tz.IsDaylightSavingTime(dt));
}

// ===========================================================================
// SharpRuntime::SharpRuntimeHelper — typedef sizes
// ===========================================================================

TEST(SharpRuntimeHelperTests, sbytecs_Is8BitSigned) {
    EXPECT_EQ(sizeof(SharpRuntime::sbytecs), 1u);
    EXPECT_EQ(SharpRuntime::SBYTECS_MAX, 127);
    EXPECT_EQ(SharpRuntime::SBYTECS_MIN, -128);
}

TEST(SharpRuntimeHelperTests, bytecs_Is8BitUnsigned) {
    EXPECT_EQ(sizeof(SharpRuntime::bytecs), 1u);
    EXPECT_EQ(SharpRuntime::BYTECS_MAX, 255u);
    EXPECT_EQ(SharpRuntime::BYTECS_MIN, 0u);
}

TEST(SharpRuntimeHelperTests, shortcs_Is16BitSigned) {
    EXPECT_EQ(sizeof(SharpRuntime::shortcs), 2u);
    EXPECT_EQ(SharpRuntime::SHORTCS_MAX, 32767);
    EXPECT_EQ(SharpRuntime::SHORTCS_MIN, -32768);
}

TEST(SharpRuntimeHelperTests, intcs_Is32BitSigned) {
    EXPECT_EQ(sizeof(SharpRuntime::intcs), 4u);
    EXPECT_EQ(SharpRuntime::INTCS_MAX, 2147483647);
    EXPECT_EQ(SharpRuntime::INTCS_MIN, -2147483648);
}

TEST(SharpRuntimeHelperTests, longcs_Is64BitSigned) {
    EXPECT_EQ(sizeof(SharpRuntime::longcs), 8u);
    EXPECT_EQ(SharpRuntime::LONGCS_MAX, INT64_MAX);
    EXPECT_EQ(SharpRuntime::LONGCS_MIN, INT64_MIN);
}

TEST(SharpRuntimeHelperTests, ulongcs_Is64BitUnsigned) {
    EXPECT_EQ(sizeof(SharpRuntime::ulongcs), 8u);
    EXPECT_EQ(SharpRuntime::ULONGCS_MAX, UINT64_MAX);
    EXPECT_EQ(SharpRuntime::ULONGCS_MIN, 0u);
}

TEST(SharpRuntimeHelperTests, charcs_Is16Bit) {
    EXPECT_EQ(sizeof(SharpRuntime::charcs), 2u);
}

TEST(SharpRuntimeHelperTests, SingleIsFloat) {
    EXPECT_EQ(sizeof(SharpRuntime::Single), sizeof(float));
}

TEST(SharpRuntimeHelperTests, DotNetNameAliases) {
    EXPECT_EQ(sizeof(SharpRuntime::Byte),   1u);
    EXPECT_EQ(sizeof(SharpRuntime::Int16),  2u);
    EXPECT_EQ(sizeof(SharpRuntime::Int32),  4u);
    EXPECT_EQ(sizeof(SharpRuntime::Int64),  8u);
    EXPECT_EQ(sizeof(SharpRuntime::UInt16), 2u);
    EXPECT_EQ(sizeof(SharpRuntime::UInt32), 4u);
    EXPECT_EQ(sizeof(SharpRuntime::UInt64), 8u);
}

// ===========================================================================
// Action / Func / Predicate — std::function typedefs
// ===========================================================================

TEST(ActionTests, Action_NoArgs) {
    int called = 0;
    System::Action a = [&]{ ++called; };
    a();
    EXPECT_EQ(called, 1);
}

TEST(ActionTests, ActionT_OneArg) {
    int captured = 0;
    System::ActionT<int> a = [&](int v){ captured = v; };
    a(99);
    EXPECT_EQ(captured, 99);
}

TEST(ActionTests, ActionT2_TwoArgs) {
    int sum = 0;
    System::ActionT2<int, int> a = [&](int x, int y){ sum = x + y; };
    a(3, 4);
    EXPECT_EQ(sum, 7);
}

TEST(ActionTests, ActionT3_ThreeArgs) {
    int sum = 0;
    System::ActionT3<int, int, int> a = [&](int x, int y, int z){ sum = x + y + z; };
    a(1, 2, 3);
    EXPECT_EQ(sum, 6);
}
TEST(ActionTests, ActionT4_FourArgs) {
    int sum = 0;
    System::ActionT4<int,int,int,int> a = [&](int a, int b, int c, int d){ sum = a+b+c+d; };
    a(1, 2, 3, 4);
    EXPECT_EQ(sum, 10);
}
TEST(ActionTests, ActionT8_EightArgs) {
    int sum = 0;
    System::ActionT8<int,int,int,int,int,int,int,int> a =
        [&](int a,int b,int c,int d,int e,int f,int g,int h){ sum=a+b+c+d+e+f+g+h; };
    a(1,1,1,1,1,1,1,1);
    EXPECT_EQ(sum, 8);
}
TEST(ActionTests, ActionT16_SixteenArgs) {
    int sum = 0;
    System::ActionT16<int,int,int,int,int,int,int,int,int,int,int,int,int,int,int,int> a =
        [&](int a,int b,int c,int d,int e,int f,int g,int h,
            int i,int j,int k,int l,int m,int n,int o,int p){
            sum=a+b+c+d+e+f+g+h+i+j+k+l+m+n+o+p; };
    a(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1);
    EXPECT_EQ(sum, 16);
}
TEST(ActionTests, Comparison_LessThan) {
    System::Comparison<int> cmp = [](int x, int y){ return x - y; };
    EXPECT_LT(cmp(1, 2), 0);
    EXPECT_EQ(cmp(3, 3), 0);
    EXPECT_GT(cmp(5, 4), 0);
}
TEST(ActionTests, Converter_IntToString) {
    System::Converter<int, std::string> conv = [](int v){ return std::to_string(v); };
    EXPECT_EQ(conv(42), "42");
}
TEST(ActionTests, SpanAction_Buffers) {
    int sum = 0;
    System::Buffers::SpanAction<int, int> a =
        [&](System::Span<int> sp, int extra){ for (int i = 0; i < sp.getLengthProperty(); ++i) sum += sp[i]; sum += extra; };
    std::vector<int> v = {1, 2, 3};
    System::Span<int> sp(v.data(), static_cast<int>(v.size()));
    a(sp, 10);
    EXPECT_EQ(sum, 16);
}
TEST(ActionTests, ReadOnlySpanAction_Buffers) {
    int count = 0;
    System::Buffers::ReadOnlySpanAction<int, std::string> a =
        [&](System::ReadOnlySpan<int> sp, std::string){ count = sp.getLengthProperty(); };
    std::vector<int> v = {5, 6, 7, 8};
    System::ReadOnlySpan<int> sp(v.data(), static_cast<int>(v.size()));
    a(sp, "ignored");
    EXPECT_EQ(count, 4);
}

TEST(FuncTests, Func_NoArgs) {
    System::Func<int> f = []{ return 42; };
    EXPECT_EQ(f(), 42);
}

TEST(FuncTests, FuncT_OneArg) {
    System::FuncT<std::string, int> f = [](std::string s){ return static_cast<int>(s.size()); };
    EXPECT_EQ(f("hello"), 5);
}

TEST(FuncTests, FuncT2_TwoArgs) {
    System::FuncT2<int, int, int> f = [](int a, int b){ return a + b; };
    EXPECT_EQ(f(3, 4), 7);
}

TEST(PredicateTests, Predicate_IsEven) {
    System::Predicate<int> p = [](int x){ return x % 2 == 0; };
    EXPECT_TRUE(p(4));
    EXPECT_FALSE(p(7));
}

// ===========================================================================
// MarshalByRefObject
// ===========================================================================

TEST(MarshalByRefObjectTests, Instantiation_NoThrow) {
    EXPECT_NO_THROW(System::MarshalByRefObject obj);
}

// ===========================================================================
// Threading::ThreadStart / ParameterizedThreadStart
// ===========================================================================

TEST(ThreadStartTests, ThreadStart_IsCallable) {
    int called = 0;
    System::Threading::ThreadStart ts = [&]{ ++called; };
    ts();
    EXPECT_EQ(called, 1);
}

TEST(ThreadStartTests, ParameterizedThreadStart_IsCallable) {
    void* captured = nullptr;
    System::Threading::ParameterizedThreadStart pts = [&](void* p){ captured = p; };
    int x = 0;
    pts(&x);
    EXPECT_EQ(captured, &x);
}

// ===========================================================================
// ApplicationId
// ===========================================================================

TEST(ApplicationIdTests, Properties_Accessible) {
    System::Version ver(1, 2, 3, 4);
    System::ApplicationId id("pubkey", "MyApp", ver, "x86", "neutral");
    EXPECT_EQ(id.getNameProperty(), "MyApp");
    EXPECT_EQ(id.getCultureProperty(), "neutral");
    EXPECT_EQ(id.getProcessorArchitectureProperty(), "x86");
    EXPECT_EQ(id.getPublicKeyTokenProperty(), "pubkey");
    EXPECT_EQ(id.getVersionProperty(), ver);
}

TEST(ApplicationIdTests, ToString_ContainsName) {
    System::Version ver(2, 0, 0, 0);
    System::ApplicationId id("tok", "TestApp", ver, "any", "en");
    EXPECT_NE(id.ToString().find("TestApp"), std::string::npos);
}

// ===========================================================================
// Numerics::GenericMathInterfaces
// ===========================================================================

TEST(GenericMathInterfacesTests, INumberBase_Radix) {
    EXPECT_EQ(System::Numerics::INumberBase<int>::Radix, 2);
}

TEST(GenericMathInterfacesTests, TemplateHierarchy_Instantiates) {
    struct MyNum : System::Numerics::ISignedNumber<MyNum> {};
    struct MyUNum : System::Numerics::IUnsignedNumber<MyUNum> {};
    struct MyFloat : System::Numerics::IFloatingPointIeee754<MyFloat> {};
    MyNum n; MyUNum u; MyFloat f;
    (void)n; (void)u; (void)f;
    SUCCEED();
}

TEST(GenericMathInterfacesTests, OperatorInterfaces_Instantiate) {
    System::Numerics::IAdditionOperators<int, int, int> a;
    System::Numerics::ISubtractionOperators<int, int, int> s;
    System::Numerics::IMultiplyOperators<int, int, int> m;
    (void)a; (void)s; (void)m;
    SUCCEED();
}

// ===========================================================================
// Collections::ObjectModel::KeyedCollection<TKey, TItem>
// ===========================================================================

namespace {
    struct NamedItem {
        std::string name;
        int value;
        bool operator==(const NamedItem& o) const { return name == o.name && value == o.value; }
    };

    class NamedCollection : public System::Collections::ObjectModel::KeyedCollection<std::string, NamedItem> {
    protected:
        std::string GetKeyForItem(const NamedItem& item) const override { return item.name; }
    };
}

TEST(KeyedCollectionTests, Add_And_Contains_Key) {
    NamedCollection nc;
    nc.Add({"alpha", 1});
    nc.Add({"beta",  2});
    EXPECT_TRUE(nc.Contains("alpha"));
    EXPECT_TRUE(nc.Contains("beta"));
    EXPECT_FALSE(nc.Contains("gamma"));
}

TEST(KeyedCollectionTests, IndexByKey_ReturnsItem) {
    NamedCollection nc;
    nc.Add({"x", 10});
    EXPECT_EQ(nc["x"].value, 10);
}

TEST(KeyedCollectionTests, Remove_ByKey) {
    NamedCollection nc;
    nc.Add({"a", 1});
    nc.Add({"b", 2});
    EXPECT_TRUE(nc.Remove("a"));
    EXPECT_FALSE(nc.Contains("a"));
    EXPECT_TRUE(nc.Contains("b"));
}

TEST(KeyedCollectionTests, Remove_MissingKey_ReturnsFalse) {
    NamedCollection nc;
    EXPECT_FALSE(nc.Remove("nothing"));
}

TEST(KeyedCollectionTests, Count_AfterAddRemove) {
    NamedCollection nc;
    nc.Add({"p", 1});
    nc.Add({"q", 2});
    nc.Add({"r", 3});
    EXPECT_EQ(nc.getCountProperty(), 3);
    nc.Remove("q");
    EXPECT_EQ(nc.getCountProperty(), 2);
}

TEST(KeyedCollectionTests, Add_DuplicateKey_Throws) {
    NamedCollection nc;
    nc.Add({"a", 1});
    EXPECT_THROW(nc.Add({"a", 2}), System::ArgumentException);
    EXPECT_EQ(nc.getCountProperty(), 1);
}

TEST(KeyedCollectionTests, Insert_DuplicateKey_Throws) {
    NamedCollection nc;
    nc.Add({"a", 1});
    nc.Add({"b", 2});
    EXPECT_THROW(nc.Insert(0, {"b", 3}), System::ArgumentException);
    EXPECT_EQ(nc.getCountProperty(), 2);
}

TEST(KeyedCollectionTests, IndexByKey_Missing_ThrowsKeyNotFoundException) {
    NamedCollection nc;
    nc.Add({"a", 1});
    EXPECT_THROW(nc["missing"], System::Collections::Generic::KeyNotFoundException);
    const NamedCollection& constNc = nc;
    EXPECT_THROW(constNc["missing"], System::Collections::Generic::KeyNotFoundException);
}

namespace {
    // Re-exposes protected members for direct testing.
    class ExposedNamedCollection : public NamedCollection {
    public:
        using NamedCollection::SetItem;
        using NamedCollection::ChangeItemKey;
    };
}

TEST(KeyedCollectionTests, SetItem_UpdatesIndex) {
    ExposedNamedCollection nc;
    nc.Add({"a", 1});
    nc.Add({"b", 2});
    nc.SetItem(0, {"c", 99});
    EXPECT_FALSE(nc.Contains("a"));
    EXPECT_TRUE(nc.Contains("c"));
    EXPECT_EQ(nc["c"].value, 99);
}

TEST(KeyedCollectionTests, SetItem_DuplicateKey_Throws) {
    ExposedNamedCollection nc;
    nc.Add({"a", 1});
    nc.Add({"b", 2});
    EXPECT_THROW(nc.SetItem(0, {"b", 99}), System::ArgumentException);
    EXPECT_EQ(nc["a"].value, 1);
}

TEST(KeyedCollectionTests, SetItem_SameKey_UpdatesValueWithoutError) {
    ExposedNamedCollection nc;
    nc.Add({"a", 1});
    nc.SetItem(0, {"a", 42});
    EXPECT_EQ(nc["a"].value, 42);
}

TEST(KeyedCollectionTests, ChangeItemKey_UpdatesIndex) {
    ExposedNamedCollection nc;
    nc.Add({"a", 1});
    // Per .NET convention: hold a reference, notify with the item still under its old key,
    // then mutate the field the reference points at.
    NamedItem& item = nc["a"];
    nc.ChangeItemKey(item, "z");
    item.name = "z";
    EXPECT_FALSE(nc.Contains("a"));
    EXPECT_TRUE(nc.Contains("z"));
    EXPECT_EQ(nc["z"].value, 1);
}

TEST(KeyedCollectionTests, ChangeItemKey_MissingItem_Throws) {
    ExposedNamedCollection nc;
    EXPECT_THROW(nc.ChangeItemKey({"ghost", 1}, "z"), System::ArgumentException);
}

TEST(KeyedCollectionTests, ChangeItemKey_DuplicateNewKey_Throws) {
    ExposedNamedCollection nc;
    nc.Add({"a", 1});
    nc.Add({"b", 2});
    EXPECT_THROW(nc.ChangeItemKey(nc["a"], "b"), System::ArgumentException);
}
