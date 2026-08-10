// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Task 39 remaining: SynchronizationContext, PeriodicTimer, WaitHandle,
// ASCIIEncoding, UnicodeEncoding, UTF8Encoding, EncodingInfo,
// ReadOnlyObservableCollection, ReadOnlySet, CollectionExtensions,
// StoragePaths, Experimental::Property.
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <stdexcept>

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Threading/SynchronizationContext.hpp"
#include "System/Threading/PeriodicTimer.hpp"
#include "System/Threading/Timeout.hpp"
#include "System/Threading/WaitHandle.hpp"
#include "System/Text/ASCIIEncoding.hpp"
#include "System/Text/UnicodeEncoding.hpp"
#include "System/Text/UTF8Encoding.hpp"
#include "System/Text/EncodingInfo.hpp"
#include "System/Collections/ObjectModel/ReadOnlyObservableCollection.hpp"
#include "System/Collections/ObjectModel/ReadOnlySet.hpp"
#include "System/Collections/Generic/CollectionExtensions.hpp"
#include "SharpRuntime/Storage/StoragePaths.hpp"
#include "System/ArgumentNullException.hpp"
#include "SharpRuntime/Experimental/Property.hpp"
#include "SharpRuntime/Experimental/ReadonlyProperty.hpp"
#include "System/TimeSpan.hpp"

// ===========================================================================
// SynchronizationContext
// ===========================================================================

using System::Threading::SynchronizationContext;
using System::Threading::SendOrPostCallback;

TEST(SynchronizationContextTests, GetCurrent_ReturnsNullptr) {
    EXPECT_EQ(SynchronizationContext::getCurrentProperty(), nullptr);
}

TEST(SynchronizationContextTests, Post_InvokesCallbackAsynchronously) {
    // Regression: Post() previously ran the callback inline/synchronously, identically to
    // Send() -- defeating the entire purpose of the distinction any caller relies on. Verified
    // against SynchronizationContext.cs: the default Post() queues to the thread pool.
    std::promise<void> called;
    std::future<void> fut = called.get_future();
    SynchronizationContext ctx;
    std::thread::id callingThreadId = std::this_thread::get_id();
    std::atomic<bool> ranOnDifferentThread{false};
    ctx.Post([&](void*) {
        ranOnDifferentThread = (std::this_thread::get_id() != callingThreadId);
        called.set_value();
    }, nullptr);
    ASSERT_EQ(fut.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    EXPECT_TRUE(ranOnDifferentThread.load());
}

TEST(SynchronizationContextTests, Send_InvokesCallbackSynchronously) {
    int value = 0;
    SynchronizationContext ctx;
    ctx.Send([&value](void* s) { value = *static_cast<int*>(s); }, &value);
    // callback receives &value; value stays 0 but the lambda ran
    (void)value;
}

TEST(SynchronizationContextTests, Post_NullCallback_NoThrow) {
    SynchronizationContext ctx;
    EXPECT_NO_THROW(ctx.Post(nullptr, nullptr));
}

TEST(SynchronizationContextTests, SetSynchronizationContext_NoThrow) {
    EXPECT_NO_THROW(SynchronizationContext::SetSynchronizationContext(nullptr));
}

TEST(SynchronizationContextTests, SetSynchronizationContext_RoundTrips) {
    // Regression: SetSynchronizationContext()/getCurrentProperty() previously didn't round-trip at
    // all -- getCurrentProperty() always returned nullptr regardless of what had been set.
    SynchronizationContext ctx;
    SynchronizationContext::SetSynchronizationContext(&ctx);
    EXPECT_EQ(SynchronizationContext::getCurrentProperty(), &ctx);
    SynchronizationContext::SetSynchronizationContext(nullptr); // reset for other tests on this thread
    EXPECT_EQ(SynchronizationContext::getCurrentProperty(), nullptr);
}

// ===========================================================================
// PeriodicTimer
// ===========================================================================

using System::Threading::PeriodicTimer;
using System::TimeSpan;

TEST(PeriodicTimerTests, Dispose_MakesWaitForNextTickReturnFalse) {
    PeriodicTimer timer(TimeSpan::FromMilliseconds(10000)); // long period
    timer.Dispose();
    EXPECT_FALSE(timer.WaitForNextTick());
}

TEST(PeriodicTimerTests, WaitForNextTick_ShortPeriod_ReturnsTrue) {
    PeriodicTimer timer(TimeSpan::FromMilliseconds(1));
    EXPECT_TRUE(timer.WaitForNextTick());
    timer.Dispose();
}

// Regression tests for a wave-3 audit finding: the constructor performed no validation of
// period at all -- TimeSpan::Zero or a negative (non-Infinite) period silently constructed a
// timer whose next_ was already <= now(), so WaitForNextTick() would busy-loop at 100% CPU
// instead of throwing. Verified against PeriodicTimer.cs's TryGetMilliseconds, which requires
// the period to be Timeout.InfiniteTimeSpan or in [1ms, uint.MaxValue - 1].
TEST(PeriodicTimerTests, Constructor_ZeroPeriod_ThrowsArgumentOutOfRangeException) {
    EXPECT_THROW(PeriodicTimer timer(TimeSpan::FromMilliseconds(0)), System::ArgumentOutOfRangeException);
}

TEST(PeriodicTimerTests, Constructor_NegativePeriod_ThrowsArgumentOutOfRangeException) {
    EXPECT_THROW(PeriodicTimer timer(TimeSpan::FromMilliseconds(-5)), System::ArgumentOutOfRangeException);
}

TEST(PeriodicTimerTests, Constructor_InfiniteTimeSpan_DoesNotThrow) {
    EXPECT_NO_THROW(PeriodicTimer timer(TimeSpan(System::Threading::Timeout::InfiniteTimeSpan)));
}

// A PeriodicTimer constructed with Timeout.InfiniteTimeSpan never ticks on its own, but
// Dispose() must still unblock a pending WaitForNextTick() call -- verified against
// PeriodicTimer.cs's Dispose(), which calls State.Signal(stopping: true) unconditionally.
TEST(PeriodicTimerTests, InfinitePeriod_WaitForNextTick_UnblocksOnDispose) {
    PeriodicTimer timer(TimeSpan(System::Threading::Timeout::InfiniteTimeSpan));
    std::atomic<bool> result{true};
    std::atomic<bool> finished{false};
    std::thread waiter([&] {
        result = timer.WaitForNextTick();
        finished = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(finished.load());
    timer.Dispose();
    waiter.join();
    EXPECT_TRUE(finished.load());
    EXPECT_FALSE(result.load());
}

// ===========================================================================
// WaitHandle constants
// ===========================================================================

using System::Threading::WaitHandle;

TEST(WaitHandleTests, WaitTimeout_Is258) {
    EXPECT_EQ(WaitHandle::WaitTimeout, 258);
}

TEST(WaitHandleTests, InvalidHandle_IsMinusOne) {
    EXPECT_EQ(WaitHandle::InvalidHandle, -1);
}

// ===========================================================================
// ASCIIEncoding
// ===========================================================================

using System::Text::ASCIIEncoding;

TEST(ASCIIEncodingTests, EncodingName_IsUsAscii) {
    ASCIIEncoding enc;
    EXPECT_EQ(enc.getEncodingNameProperty(), "us-ascii");
}

TEST(ASCIIEncodingTests, GetBytes_HelloWorld) {
    ASCIIEncoding enc;
    auto bytes = enc.GetBytes("ABC");
    ASSERT_EQ(static_cast<int>(bytes.size()), 3);
    EXPECT_EQ(bytes[0], uint8_t('A'));
    EXPECT_EQ(bytes[1], uint8_t('B'));
    EXPECT_EQ(bytes[2], uint8_t('C'));
}

TEST(ASCIIEncodingTests, GetString_RoundTrip) {
    ASCIIEncoding enc;
    std::string original = "Hello";
    auto bytes = enc.GetBytes(original);
    std::string result = enc.GetString(bytes.data(), 0, static_cast<int>(bytes.size()));
    EXPECT_EQ(result, original);
}

TEST(ASCIIEncodingTests, GetBytes_Empty) {
    ASCIIEncoding enc;
    auto bytes = enc.GetBytes("");
    EXPECT_TRUE(bytes.empty());
}

// GetBytes previously iterated the UTF-8-encoded input byte-wise, so a single multi-byte
// non-ASCII character produced one '?' per UTF-8 byte instead of .NET's one per UTF-16 code
// unit (a BMP character is always exactly one UTF-16 code unit). Verified against
// ASCIIEncoding.cs, which operates on char[]/UTF-16 code units.
TEST(ASCIIEncodingTests, GetBytes_NonAsciiBmpChar_ProducesExactlyOneReplacementByte) {
    ASCIIEncoding enc;
    // "café" -- 'é' (U+00E9) is 2 UTF-8 bytes but a single BMP character.
    auto bytes = enc.GetBytes("caf\xC3\xA9");
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], uint8_t('c'));
    EXPECT_EQ(bytes[1], uint8_t('a'));
    EXPECT_EQ(bytes[2], uint8_t('f'));
    EXPECT_EQ(bytes[3], uint8_t('?'));
}

TEST(ASCIIEncodingTests, GetBytes_ThreeByteUtf8Char_ProducesExactlyOneReplacementByte) {
    ASCIIEncoding enc;
    // U+20AC EURO SIGN, 3 UTF-8 bytes, still a single BMP character.
    auto bytes = enc.GetBytes("\xE2\x82\xAC");
    ASSERT_EQ(bytes.size(), 1u);
    EXPECT_EQ(bytes[0], uint8_t('?'));
}

// A supplementary-plane character (>= U+10000) is two UTF-16 code units (a surrogate pair)
// in real .NET, so it maps to two '?' bytes, not one.
TEST(ASCIIEncodingTests, GetBytes_SupplementaryPlaneChar_ProducesTwoReplacementBytes) {
    ASCIIEncoding enc;
    // U+1F600 GRINNING FACE, 4 UTF-8 bytes, one supplementary-plane character.
    auto bytes = enc.GetBytes("\xF0\x9F\x98\x80");
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(bytes[0], uint8_t('?'));
    EXPECT_EQ(bytes[1], uint8_t('?'));
}

// ===========================================================================
// UnicodeEncoding
// ===========================================================================

using System::Text::UnicodeEncoding;

TEST(UnicodeEncodingTests, EncodingName_IsUtf16) {
    UnicodeEncoding enc;
    EXPECT_EQ(enc.getEncodingNameProperty(), "utf-16");
}

TEST(UnicodeEncodingTests, GetBytes_TwoBytesPerChar) {
    UnicodeEncoding enc;
    auto bytes = enc.GetBytes("AB");
    EXPECT_EQ(static_cast<int>(bytes.size()), 4); // 2 chars * 2 bytes
}

TEST(UnicodeEncodingTests, GetString_RoundTrip_ASCII) {
    UnicodeEncoding enc;
    std::string original = "Hi";
    auto bytes = enc.GetBytes(original);
    std::string result = enc.GetString(bytes.data(), 0, static_cast<int>(bytes.size()));
    EXPECT_EQ(result, original);
}

TEST(UnicodeEncodingTests, GetBytes_Empty) {
    UnicodeEncoding enc;
    EXPECT_TRUE(enc.GetBytes("").empty());
}

// ===========================================================================
// UTF8Encoding
// ===========================================================================

using System::Text::UTF8Encoding;

TEST(UTF8EncodingTests, EncodingName_IsUtf8) {
    UTF8Encoding enc;
    EXPECT_EQ(enc.getEncodingNameProperty(), "utf-8");
}

TEST(UTF8EncodingTests, GetBytes_HelloWorld) {
    UTF8Encoding enc;
    auto bytes = enc.GetBytes("Hello");
    ASSERT_EQ(static_cast<int>(bytes.size()), 5);
    EXPECT_EQ(bytes[0], uint8_t('H'));
}

TEST(UTF8EncodingTests, GetString_RoundTrip) {
    UTF8Encoding enc;
    std::string original = "World";
    auto bytes = enc.GetBytes(original);
    std::string result = enc.GetString(bytes.data(), 0, static_cast<int>(bytes.size()));
    EXPECT_EQ(result, original);
}

TEST(UTF8EncodingTests, GetBytes_Empty) {
    UTF8Encoding enc;
    EXPECT_TRUE(enc.GetBytes("").empty());
}

// ===========================================================================
// EncodingInfo
// ===========================================================================

using System::Text::EncodingInfo;

TEST(EncodingInfoTests, CodePage_StoredCorrectly) {
    EncodingInfo info(65001, "utf-8", "Unicode (UTF-8)");
    EXPECT_EQ(info.getCodePageProperty(), 65001);
}

TEST(EncodingInfoTests, Name_StoredCorrectly) {
    EncodingInfo info(65001, "utf-8", "Unicode (UTF-8)");
    EXPECT_EQ(info.getNameProperty(), "utf-8");
}

TEST(EncodingInfoTests, DisplayName_StoredCorrectly) {
    EncodingInfo info(65001, "utf-8", "Unicode (UTF-8)");
    EXPECT_EQ(info.getDisplayNameProperty(), "Unicode (UTF-8)");
}

TEST(EncodingInfoTests, GetEncoding_ReturnsNonNull) {
    EncodingInfo info(65001, "utf-8", "Unicode (UTF-8)");
    EXPECT_NE(info.GetEncoding(), nullptr);
}

// ===========================================================================
// ReadOnlyObservableCollection
// ===========================================================================

using System::Collections::ObjectModel::ObservableCollection;
using System::Collections::ObjectModel::ReadOnlyObservableCollection;

TEST(ReadOnlyObservableCollectionTests, Count_MatchesSource) {
    auto oc = std::make_shared<ObservableCollection<int>>();
    oc->Add(1); oc->Add(2); oc->Add(3);
    ReadOnlyObservableCollection<int> roc(oc);
    EXPECT_EQ(roc.getCountProperty(), 3);
}

TEST(ReadOnlyObservableCollectionTests, IndexOperator_ReturnsElement) {
    auto oc = std::make_shared<ObservableCollection<int>>();
    oc->Add(10); oc->Add(20);
    ReadOnlyObservableCollection<int> roc(oc);
    EXPECT_EQ(roc[0], 10);
    EXPECT_EQ(roc[1], 20);
}

TEST(ReadOnlyObservableCollectionTests, Contains_Found) {
    auto oc = std::make_shared<ObservableCollection<int>>();
    oc->Add(5);
    ReadOnlyObservableCollection<int> roc(oc);
    EXPECT_TRUE(roc.Contains(5));
    EXPECT_FALSE(roc.Contains(99));
}

TEST(ReadOnlyObservableCollectionTests, IsEmpty_TrueWhenEmpty) {
    auto oc = std::make_shared<ObservableCollection<int>>();
    ReadOnlyObservableCollection<int> roc(oc);
    EXPECT_TRUE(roc.getIsEmptyProperty());
}

TEST(ReadOnlyObservableCollectionTests, RangeFor_IteratesAll) {
    auto oc = std::make_shared<ObservableCollection<int>>();
    oc->Add(1); oc->Add(2); oc->Add(3);
    ReadOnlyObservableCollection<int> roc(oc);
    int sum = 0;
    for (const auto& v : roc) sum += v;
    EXPECT_EQ(sum, 6);
}

// ===========================================================================
// ReadOnlySet
// ===========================================================================

using System::Collections::ObjectModel::ReadOnlySet;

TEST(ReadOnlySetTests, Count_MatchesUnderlying) {
    auto s = std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1, 2, 3});
    ReadOnlySet<int> ros(s);
    EXPECT_EQ(ros.getCountProperty(), 3);
}

TEST(ReadOnlySetTests, Contains_Present_True) {
    auto s = std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{42});
    ReadOnlySet<int> ros(s);
    EXPECT_TRUE(ros.Contains(42));
}

TEST(ReadOnlySetTests, Contains_Absent_False) {
    auto s = std::make_shared<std::unordered_set<int>>();
    ReadOnlySet<int> ros(s);
    EXPECT_FALSE(ros.Contains(7));
}

TEST(ReadOnlySetTests, RangeFor_IteratesAll) {
    auto s = std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1, 2, 3});
    ReadOnlySet<int> ros(s);
    int sum = 0;
    for (const auto& v : ros) sum += v;
    EXPECT_EQ(sum, 6);
}

// ===========================================================================
// CollectionExtensions
// ===========================================================================

using System::Collections::Generic::CollectionExtensions;

TEST(CollectionExtensionsTests, GetValueOrDefault_KeyPresent) {
    std::unordered_map<std::string, int> m{{"a", 1}};
    EXPECT_EQ(CollectionExtensions::GetValueOrDefault(m, std::string("a")), 1);
}

TEST(CollectionExtensionsTests, GetValueOrDefault_KeyAbsent_DefaultZero) {
    std::unordered_map<std::string, int> m;
    EXPECT_EQ(CollectionExtensions::GetValueOrDefault(m, std::string("x")), 0);
}

TEST(CollectionExtensionsTests, GetValueOrDefault_WithDefault_KeyAbsent) {
    std::unordered_map<std::string, int> m;
    EXPECT_EQ(CollectionExtensions::GetValueOrDefault(m, std::string("x"), 99), 99);
}

TEST(CollectionExtensionsTests, TryAdd_NewKey_ReturnsTrue) {
    std::unordered_map<std::string, int> m;
    EXPECT_TRUE(CollectionExtensions::TryAdd(m, std::string("k"), 5));
    EXPECT_EQ(m["k"], 5);
}

TEST(CollectionExtensionsTests, TryAdd_ExistingKey_ReturnsFalse) {
    std::unordered_map<std::string, int> m{{"k", 1}};
    EXPECT_FALSE(CollectionExtensions::TryAdd(m, std::string("k"), 2));
    EXPECT_EQ(m["k"], 1);
}

TEST(CollectionExtensionsTests, Remove_ExistingKey_TrueAndOutputValue) {
    std::unordered_map<std::string, int> m{{"k", 42}};
    int removed = 0;
    EXPECT_TRUE(CollectionExtensions::Remove(m, std::string("k"), removed));
    EXPECT_EQ(removed, 42);
    EXPECT_TRUE(m.empty());
}

TEST(CollectionExtensionsTests, Remove_MissingKey_False) {
    std::unordered_map<std::string, int> m;
    int removed = 0;
    EXPECT_FALSE(CollectionExtensions::Remove(m, std::string("missing"), removed));
}

TEST(CollectionExtensionsTests, AsReadOnly_ReturnsSameData) {
    std::vector<int> v = {1, 2, 3};
    const auto& ref = CollectionExtensions::AsReadOnly(v);
    EXPECT_EQ(ref.size(), size_t(3));
    EXPECT_EQ(ref[0], 1);
}

// ===========================================================================
// StoragePaths
// ===========================================================================

TEST(StoragePathsTests, GetIsolatedStorageRoot_NoThrow) {
    EXPECT_NO_THROW((void)SharpRuntime::Storage::StoragePaths::GetIsolatedStorageRoot());
}

TEST(StoragePathsTests, GetIsolatedStorageRoot_NonEmpty) {
    auto path = SharpRuntime::Storage::StoragePaths::GetIsolatedStorageRoot();
    EXPECT_FALSE(path.empty());
}

// ===========================================================================
// Experimental::Property
// ===========================================================================

using SharpRuntime::Experimental::Property;

TEST(ExperimentalPropertyTests, ReadWrite_GetReturnsSetValue) {
    int stored = 0;
    Property<int> p(
        [&stored]() { return stored; },
        [&stored](const int& v) { stored = v; }
    );
    p.set(42);
    EXPECT_EQ(p.get(), 42);
}

TEST(ExperimentalPropertyTests, ReadOnly_GetReturnsValue) {
    Property<int> p([](){ return 7; });
    EXPECT_EQ(p.get(), 7);
}

// Regression test for a wave-3 audit finding: set() threw std::logic_error (an unrelated
// std:: exception type invisible to code catching System::Exception&) instead of
// System::NotSupportedException -- this Property never has a real .NET counterpart to
// verify against (it's a SharpRuntime-internal experimental helper), but the project's
// overall policy is that every thrown exception is a System:: type, and NotSupportedException
// is the closest .NET-idiomatic match for "this operation is permanently unsupported on a
// read-only property" (as opposed to NotImplementedException's "not yet implemented").
TEST(ExperimentalPropertyTests, ReadOnly_Set_ThrowsNotSupportedException) {
    Property<int> p([](){ return 0; });
    EXPECT_THROW(p.set(1), System::NotSupportedException);
}

TEST(ExperimentalPropertyTests, ImplicitConversion_WorksAsGetter) {
    Property<std::string> p([](){ return std::string("hello"); });
    std::string v = p;
    EXPECT_EQ(v, "hello");
}

// ---------------------------------------------------------------------------
// SR-AUD-179 / ticket #2244 -- the assignment expression's own result.
//
// operator= used to return a private `cachedValue` member by T&.  That member
// was never synchronised with the getter or the setter, so the expression
// `p = v` evaluated to an unrelated object: an empty string for
// Property<std::string> (the finding's own reproduction, measured verbatim as
// `stored=new assignment_result=`), and an INDETERMINATE value for a scalar T,
// because `T cachedValue;` is default-initialised -- two runs of
// build-probe/2243_probe1_before printed 535613888 and -455742320 for the same
// case.  operator= now returns T by value, read back through the getter, as
// std::atomic's assignment operator does.
// ---------------------------------------------------------------------------

TEST(ExperimentalPropertyTests, Assignment_ExpressionYieldsTheValueTheGetterReadsBack) {
    std::string stored = "old";
    Property<std::string> p(
        [&stored]() { return stored; },
        [&stored](const std::string& v) { stored = v; }
    );
    const std::string result = (p = std::string("new"));
    EXPECT_EQ(stored, "new");
    EXPECT_EQ(result, "new");
    EXPECT_EQ(p.get(), "new");
}

TEST(ExperimentalPropertyTests, Assignment_ExpressionYieldsTheValueForAScalarType) {
    int stored = 0;
    Property<int> p(
        [&stored]() { return stored; },
        [&stored](const int& v) { stored = v; }
    );
    const int result = (p = 42);
    EXPECT_EQ(stored, 42);
    EXPECT_EQ(result, 42);
}

// A setter is free to transform what it is given.  The assignment expression
// must report what the property now holds, not what the caller offered -- which
// is why the repair reads back through the getter instead of returning `value`.
TEST(ExperimentalPropertyTests, Assignment_TransformingSetter_ExpressionReportsTheStoredValue) {
    int stored = 0;
    Property<int> p(
        [&stored]() { return stored; },
        [&stored](const int& v) { stored = v < 0 ? 0 : v; }
    );
    const int result = (p = -5);
    EXPECT_EQ(stored, 0);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(p.get(), 0);
}

// Chained assignment used to propagate the indeterminate cache into the
// left-hand property; it now propagates the value.
TEST(ExperimentalPropertyTests, Assignment_Chained_SetsBothProperties) {
    int a = -1;
    int b = -1;
    Property<int> pa([&a]() { return a; }, [&a](const int& v) { a = v; });
    Property<int> pb([&b]() { return b; }, [&b](const int& v) { b = v; });
    pa = pb = 9;
    EXPECT_EQ(b, 9);
    EXPECT_EQ(a, 9);
}

TEST(ExperimentalPropertyTests, Assignment_InvokesTheSetterThenTheGetterExactlyOnce) {
    int stored = 0;
    int getterCalls = 0;
    int setterCalls = 0;
    Property<int> p(
        [&]() { ++getterCalls; return stored; },
        [&](const int& v) { ++setterCalls; stored = v; }
    );
    getterCalls = 0;
    const int result = (p = 5);
    EXPECT_EQ(result, 5);
    EXPECT_EQ(setterCalls, 1);
    EXPECT_EQ(getterCalls, 1);
}

// The read-only route is unchanged: the throw happens before the read-back, so
// a failed assignment never reaches the getter.
TEST(ExperimentalPropertyTests, Assignment_ReadOnly_ThrowsBeforeInvokingTheGetter) {
    int getterCalls = 0;
    Property<int> p([&]() { ++getterCalls; return 3; });
    EXPECT_THROW((void)(p = 1), System::NotSupportedException);
    EXPECT_EQ(getterCalls, 0);
}

// ReadOnlyProperty deletes its own operator=, which hides the base's by name;
// reaching the base through a reference still throws rather than mutating.
TEST(ExperimentalPropertyTests, Assignment_ReadOnlyPropertyThroughBaseReference_Throws) {
    SharpRuntime::Experimental::ReadOnlyProperty<std::string> ro(
        []() { return std::string("read-only"); });
    SharpRuntime::Experimental::Property<std::string>& base = ro;
    EXPECT_THROW((void)(base = std::string("attempt")), System::NotSupportedException);
    EXPECT_EQ(ro.get(), "read-only");
}

// Layout pin.  #2244 deliberately retained the now-vestigial cachedValue member
// so that sizeof(Property<T>) did not move; ticket #2246 carries the approval
// request to remove it.  If that approval lands, this test is the one that must
// be updated in the same change.
// The concrete sizes are libstdc++-specific (the trailing member is padded to
// the callables' alignment), so the pin is the invariant rather than a number:
// the object is strictly larger than its two callables, i.e. the cache is still
// in the layout.
TEST(ExperimentalPropertyTests, Layout_CachedValueIsStillPartOfTheObject) {
    EXPECT_GT(sizeof(Property<int>), 2 * sizeof(std::function<int()>));
    EXPECT_GE(sizeof(Property<std::string>),
              2 * sizeof(std::function<std::string()>) + sizeof(std::string));
    EXPECT_EQ(sizeof(SharpRuntime::Experimental::ReadOnlyProperty<int>), sizeof(Property<int>));
}

// ---------------------------------------------------------------------------
// SR-AUD-181 / ticket #2245 -- the advertised macro family.
//
// DEF_PROP_AUTO and DEF_PROP_CUSTOM expanded to `SharpRuntime::Property<type>`
// while the class is `SharpRuntime::Experimental::Property<T>`, so a minimal
// class using the documented macros failed to compile at the type name.  This
// fixture is that minimal class: it exists so the macros are compiled by the
// test build rather than only by documentation.
// ---------------------------------------------------------------------------

namespace {

class MacroWidget {
public:
    MacroWidget()
        : IMPL_PROP_AUTO(int, Value)
        , IMPL_PROP_AUTO_READONLY(std::string, Label)
        , IMPL_PROP_CUSTOM(int, Doubled, { return raw * 2; }, { raw = v / 2; })
        , IMPL_PROP_CUSTOM_READONLY(int, Negated, { return -raw; })
    {}

    DEF_PROP_AUTO(int, Value, 0)
    DEF_PROP_AUTO(std::string, Label, "label")
    DEF_PROP_CUSTOM(int, Doubled)
    DEF_PROP_CUSTOM(int, Negated)

private:
    int raw = 21;
};

}  // namespace

TEST(ExperimentalPropertyMacroTests, AutoPair_ReadsAndWritesTheGeneratedBackingField) {
    MacroWidget w;
    EXPECT_EQ(w.Value.get(), 0);
    w.Value.set(7);
    EXPECT_EQ(w.Value.get(), 7);
    EXPECT_EQ(static_cast<int>(w.Value = 8), 8);
    EXPECT_EQ(w.Value.get(), 8);
}

TEST(ExperimentalPropertyMacroTests, AutoReadonlyPair_ReadsTheInitialiserAndRejectsWrites) {
    MacroWidget w;
    EXPECT_EQ(w.Label.get(), "label");
    EXPECT_THROW(w.Label.set("nope"), System::NotSupportedException);
    EXPECT_EQ(w.Label.get(), "label");
}

TEST(ExperimentalPropertyMacroTests, CustomPair_UsesTheSuppliedBodies) {
    MacroWidget w;
    EXPECT_EQ(w.Doubled.get(), 42);
    EXPECT_EQ(w.Negated.get(), -21);
    w.Doubled.set(100);
    EXPECT_EQ(w.Doubled.get(), 100);
    EXPECT_EQ(w.Negated.get(), -50);
}

TEST(ExperimentalPropertyMacroTests, CustomReadonlyPair_RejectsWrites) {
    MacroWidget w;
    EXPECT_EQ(w.Negated.get(), -21);
    EXPECT_THROW(w.Negated.set(1), System::NotSupportedException);
    EXPECT_EQ(w.Negated.get(), -21);
}

// -----------------------------------------------------------------------
// Ticket #2247: the constructor decides an empty getter at the public boundary.
//
// Before this ticket, Property<T>(std::function<T()>{}) constructed happily and the failure
// surfaced at the FIRST READ as std::bad_function_call -- a native exception invisible to code
// catching System::Exception&, raised arbitrarily far from the construction that caused it.
// That is the shape CCF-011 named, and CCF-011's policy applies unchanged: decide emptiness at
// the public boundary, before anything is done with the input, and report an argument to an
// ordinary method as System::ArgumentNullException. It is an ADJACENCY to that family, not a
// member: CCF-011 is closed with six named members, none of them this header, and no frozen
// finding names this door. Audit numbering stays frozen, so this carries no SR-AUD-* identifier.
//
// PREMISE CORRECTION to #2247's own acceptance criterion: it says "ReadOnlyProperty is
// unaffected". ReadOnlyProperty DOES exist (SharpRuntime/Experimental/ReadonlyProperty.hpp,
// spelled with a lowercase 'o' in the filename), and it is NOT unaffected -- it forwards its
// getter straight to this constructor, so it inherits the rejection. That is correct and is
// pinned below. What is genuinely unaffected is the read-only SPELLING: an empty SETTER.
// -----------------------------------------------------------------------

TEST(ExperimentalPropertyTests, Construction_EmptyGetter_ThrowsArgumentNullException) {
    EXPECT_THROW(Property<int>(std::function<int()>{}), System::ArgumentNullException);
}

TEST(ExperimentalPropertyTests, Construction_EmptyGetter_NamesTheParameter) {
    try {
        Property<int> p(std::function<int()>{});
        FAIL() << "expected System::ArgumentNullException";
    } catch (const System::ArgumentNullException& ex) {
        EXPECT_EQ(ex.getParamNameProperty(), "customGetter");
    }
}

TEST(ExperimentalPropertyTests, Construction_EmptyGetter_IsCatchableAsSystemException) {
    // The whole point: std::bad_function_call is not, and that was the defect.
    bool caught = false;
    try {
        Property<std::string> p(std::function<std::string()>{});
    } catch (const System::Exception&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
}

TEST(ExperimentalPropertyTests, Construction_EmptyGetter_ThrowsEvenWithAUsableSetter) {
    int backing = 0;
    EXPECT_THROW(
        Property<int>(std::function<int()>{}, [&backing](const int& v) { backing = v; }),
        System::ArgumentNullException);
    EXPECT_EQ(backing, 0);
}

TEST(ExperimentalPropertyTests, Construction_EmptyGetter_ThrowsBeforeAnyRead) {
    // "Before anything is done with the input" is the operative half of the policy: no read
    // path may run first, or the native exception would still escape.
    bool setterRan = false;
    try {
        Property<int> p(std::function<int()>{},
                        [&setterRan](const int&) { setterRan = true; });
        FAIL() << "expected System::ArgumentNullException";
    } catch (const System::ArgumentNullException&) {
        EXPECT_FALSE(setterRan);
    }
}

TEST(ExperimentalPropertyTests, Construction_EmptySetter_StillConstructsAndIsReadOnly) {
    // The deliberate read-only spelling is untouched: an empty SETTER is legal, reads work, and
    // a write is still System::NotSupportedException rather than an argument error.
    int backing = 5;
    Property<int> p([&backing]() { return backing; }, nullptr);
    EXPECT_EQ(p.get(), 5);
    EXPECT_THROW(p.set(9), System::NotSupportedException);
    EXPECT_EQ(backing, 5);
}

TEST(ExperimentalPropertyTests, Construction_BothPresent_IsUnaffected) {
    int backing = 1;
    Property<int> p([&backing]() { return backing; },
                    [&backing](const int& v) { backing = v; });
    EXPECT_EQ(p.get(), 1);
    p = 7;
    EXPECT_EQ(backing, 7);
    EXPECT_EQ(static_cast<int>(p), 7);
}

TEST(ExperimentalPropertyTests, Construction_ReadOnlyProperty_InheritsTheRejection) {
    // ReadOnlyProperty forwards its getter to the base constructor, so it is NOT unaffected --
    // see the premise correction above.
    EXPECT_THROW(SharpRuntime::Experimental::ReadOnlyProperty<int>(std::function<int()>{}),
                 System::ArgumentNullException);
}

TEST(ExperimentalPropertyTests, Construction_ReadOnlyProperty_WithAGetterStillWorks) {
    SharpRuntime::Experimental::ReadOnlyProperty<int> ro([]() { return 42; });
    EXPECT_EQ(ro.get(), 42);
}
