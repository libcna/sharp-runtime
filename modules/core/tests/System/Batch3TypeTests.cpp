// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
// Batch 3 non-exception type tests:
//   MidpointRounding, UInt128, MarshalByRefObject, EventHandler,
//   ReadOnlyMemory, Activator, ThreadStaticAttribute, FlagsAttribute
#include <gtest/gtest.h>
#include <limits>
#include <type_traits>
#include <vector>
#include "System/MidpointRounding.hpp"
#include "System/UInt128.hpp"
#include "System/MarshalByRefObject.hpp"
#include "System/EventHandler.hpp"
#include "System/EventArgs.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ArraySegment.hpp"
#include "System/ReadOnlyMemory.hpp"
#include "System/Activator.hpp"
#include "System/ThreadStaticAttribute.hpp"
#include "System/FlagsAttribute.hpp"
#include "System/Attribute.hpp"

// ---------------------------------------------------------------------------
// MidpointRounding (extra tests; basic ToEven/AwayFromZero/ToPositiveInfinity
//   are in SystemTypesRemainingTests.cpp)
// ---------------------------------------------------------------------------
TEST(MidpointRoundingNewTests, ToZero_IsTwo) {
    EXPECT_EQ(static_cast<int>(System::MidpointRounding::ToZero), 2);
}
TEST(MidpointRoundingNewTests, ToNegativeInfinity_IsThree) {
    EXPECT_EQ(static_cast<int>(System::MidpointRounding::ToNegativeInfinity), 3);
}
TEST(MidpointRoundingNewTests, ToEven_NeToZero) {
    EXPECT_NE(System::MidpointRounding::ToEven, System::MidpointRounding::ToZero);
}

// ---------------------------------------------------------------------------
// UInt128 (extra tests; basic arithmetic/ToString/comparison in Task40Tests.cpp)
// ---------------------------------------------------------------------------
TEST(UInt128NewTests, BitwiseAnd) {
    System::UInt128 a(0xFF), b(0x0F);
    EXPECT_EQ(static_cast<unsigned long long>(a & b), 0x0FULL);
}
TEST(UInt128NewTests, BitwiseOr) {
    System::UInt128 a(0xF0), b(0x0F);
    EXPECT_EQ(static_cast<unsigned long long>(a | b), 0xFFULL);
}
TEST(UInt128NewTests, BitwiseXor) {
    System::UInt128 a(0xFF), b(0x0F);
    EXPECT_EQ(static_cast<unsigned long long>(a ^ b), 0xF0ULL);
}
TEST(UInt128NewTests, ShiftLeft) {
    System::UInt128 a(1);
    EXPECT_EQ(static_cast<unsigned long long>(a << 4), 16ULL);
}
TEST(UInt128NewTests, ShiftRight) {
    System::UInt128 a(256);
    EXPECT_EQ(static_cast<unsigned long long>(a >> 4), 16ULL);
}
TEST(UInt128NewTests, GetUpperLower_FromParts) {
    System::UInt128 v(0xABCDEF01ULL, 0x12345678ULL);
    EXPECT_EQ(v.getUpperProperty(), 0xABCDEF01ULL);
    EXPECT_EQ(v.getLowerProperty(), 0x12345678ULL);
}
TEST(UInt128NewTests, MinValue_IsZero) {
    EXPECT_EQ(static_cast<unsigned long long>(System::UInt128::MinValue()), 0ULL);
}
TEST(UInt128NewTests, MaxValue_UpperIsMax) {
    EXPECT_EQ(System::UInt128::MaxValue().getUpperProperty(), 0xFFFFFFFFFFFFFFFFULL);
}
TEST(UInt128NewTests, Modulo) {
    System::UInt128 a(10), b(3);
    EXPECT_EQ(static_cast<unsigned long long>(a % b), 1ULL);
}
TEST(UInt128NewTests, Subtraction) {
    System::UInt128 a(10), b(3);
    EXPECT_EQ(static_cast<unsigned long long>(a - b), 7ULL);
}

// ---------------------------------------------------------------------------
// MarshalByRefObject (extra; Task42Tests has Instantiation_NoThrow)
// ---------------------------------------------------------------------------
TEST(MarshalByRefObjectNewTests, PolymorphicDeletion_NoLeak) {
    struct Derived : System::MarshalByRefObject {};
    std::unique_ptr<System::MarshalByRefObject> p = std::make_unique<Derived>();
    EXPECT_NE(p.get(), nullptr);
}
TEST(MarshalByRefObjectNewTests, DefaultCtor_DoesNotThrow) {
    EXPECT_NO_THROW(System::MarshalByRefObject obj);
}

// ---------------------------------------------------------------------------
// EventHandler<TEventArgs>
// ---------------------------------------------------------------------------
TEST(EventHandlerTests, Add_And_Raise_CallsHandler) {
    System::EventHandler<System::EventArgs> eh;
    bool called = false;
    eh += [&](System::Object*, const System::EventArgs&) { called = true; };
    eh.Raise(nullptr, System::EventArgs{});
    EXPECT_TRUE(called);
}
TEST(EventHandlerTests, MultipleHandlers_AllCalled) {
    System::EventHandler<System::EventArgs> eh;
    int count = 0;
    eh += [&](System::Object*, const System::EventArgs&) { ++count; };
    eh += [&](System::Object*, const System::EventArgs&) { ++count; };
    eh.Raise(nullptr, System::EventArgs{});
    EXPECT_EQ(count, 2);
}
TEST(EventHandlerTests, Remove_HandlerNotCalledAfterRemove) {
    System::EventHandler<System::EventArgs> eh;
    bool called = false;
    auto token = eh.Add([&](System::Object*, const System::EventArgs&) { called = true; });
    eh.Remove(token);
    eh.Raise(nullptr, System::EventArgs{});
    EXPECT_FALSE(called);
}
TEST(EventHandlerTests, Clear_NoHandlersCalled) {
    System::EventHandler<System::EventArgs> eh;
    int count = 0;
    eh += [&](System::Object*, const System::EventArgs&) { ++count; };
    eh.Clear();
    eh.Raise(nullptr, System::EventArgs{});
    EXPECT_EQ(count, 0);
}
TEST(EventHandlerTests, Empty_TrueWhenNoHandlers) {
    System::EventHandler<System::EventArgs> eh;
    EXPECT_TRUE(eh.Empty());
}
TEST(EventHandlerTests, Size_ReflectsHandlerCount) {
    System::EventHandler<System::EventArgs> eh;
    eh += [](System::Object*, const System::EventArgs&) {};
    eh += [](System::Object*, const System::EventArgs&) {};
    EXPECT_EQ(eh.Size(), 2u);
}
TEST(EventHandlerTests, Invoke_SameAsRaise) {
    System::EventHandler<System::EventArgs> eh;
    int count = 0;
    eh += [&](System::Object*, const System::EventArgs&) { ++count; };
    eh.Invoke(nullptr, System::EventArgs{});
    EXPECT_EQ(count, 1);
}

// ---------------------------------------------------------------------------
// ReadOnlyMemory<T>
// ---------------------------------------------------------------------------
TEST(ReadOnlyMemoryTests, DefaultCtor_IsEmpty) {
    System::ReadOnlyMemory<int> m;
    EXPECT_TRUE(m.getIsEmptyProperty());
    EXPECT_EQ(m.getLengthProperty(), 0);
}
TEST(ReadOnlyMemoryTests, ConstructFromVector_LengthCorrect) {
    std::vector<int> v = {1, 2, 3};
    System::ReadOnlyMemory<int> m(v);
    EXPECT_EQ(m.getLengthProperty(), 3);
}
TEST(ReadOnlyMemoryTests, ElementAccess_CorrectValue) {
    std::vector<int> v = {10, 20, 30};
    System::ReadOnlyMemory<int> m(v);
    EXPECT_EQ(m[0], 10);
    EXPECT_EQ(m[2], 30);
}
TEST(ReadOnlyMemoryTests, OutOfRange_Throws) {
    std::vector<int> v = {1};
    System::ReadOnlyMemory<int> m(v);
    EXPECT_THROW(m[5], System::ArgumentOutOfRangeException);
}
TEST(ReadOnlyMemoryTests, Slice_CorrectLength) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    System::ReadOnlyMemory<int> m(v);
    auto s = m.Slice(1, 3);
    EXPECT_EQ(s.getLengthProperty(), 3);
    EXPECT_EQ(s[0], 2);
}
TEST(ReadOnlyMemoryTests, ToArray_CopiesData) {
    std::vector<int> v = {7, 8, 9};
    System::ReadOnlyMemory<int> m(v);
    auto arr = m.ToArray();
    EXPECT_EQ(arr.size(), 3u);
    EXPECT_EQ(arr[1], 8);
}
TEST(ReadOnlyMemoryTests, Empty_IsEmpty) {
    auto e = System::ReadOnlyMemory<int>::getEmptyProperty();
    EXPECT_TRUE(e.getIsEmptyProperty());
}

// ---------------------------------------------------------------------------
// SR-AUD-043b (#1854, approved 2026-07-31): the three ReadOnlyMemory
// constructors reject an invalid length instead of storing it.
//
// #1852 (043a) closed the reachable exploit by validating Span/ReadOnlySpan
// construction; these constructors were still directly reachable and stored a
// negative length verbatim, which any later size_t use would turn into an
// unbounded read. Rejecting it required dropping `noexcept` from all three and
// `constexpr` from the pointer/length one. An Itanium mangled name does not
// encode noexcept, so this moved no exported symbol; the fields stay signed
// intcs, so nothing moved in the layout either.
// ---------------------------------------------------------------------------

TEST(ReadOnlyMemoryTests, PtrLengthCtor_NegativeLength_ThrowsAOORE) {
    const int arr[] = {1, 2, 3};
    EXPECT_THROW(System::ReadOnlyMemory<int>(arr, -1),
                 System::ArgumentOutOfRangeException);
}

TEST(ReadOnlyMemoryTests, PtrLengthCtor_IntcsMinLength_ThrowsAOORE) {
    const int arr[] = {1};
    EXPECT_THROW(System::ReadOnlyMemory<int>(arr, SharpRuntime::INTCS_MIN),
                 System::ArgumentOutOfRangeException);
}

TEST(ReadOnlyMemoryTests, PtrLengthCtor_ThrowNamesTheLengthParameter) {
    const int arr[] = {1};
    try {
        System::ReadOnlyMemory<int> m(arr, -1);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "length");
    }
}

TEST(ReadOnlyMemoryTests, PtrLengthCtor_ZeroAndPositiveLength_StillConstruct) {
    const int arr[] = {1, 2, 3};
    System::ReadOnlyMemory<int> empty(arr, 0);
    EXPECT_EQ(empty.getLengthProperty(), 0);
    EXPECT_TRUE(empty.getIsEmptyProperty());
    System::ReadOnlyMemory<int> full(arr, 3);
    EXPECT_EQ(full.getLengthProperty(), 3);
    EXPECT_EQ(full[2], 3);
}

TEST(ReadOnlyMemoryTests, VectorCtor_UsesTheSharedCheckedNarrowing) {
    // The vector constructor's only way to a negative length was narrowing a
    // size() > INT32_MAX; it now routes through the same checkedSpanLength guard
    // as Span/ReadOnlySpan/Memory/ArraySegment (unit-tested directly in
    // ReadOnlySpanTests). Empty and ordinary vectors are unaffected.
    std::vector<int> empty;
    System::ReadOnlyMemory<int> me(empty);
    EXPECT_EQ(me.getLengthProperty(), 0);
    std::vector<int> v = {1, 2, 3};
    System::ReadOnlyMemory<int> mv(v);
    EXPECT_EQ(mv.getLengthProperty(), 3);
}

TEST(ReadOnlyMemoryTests, ArraySegmentCtor_ValidSegmentsStillConstruct) {
    // ArraySegment validates its own offset/count, so no segment reachable
    // through the public surface can carry a negative one -- the guard added to
    // this constructor is defence in depth, kept so all three constructors carry
    // one contract. What is testable is that every valid segment still works.
    std::vector<int> v = {1, 2, 3, 4};
    System::ReadOnlyMemory<int> whole{System::ArraySegment<int>(v, 0, 4)};
    EXPECT_EQ(whole.getLengthProperty(), 4);
    System::ReadOnlyMemory<int> part{System::ArraySegment<int>(v, 1, 2)};
    EXPECT_EQ(part.getLengthProperty(), 2);
    EXPECT_EQ(part[0], 2);
    System::ReadOnlyMemory<int> none{System::ArraySegment<int>()};
    EXPECT_EQ(none.getLengthProperty(), 0);
    EXPECT_THROW(System::ArraySegment<int>(v, 0, -1),
                 System::ArgumentOutOfRangeException);
}

TEST(ReadOnlyMemoryTests, ExceptionSpecificationsAreTheApprovedOnes) {
    const int* p = nullptr;
    std::vector<int> v;
    // All three validating constructors gave up noexcept (#1854).
    static_assert(!noexcept(System::ReadOnlyMemory<int>(p, 1)),
                  "#1854: the pointer/length ctor must be able to throw");
    static_assert(!noexcept(System::ReadOnlyMemory<int>(v)),
                  "#1854: the vector ctor must be able to throw");
    static_assert(!std::is_nothrow_constructible_v<System::ReadOnlyMemory<int>,
                                                   const System::ArraySegment<int>&>,
                  "#1854: the ArraySegment ctor must be able to throw");
    // The default constructor validates nothing and keeps constexpr + noexcept,
    // so a ReadOnlyMemory<T> object can still be created in a constant
    // expression. Note the dropped `constexpr` on the pointer/length ctor cost
    // nothing observable: no accessor on this type is constexpr, so even before
    // #1854 a constexpr-constructed ReadOnlyMemory could not be QUERIED in a
    // constant expression.
    static_assert(std::is_nothrow_default_constructible_v<System::ReadOnlyMemory<int>>,
                  "#1854: the default ctor must stay noexcept");
    constexpr System::ReadOnlyMemory<int> defaulted{};
    EXPECT_EQ(defaulted.getLengthProperty(), 0);
    EXPECT_TRUE(defaulted.getIsEmptyProperty());
}

// ---------------------------------------------------------------------------
// Activator
// ---------------------------------------------------------------------------
struct DefaultCtorStruct { int x = 42; };

TEST(ActivatorTests, CreateInstance_DefaultConstructible) {
    auto v = System::Activator::CreateInstance<DefaultCtorStruct>();
    EXPECT_EQ(v.x, 42);
}
TEST(ActivatorTests, CreateInstance_Int_IsZero) {
    auto v = System::Activator::CreateInstance<int>();
    EXPECT_EQ(v, 0);
}
TEST(ActivatorTests, CreateInstancePtr_NotNull) {
    auto p = System::Activator::CreateInstancePtr<DefaultCtorStruct>();
    EXPECT_NE(p.get(), nullptr);
    EXPECT_EQ(p->x, 42);
}
TEST(ActivatorTests, CreateInstancePtr_String_Empty) {
    auto p = System::Activator::CreateInstancePtr<std::string>();
    EXPECT_NE(p.get(), nullptr);
    EXPECT_TRUE(p->empty());
}
TEST(ActivatorTests, CreateInstance_WithArgs_SetsValue) {
    struct Point { int x, y; Point(int a, int b) : x(a), y(b) {} };
    auto p = System::Activator::CreateInstance<Point>(3, 7);
    EXPECT_EQ(p.x, 3);
    EXPECT_EQ(p.y, 7);
}
TEST(ActivatorTests, CreateInstancePtr_WithArgs_NotNull) {
    struct Point { int x, y; Point(int a, int b) : x(a), y(b) {} };
    auto p = System::Activator::CreateInstancePtr<Point>(5, 9);
    ASSERT_NE(p.get(), nullptr);
    EXPECT_EQ(p->x, 5);
    EXPECT_EQ(p->y, 9);
}
TEST(ActivatorTests, CreateInstance_String_WithArg) {
    auto s = System::Activator::CreateInstance<std::string>(std::string("hello"));
    EXPECT_EQ(s, "hello");
}

// ---------------------------------------------------------------------------
// ThreadStaticAttribute
// ---------------------------------------------------------------------------
TEST(ThreadStaticAttributeNewTests, IsA_Attribute) {
    System::ThreadStaticAttribute attr;
    EXPECT_NE(dynamic_cast<System::Attribute*>(&attr), nullptr);
}

// ---------------------------------------------------------------------------
// FlagsAttribute
// ---------------------------------------------------------------------------
TEST(FlagsAttributeNewTests, IsA_Attribute) {
    System::FlagsAttribute attr;
    EXPECT_NE(dynamic_cast<System::Attribute*>(&attr), nullptr);
}
TEST(FlagsAttributeNewTests, DefaultCtor_DoesNotThrow) {
    EXPECT_NO_THROW(System::FlagsAttribute f);
}

// ---------------------------------------------------------------------------
// CCF-004 / SR-AUD-049 — one-argument Slice validates before it subtracts (#1833)
//
// `ReadOnlyMemory<T>::Slice(intcs start)` used to evaluate `length_ - start` as the
// second CALL ARGUMENT, so the subtraction happened before the two-argument overload's
// unsigned check could reject `start`. For `start == INTCS_MIN` that subtraction is
// undefined behaviour; the audited input was `Slice(INTCS_MIN)` on a 3-element memory
// (`signed integer overflow: 3 - -2147483648`). The exception it then threw was already
// correct, which is why this is CCF-004 class B: the repair must leave the exception
// type, paramName and message byte-identical and only move the check earlier.
//
// The whole one-argument-Slice family is covered here, not only the audited type, because
// the inventory that accompanied the fix is part of the finding's remediation:
// `Memory<T>`, `Span<T>`, `ReadOnlySpan<T>` and `ArraySegment<T>` each validate with a
// signed pair-compare BEFORE subtracting and were measured clean. These tests pin that,
// so a later "simplification" of any of them into a forward-then-check shape fails here.
// ---------------------------------------------------------------------------

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ArraySegment.hpp"
#include "System/Memory.hpp"
#include "System/Span.hpp"

namespace {
    // The exact strings measured before the change (build-probe/1833_prefix_values.log).
    constexpr const char* kStartMessage =
        "Specified argument was out of the range of valid values. (Parameter 'start')";
}

TEST(SliceDefinedArithmeticTests, ReadOnlyMemorySliceRejectsIntcsMinWithTheUnchangedException) {
    std::vector<int> v{10, 20, 30};
    System::ReadOnlyMemory<int> m(v.data(), 3);
    try {
        (void)m.Slice(SharpRuntime::INTCS_MIN);
        FAIL() << "Slice(INTCS_MIN) must throw";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "start");
        EXPECT_EQ(e.getMessageProperty(), kStartMessage);
    }
}

TEST(SliceDefinedArithmeticTests, ReadOnlyMemorySliceRejectsEveryOutOfRangeStart) {
    std::vector<int> v{10, 20, 30};
    System::ReadOnlyMemory<int> m(v.data(), 3);
    for (SharpRuntime::intcs bad : {SharpRuntime::INTCS_MIN, SharpRuntime::INTCS_MIN + 1,
                                    static_cast<SharpRuntime::intcs>(-1),
                                    static_cast<SharpRuntime::intcs>(4),
                                    SharpRuntime::INTCS_MAX}) {
        EXPECT_THROW((void)m.Slice(bad), System::ArgumentOutOfRangeException) << "start=" << bad;
    }
}

TEST(SliceDefinedArithmeticTests, ReadOnlyMemorySliceKeepsEveryValidStartUnchanged) {
    std::vector<int> v{10, 20, 30};
    System::ReadOnlyMemory<int> m(v.data(), 3);
    EXPECT_EQ(m.Slice(0).getLengthProperty(), 3);
    EXPECT_EQ(m.Slice(1).getLengthProperty(), 2);
    EXPECT_EQ(m.Slice(2).getLengthProperty(), 1);
    // start == Length is legal and yields an empty memory — the boundary the guard must
    // NOT reject; `>` rather than `>=` in the unsigned compare is what makes it legal.
    EXPECT_EQ(m.Slice(3).getLengthProperty(), 0);
    // Contents, not only lengths.
    auto s = m.Slice(1);
    EXPECT_EQ(s[0], 20);
    EXPECT_EQ(s[1], 30);
    // Composition still works, so the guard is not applied to the wrong length.
    EXPECT_EQ(m.Slice(1).Slice(1).getLengthProperty(), 1);
    EXPECT_EQ(m.Slice(1).Slice(1)[0], 30);
}

TEST(SliceDefinedArithmeticTests, DefaultReadOnlyMemorySliceZeroStaysLegal) {
    System::ReadOnlyMemory<int> empty;
    EXPECT_EQ(empty.Slice(0).getLengthProperty(), 0);
    EXPECT_THROW((void)empty.Slice(1), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)empty.Slice(SharpRuntime::INTCS_MIN), System::ArgumentOutOfRangeException);
}

TEST(SliceDefinedArithmeticTests, EverySiblingOneArgumentSliceAlsoRejectsIntcsMin) {
    // Inventoried alongside the fix and measured clean under UBSan; pinned so that a later
    // rewrite into ReadOnlyMemory's old forward-then-check shape is caught here.
    std::vector<int> v{10, 20, 30};

    System::Memory<int> mem(v);
    EXPECT_THROW((void)mem.Slice(SharpRuntime::INTCS_MIN), System::ArgumentOutOfRangeException);
    EXPECT_EQ(mem.Slice(3).getLengthProperty(), 0);

    System::Span<int> sp(v.data(), 3);
    EXPECT_THROW((void)sp.Slice(SharpRuntime::INTCS_MIN), System::ArgumentOutOfRangeException);
    EXPECT_EQ(sp.Slice(3).getLengthProperty(), 0);

    System::ReadOnlySpan<int> rsp(v.data(), 3);
    EXPECT_THROW((void)rsp.Slice(SharpRuntime::INTCS_MIN), System::ArgumentOutOfRangeException);
    EXPECT_EQ(rsp.Slice(3).getLengthProperty(), 0);

    System::ArraySegment<int> seg(v);
    EXPECT_THROW((void)seg.Slice(SharpRuntime::INTCS_MIN), System::ArgumentOutOfRangeException);
    EXPECT_EQ(seg.Slice(3).getCountProperty(), 0);
}

TEST(SliceDefinedArithmeticTests, ArraySegmentSliceKeepsItsOwnParamNameAndMessage) {
    // Deliberately NOT "start": .NET's ArraySegment<T>.Slice's parameter is named `index`,
    // so this divergence from the rest of the family is correct and is pinned as such.
    std::vector<int> v{10, 20, 30};
    System::ArraySegment<int> seg(v);
    try {
        (void)seg.Slice(SharpRuntime::INTCS_MIN);
        FAIL() << "ArraySegment::Slice(INTCS_MIN) must throw";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "index");
        EXPECT_EQ(e.getMessageProperty(),
                  "ArraySegment::Slice: index out of range (Parameter 'index')");
    }
}

TEST(SliceDefinedArithmeticTests, TwoArgumentReadOnlyMemorySliceIsStillReachableAndUnchanged) {
    // The one-argument guard must not shadow the two-argument overload's own checks.
    std::vector<int> v{10, 20, 30};
    System::ReadOnlyMemory<int> m(v.data(), 3);
    EXPECT_EQ(m.Slice(1, 2).getLengthProperty(), 2);
    EXPECT_EQ(m.Slice(3, 0).getLengthProperty(), 0);
    EXPECT_THROW((void)m.Slice(1, 3), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)m.Slice(SharpRuntime::INTCS_MAX, 10), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)m.Slice(0, -1), System::ArgumentOutOfRangeException);
}
