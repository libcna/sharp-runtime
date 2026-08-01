// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #1936: ImmutableSortedSet<T>::SetEquals must use equivalence under
// this set's ordering comparer, not raw element operator==.
#include <gtest/gtest.h>

#include <bit>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "System/Collections/Immutable/ImmutableSortedSet.hpp"
#include "System/detail/ComparisonPolicy.hpp"

namespace ImmutableSortedSetEqualityContractSupport {

namespace I = System::Collections::Immutable;
namespace P = System::detail;

template<typename T>
using Set = I::ImmutableSortedSet<T>;

template<typename F>
F firstNaN();

template<>
float firstNaN<float>() {
    return std::bit_cast<float>(UINT32_C(0x7fc00001));
}

template<>
double firstNaN<double>() {
    return std::bit_cast<double>(UINT64_C(0x7ff8000000000001));
}

template<>
long double firstNaN<long double>() {
    return std::nanl("1936a");
}

template<typename F>
F secondNaN();

template<>
float secondNaN<float>() {
    return std::bit_cast<float>(UINT32_C(0x7fc00002));
}

template<>
double secondNaN<double>() {
    return std::bit_cast<double>(UINT64_C(0x7ff8000000000002));
}

template<>
long double secondNaN<long double>() {
    return std::nanl("1936b");
}

template<typename T>
void expectEqualRelations(const Set<T>& left, const Set<T>& right) {
    EXPECT_TRUE(left.SetEquals(right));
    EXPECT_TRUE(left.IsSubsetOf(right));
    EXPECT_FALSE(left.IsProperSubsetOf(right));
    EXPECT_TRUE(left.IsSupersetOf(right));
    EXPECT_FALSE(left.IsProperSupersetOf(right));
}

template<typename T>
void expectEqualOperationResults(const Set<T>& left, const Set<T>& right) {
    EXPECT_TRUE(left.Overlaps(right));
    EXPECT_EQ(left.Intersect(right).getCountProperty(), left.getCountProperty());
    EXPECT_EQ(left.Union(right).getCountProperty(), left.getCountProperty());
    EXPECT_TRUE(left.Except(right).getIsEmptyProperty());
    EXPECT_TRUE(left.SymmetricExcept(right).getIsEmptyProperty());
}

template<typename F>
void directFloatingMatrix() {
    const F nan1 = firstNaN<F>();
    const F nan2 = secondNaN<F>();
    const F infinity = std::numeric_limits<F>::infinity();

    const auto empty = Set<F>::Empty();
    const auto emptyIndependent = Set<F>::Empty();
    expectEqualRelations(empty, emptyIndependent);
    EXPECT_FALSE(empty.Overlaps(emptyIndependent));
    EXPECT_TRUE(empty.Intersect(emptyIndependent).getIsEmptyProperty());
    EXPECT_TRUE(empty.Union(emptyIndependent).getIsEmptyProperty());
    EXPECT_TRUE(empty.Except(emptyIndependent).getIsEmptyProperty());
    EXPECT_TRUE(empty.SymmetricExcept(emptyIndependent).getIsEmptyProperty());

    const auto oneFinite = Set<F>::Create({F(1)});
    expectEqualRelations(oneFinite, Set<F>::Create({F(1)}));

    const auto oneNaN = Set<F>::Create({nan1});
    const auto sharedCopy = oneNaN;
    expectEqualRelations(oneNaN, oneNaN);
    expectEqualRelations(oneNaN, sharedCopy);
    expectEqualRelations(oneNaN, Set<F>::Create({nan1}));
    expectEqualRelations(oneNaN, Set<F>::Create({nan2}));
    expectEqualOperationResults(oneNaN, Set<F>::Create({nan2}));

    const auto signedZero = Set<F>::Create({F(0)});
    const auto negativeZero = Set<F>::Create({-F(0)});
    expectEqualRelations(signedZero, negativeZero);
    expectEqualOperationResults(signedZero, negativeZero);

    const auto finiteAndInfinite =
        Set<F>::Create({-infinity, F(-3), F(0), F(4), infinity});
    const auto differentInsertion =
        Set<F>::Create({infinity, F(4), F(0), F(-3), -infinity});
    expectEqualRelations(finiteAndInfinite, differentInsertion);
    expectEqualOperationResults(finiteAndInfinite, differentInsertion);

    const auto mixed = Set<F>::Create({nan1, F(1), F(2), infinity});
    const auto mixedDifferentPayloadAndInsertion =
        Set<F>::Create({infinity, F(2), F(1), nan2});
    expectEqualRelations(mixed, mixedDifferentPayloadAndInsertion);
    expectEqualOperationResults(mixed, mixedDifferentPayloadAndInsertion);

    const auto duplicateInput =
        Set<F>::Create(std::vector<F>{nan1, nan2, F(1), F(1)});
    expectEqualRelations(duplicateInput, Set<F>::Create({nan2, F(1)}));

    const auto properSubset = Set<F>::Create({nan1, F(1)});
    const auto properSuperset = Set<F>::Create({nan2, F(1), F(2)});
    EXPECT_TRUE(properSubset.IsSubsetOf(properSuperset));
    EXPECT_TRUE(properSubset.IsProperSubsetOf(properSuperset));
    EXPECT_FALSE(properSubset.IsSupersetOf(properSuperset));
    EXPECT_FALSE(properSubset.IsProperSupersetOf(properSuperset));
    EXPECT_TRUE(properSuperset.IsSupersetOf(properSubset));
    EXPECT_TRUE(properSuperset.IsProperSupersetOf(properSubset));
    EXPECT_FALSE(properSuperset.IsSubsetOf(properSubset));
    EXPECT_FALSE(properSuperset.IsProperSubsetOf(properSubset));

    const auto unequalLeft = Set<F>::Create({nan1, F(2)});
    const auto unequalRight = Set<F>::Create({nan2, F(1)});
    EXPECT_FALSE(unequalLeft.SetEquals(unequalRight));
    EXPECT_FALSE(unequalLeft.IsSubsetOf(unequalRight));
    EXPECT_FALSE(unequalLeft.IsSupersetOf(unequalRight));
    EXPECT_FALSE(unequalLeft.IsProperSubsetOf(unequalRight));
    EXPECT_FALSE(unequalLeft.IsProperSupersetOf(unequalRight));
    EXPECT_TRUE(unequalLeft.Overlaps(unequalRight));
    EXPECT_EQ(unequalLeft.Intersect(unequalRight).getCountProperty(), 1);
    EXPECT_EQ(unequalLeft.Union(unequalRight).getCountProperty(), 3);
    EXPECT_EQ(unequalLeft.Except(unequalRight).getCountProperty(), 1);
    EXPECT_EQ(unequalLeft.SymmetricExcept(unequalRight).getCountProperty(), 2);

    const std::function<bool(const F&, const F&)> comparer = P::DefaultKeyLess<F>{};
    const auto comparerIdenticalLeft =
        Set<F>::Create(comparer, std::vector<F>{nan1, F(1), F(2)});
    const auto comparerIdenticalRight =
        Set<F>::Create(comparer, std::vector<F>{F(2), nan2, F(1)});
    expectEqualRelations(comparerIdenticalLeft, comparerIdenticalRight);

    const auto reverse = [comparer](const F& left, const F& right) {
        return comparer(right, left);
    };
    const auto reverseOrdered =
        Set<F>::Create(reverse, std::vector<F>{F(2), nan2, F(1)});
    expectEqualRelations(comparerIdenticalLeft, reverseOrdered);
    expectEqualOperationResults(comparerIdenticalLeft, reverseOrdered);
}

template<typename F>
void nullableFloatingMatrix() {
    using O = std::optional<F>;
    const O none;
    const O nan1(firstNaN<F>());
    const O nan2(secondNaN<F>());
    const O zero(F(0));
    const O negativeZero(-F(0));
    const O finite(F(2));
    const O infinity(std::numeric_limits<F>::infinity());

    EXPECT_FALSE(nan1 == nan2);  // Raw Nullable/operator behavior remains unchanged.
    EXPECT_TRUE(P::equalValues(nan1, nan2));
    EXPECT_FALSE(P::DefaultKeyLess<O>{}(nan1, nan2));
    EXPECT_FALSE(P::DefaultKeyLess<O>{}(nan2, nan1));

    const auto left = Set<O>::Create({none, nan1, zero, finite, infinity});
    const auto right = Set<O>::Create({infinity, finite, negativeZero, nan2, none});
    expectEqualRelations(left, left);
    const auto sharedCopy = left;
    expectEqualRelations(left, sharedCopy);
    expectEqualRelations(left, right);
    expectEqualOperationResults(left, right);
    EXPECT_TRUE(left.Contains(none));
    EXPECT_TRUE(left.Contains(nan2));
    EXPECT_TRUE(left.Contains(negativeZero));
    EXPECT_TRUE(left.Contains(infinity));

    const auto nullOnly = Set<O>::Create({none});
    const auto nullAndNaN = Set<O>::Create({none, nan1});
    EXPECT_TRUE(nullOnly.IsSubsetOf(nullAndNaN));
    EXPECT_TRUE(nullOnly.IsProperSubsetOf(nullAndNaN));
    EXPECT_TRUE(nullAndNaN.IsSupersetOf(nullOnly));
    EXPECT_TRUE(nullAndNaN.IsProperSupersetOf(nullOnly));
    EXPECT_FALSE(nullOnly.SetEquals(nullAndNaN));

    const auto unequal = Set<O>::Create({none, nan2, zero, O(F(3)), infinity});
    EXPECT_FALSE(left.SetEquals(unequal));
    EXPECT_FALSE(left.IsProperSubsetOf(unequal));
    EXPECT_FALSE(left.IsProperSupersetOf(unequal));
}

std::string lowerAscii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

const auto caseInsensitiveLess = [](const std::string& left, const std::string& right) {
    return lowerAscii(left) < lowerAscii(right);
};

} // namespace ImmutableSortedSetEqualityContractSupport

using namespace ImmutableSortedSetEqualityContractSupport;

TEST(ImmutableSortedSetEqualityContract, DirectFloatCompleteMatrix) {
    directFloatingMatrix<float>();
}

TEST(ImmutableSortedSetEqualityContract, DirectDoubleCompleteMatrix) {
    directFloatingMatrix<double>();
}

TEST(ImmutableSortedSetEqualityContract, DirectLongDoubleCompleteMatrix) {
    directFloatingMatrix<long double>();
}

TEST(ImmutableSortedSetEqualityContract, NullableFloatRegressionMatrix) {
    nullableFloatingMatrix<float>();
}

TEST(ImmutableSortedSetEqualityContract, NullableDoubleRegressionMatrix) {
    nullableFloatingMatrix<double>();
}

TEST(ImmutableSortedSetEqualityContract, NullableLongDoubleRegressionMatrix) {
    nullableFloatingMatrix<long double>();
}

TEST(ImmutableSortedSetEqualityContract, CaseInsensitiveComparerDefinesEquality) {
    const auto upper = Set<std::string>::Create(caseInsensitiveLess, {"A"});
    const auto lower = Set<std::string>::Create(caseInsensitiveLess, {"a"});
    expectEqualRelations(upper, lower);
    expectEqualOperationResults(upper, lower);
}

TEST(ImmutableSortedSetEqualityContract, ThisComparerRebuildsAndCollapsesOther) {
    const auto thisSet = Set<std::string>::Create(caseInsensitiveLess, {"A"});
    const auto other = Set<std::string>::Create({"A", "a"});
    expectEqualRelations(thisSet, other);
    EXPECT_FALSE(other.SetEquals(thisSet));
    EXPECT_TRUE(other.IsProperSupersetOf(thisSet));
    EXPECT_FALSE(other.IsProperSubsetOf(thisSet));
}

TEST(ImmutableSortedSetEqualityContract, ComparatorCanCollapseRawUnequalIntegers) {
    const auto sameDecade = [](const int& left, const int& right) {
        return left / 10 < right / 10;
    };
    const auto eleven = Set<int>::Create(sameDecade, {11});
    const auto nineteen = Set<int>::Create(sameDecade, {19});
    expectEqualRelations(eleven, nineteen);

    const auto rawDistinct = Set<int>::Create({11, 19});
    expectEqualRelations(eleven, rawDistinct);
}

TEST(ImmutableSortedSetEqualityContract, OrdinaryComparerControlsRemainOrdinary) {
    const std::function<bool(const int&, const int&)> ascending = std::less<int>{};
    const auto left = Set<int>::Create(ascending, {1, 2, 3});
    const auto equal = Set<int>::Create(ascending, {3, 2, 1});
    expectEqualRelations(left, equal);
    expectEqualOperationResults(left, equal);

    const auto smallerFirstMismatch = Set<int>::Create(ascending, {2});
    const auto largerFirstMismatch = Set<int>::Create(ascending, {1});
    EXPECT_FALSE(smallerFirstMismatch.SetEquals(largerFirstMismatch));
    EXPECT_FALSE(largerFirstMismatch.SetEquals(smallerFirstMismatch));
}

TEST(ImmutableSortedSetEqualityContract, CustomComparerRelatedOperationsStayComparerDriven) {
    const auto left = Set<std::string>::Create(caseInsensitiveLess, {"A", "B"});
    const auto right = Set<std::string>::Create({"b", "a"});
    expectEqualRelations(left, right);
    expectEqualOperationResults(left, right);
    EXPECT_TRUE(left.Contains("a"));
    EXPECT_TRUE(left.Intersect(right).Contains("A"));
    EXPECT_TRUE(left.Union(right).Contains("b"));
}
