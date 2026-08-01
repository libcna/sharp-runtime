// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #1934: the direct nullable-floating default-comparer contract.
#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>

#include "System/Collections/Generic/Comparer.hpp"
#include "System/Collections/Generic/EqualityComparer.hpp"
#include "System/Collections/Generic/IComparer.hpp"
#include "System/Collections/Generic/NullableComparer.hpp"
#include "System/Collections/Generic/NullableEqualityComparer.hpp"
#include "System/Collections/Generic/ObjectComparer.hpp"
#include "System/Collections/Generic/ObjectEqualityComparer.hpp"
#include "System/Nullable.hpp"
#include "System/detail/ComparisonPolicy.hpp"

namespace NullableFloatingComparerContractSupport {

enum class ProbeEnum : unsigned { low = 1, high = 2 };

struct ProbeValue {
    int value;
    friend bool operator<(const ProbeValue& a, const ProbeValue& b) noexcept {
        return a.value < b.value;
    }
    friend bool operator==(const ProbeValue& a, const ProbeValue& b) noexcept {
        return a.value == b.value;
    }
};

} // namespace NullableFloatingComparerContractSupport

template<>
struct std::hash<NullableFloatingComparerContractSupport::ProbeValue> {
    std::size_t operator()(
            const NullableFloatingComparerContractSupport::ProbeValue& value) const noexcept {
        return std::hash<int>{}(value.value);
    }
};

namespace {

namespace G = System::Collections::Generic;
namespace D = System::detail;
using NullableFloatingComparerContractSupport::ProbeEnum;
using NullableFloatingComparerContractSupport::ProbeValue;

template<class F>
F payloadNaN();

template<>
float payloadNaN<float>() {
    return std::bit_cast<float>(UINT32_C(0x7fc00007));
}

template<>
double payloadNaN<double>() {
    return std::bit_cast<double>(UINT64_C(0x7ff8000000000042));
}

template<>
long double payloadNaN<long double>() {
    return std::nanl("1934");
}

template<class F>
void expectDirectNullableFloatingMatrix() {
    using O = std::optional<F>;
    const O none;
    const O zero(F(0));
    const O negativeZero(-F(0));
    const O one(F(1));
    const O two(F(2));
    const O nan(std::numeric_limits<F>::quiet_NaN());
    const O otherNaN(payloadNaN<F>());
    const O positiveInfinity(std::numeric_limits<F>::infinity());
    const O negativeInfinity(-std::numeric_limits<F>::infinity());
    const O lowest(std::numeric_limits<F>::lowest());
    const O maximum(std::numeric_limits<F>::max());

    const auto& comparer = G::Comparer<O>::Default();
    const G::IComparer<O>& comparerInterface = comparer;
    G::NullableComparer<F> nullableComparer;
    const G::Comparer<O>& nullableComparerBase = nullableComparer;
    const G::IComparer<O>& nullableComparerInterface = nullableComparer;
    G::ObjectComparer<O> objectComparer;

    EXPECT_EQ(comparer.Compare(none, none), 0);
    EXPECT_LT(comparer.Compare(none, one), 0);
    EXPECT_GT(comparer.Compare(one, none), 0);
    EXPECT_EQ(comparer.Compare(one, one), 0);
    EXPECT_LT(comparer.Compare(one, two), 0);
    EXPECT_GT(comparer.Compare(two, one), 0);
    EXPECT_EQ(comparer.Compare(zero, negativeZero), 0);
    EXPECT_LT(comparer.Compare(nan, negativeInfinity), 0);
    EXPECT_GT(comparer.Compare(negativeInfinity, nan), 0);
    EXPECT_EQ(comparer.Compare(nan, otherNaN), 0);
    EXPECT_LT(comparer.Compare(lowest, maximum), 0);
    EXPECT_LT(comparer.Compare(negativeInfinity, lowest), 0);
    EXPECT_LT(comparer.Compare(maximum, positiveInfinity), 0);
    EXPECT_GT(comparer.Compare(positiveInfinity, nan), 0);

    EXPECT_EQ(comparerInterface.Compare(nan, one), comparer.Compare(nan, one));
    EXPECT_EQ(nullableComparer.Compare(nan, one), comparer.Compare(nan, one));
    EXPECT_EQ(nullableComparerBase.Compare(nan, one), comparer.Compare(nan, one));
    EXPECT_EQ(nullableComparerInterface.Compare(nan, one), comparer.Compare(nan, one));
    EXPECT_EQ(objectComparer.Compare(nan, one), comparer.Compare(nan, one));
    EXPECT_EQ(nullableComparer.Compare(none, none), comparer.Compare(none, none));
    EXPECT_EQ(nullableComparer.Compare(none, nan), comparer.Compare(none, nan));

    const auto& equality = G::EqualityComparer<O>::Default();
    const G::IEqualityComparer<O>& equalityInterface = equality;
    G::NullableEqualityComparer<F> nullableEquality;
    const G::EqualityComparer<O>& nullableEqualityBase = nullableEquality;
    const G::IEqualityComparer<O>& nullableEqualityInterface = nullableEquality;
    G::ObjectEqualityComparer<O> objectEquality;

    EXPECT_TRUE(equality.Equals(none, none));
    EXPECT_FALSE(equality.Equals(none, one));
    EXPECT_FALSE(equality.Equals(one, none));
    EXPECT_TRUE(equality.Equals(one, one));
    EXPECT_FALSE(equality.Equals(one, two));
    EXPECT_TRUE(equality.Equals(nan, otherNaN));
    EXPECT_TRUE(equality.Equals(zero, negativeZero));
    EXPECT_TRUE(equality.Equals(positiveInfinity, positiveInfinity));
    EXPECT_FALSE(equality.Equals(positiveInfinity, negativeInfinity));
    EXPECT_EQ(equality.GetHashCode(none), 0);
    EXPECT_EQ(equality.GetHashCode(nan), equality.GetHashCode(otherNaN));
    EXPECT_EQ(equality.GetHashCode(zero), equality.GetHashCode(negativeZero));

    EXPECT_EQ(equalityInterface.Equals(nan, otherNaN), equality.Equals(nan, otherNaN));
    EXPECT_EQ(nullableEquality.Equals(nan, otherNaN), equality.Equals(nan, otherNaN));
    EXPECT_EQ(nullableEqualityBase.Equals(nan, otherNaN), equality.Equals(nan, otherNaN));
    EXPECT_EQ(nullableEqualityInterface.Equals(nan, otherNaN), equality.Equals(nan, otherNaN));
    EXPECT_EQ(objectEquality.Equals(nan, otherNaN), equality.Equals(nan, otherNaN));
    EXPECT_EQ(objectEquality.GetHashCode(otherNaN), equality.GetHashCode(otherNaN));
    EXPECT_EQ(nullableEquality.GetHashCode(none), equality.GetHashCode(none));
    EXPECT_EQ(nullableEquality.GetHashCode(otherNaN), equality.GetHashCode(otherNaN));

    EXPECT_EQ(D::compareValues(nan, one), comparer.Compare(nan, one));
    EXPECT_EQ(D::equalValues(nan, otherNaN), equality.Equals(nan, otherNaN));
    EXPECT_EQ(static_cast<SharpRuntime::intcs>(D::hashValue(none)),
              equality.GetHashCode(none));
    EXPECT_EQ(static_cast<SharpRuntime::intcs>(D::hashValue(nan)),
              equality.GetHashCode(nan));

    // Raw lifted equality is a deliberately different .NET surface.
    EXPECT_FALSE(nan == otherNaN);
    EXPECT_FALSE(System::Nullable<F>(nan.value()) == System::Nullable<F>(otherNaN.value()));

    auto* reverse = G::Comparer<O>::Create(
        [](const O& a, const O& b) -> SharpRuntime::intcs {
            if (a < b) return 1;
            if (b < a) return -1;
            return 0;
        });
    EXPECT_GT(reverse->Compare(one, two), 0);
    EXPECT_EQ(reverse->Compare(nan, one), 0); // caller policy stays authoritative
    delete reverse;

    auto callerEquality = G::EqualityComparer<O>::Create(
        [](const O&, const O&) { return false; },
        [](const O&) -> SharpRuntime::intcs { return 77; });
    EXPECT_FALSE(callerEquality->Equals(none, none));
    EXPECT_EQ(callerEquality->GetHashCode(nan), 77);
}

template<class T>
void expectNonFloatingControl(const T& first, const T& second) {
    using O = std::optional<T>;
    const O none;
    const O a(first);
    const O b(second);
    const auto& comparer = G::Comparer<O>::Default();
    const auto& equality = G::EqualityComparer<O>::Default();
    EXPECT_EQ(comparer.Compare(none, none), 0);
    EXPECT_LT(comparer.Compare(none, a), 0);
    EXPECT_GT(comparer.Compare(a, none), 0);
    EXPECT_LT(comparer.Compare(a, b), 0);
    EXPECT_TRUE(equality.Equals(none, none));
    EXPECT_TRUE(equality.Equals(a, a));
    EXPECT_FALSE(equality.Equals(a, b));
    EXPECT_EQ(equality.GetHashCode(none),
              static_cast<SharpRuntime::intcs>(std::hash<O>{}(none)));
    static_assert(std::is_same_v<D::DefaultKeyLess<O>, std::less<O>>);
    static_assert(std::is_same_v<D::DefaultKeyHash<O>, std::hash<O>>);
    static_assert(std::is_same_v<D::DefaultKeyEqual<O>, std::equal_to<O>>);
}

} // namespace

TEST(NullableFloatingComparerContract, FloatCompleteMatrix) {
    expectDirectNullableFloatingMatrix<float>();
}

TEST(NullableFloatingComparerContract, DoubleCompleteMatrix) {
    expectDirectNullableFloatingMatrix<double>();
}

TEST(NullableFloatingComparerContract, LongDoubleCompleteMatrix) {
    expectDirectNullableFloatingMatrix<long double>();
}

TEST(NullableFloatingComparerContract, RepresentativeNonFloatingOptionalsAreUnchanged) {
    expectNonFloatingControl<int>(1, 2);
    expectNonFloatingControl<unsigned>(1u, 2u);
    expectNonFloatingControl<std::string>("a", "b");
    expectNonFloatingControl<ProbeEnum>(ProbeEnum::low, ProbeEnum::high);
    expectNonFloatingControl<ProbeValue>(ProbeValue{1}, ProbeValue{2});
}

TEST(NullableFloatingComparerContract, PublicShapeAndLanguagePropertiesStayFixed) {
    using O = std::optional<double>;
    using CompareMember = SharpRuntime::intcs (G::Comparer<O>::*)(const O&, const O&) const;
    using EqualsMember = bool (G::EqualityComparer<O>::*)(const O&, const O&) const;
    using HashMember = SharpRuntime::intcs (G::EqualityComparer<O>::*)(const O&) const;
    static_assert(std::is_same_v<decltype(&G::Comparer<O>::Compare), CompareMember>);
    static_assert(std::is_same_v<decltype(&G::EqualityComparer<O>::Equals), EqualsMember>);
    static_assert(std::is_same_v<decltype(&G::EqualityComparer<O>::GetHashCode), HashMember>);
    static_assert(std::has_virtual_destructor_v<G::Comparer<O>>);
    static_assert(std::has_virtual_destructor_v<G::EqualityComparer<O>>);
    static_assert(sizeof(G::Comparer<O>) == sizeof(void*));
    static_assert(sizeof(G::EqualityComparer<O>) == sizeof(void*));
    static_assert(sizeof(G::NullableComparer<double>) == sizeof(void*));
    static_assert(sizeof(G::NullableEqualityComparer<double>) == sizeof(void*));

    constexpr O none;
    constexpr O one(1.0);
    static_assert(D::compareValues(none, one) < 0);
    static_assert(D::equalValues(none, none));
    static_assert(!noexcept(D::compareValues(none, one)));
    static_assert(!noexcept(D::equalValues(none, one)));
    static_assert(!noexcept(D::hashValue(none)));
    static_assert(noexcept(D::DefaultLess<O>{}(none, one)));

    SUCCEED();
}
