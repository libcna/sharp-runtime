// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #2054 (family B-C of the System::Buffers
// namespace review, findings SR-AUD-070 and SR-AUD-077).
//
// #2054 documented six generic surfaces' implicit requirements on `T` and added
// a `static_assert` at the point where each was ALREADY enforced. Its central
// claim is therefore a NEGATIVE one -- "exactly the same set of programs
// compiles" -- and it has two halves that live in two different places:
//
//   * the rejected half: every hostile instantiation is still rejected, and now
//     rejected by name. That is
//     test/consumer/buffers_generic_requirements_negative.cpp, 13 sites, run by
//     scripts/check_negative_consumer_fixtures.py. It cannot be a runtime test:
//     a test that does not compile is not a test.
//
//   * the ACCEPTED half, which is this file. A negative fixture cannot express
//     "and this still compiles", because everything in it is compiled with a
//     site enabled or not at all. The spellings below are the ones #2054 must
//     NOT have broken -- naming a hostile instantiation, taking its `sizeof`,
//     and every well-behaved instantiation the surfaces are meant to serve --
//     and each is a compile-time fact pinned by the fact that this translation
//     unit builds, backed by a runtime assertion so the case is not silently
//     optimised into nothing.
//
// Measured before/after table: docs/BuffersNamespaceReviewPlan.md §23.4.
#include <gtest/gtest.h>
#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Buffers/ArrayBufferWriter.hpp"
#include "System/Buffers/ArrayPool.hpp"
#include "System/Buffers/MemoryPool.hpp"
#include "System/Buffers/ReadOnlySequence.hpp"
#include "System/Buffers/SearchValues.hpp"
#include "System/Buffers/SequenceReader.hpp"

using SharpRuntime::intcs;
using System::Buffers::ArrayBufferWriter;
using System::Buffers::ArrayPool;
using System::Buffers::MemoryPool;
using System::Buffers::ReadOnlySequence;
using System::Buffers::SearchValues;
using System::Buffers::SequenceReader;

namespace {

/** Copyable and equality-comparable, but NOT default-constructible. */
struct NoDefault {
    int value;
    explicit NoDefault(int initial) : value(initial) {}
    NoDefault(const NoDefault&) = default;
    NoDefault& operator=(const NoDefault&) = default;
    bool operator==(const NoDefault& other) const { return value == other.value; }
};

/** Default-constructible and copy-constructible, but NOT copy-assignable. */
struct NoCopyAssign {
    int value = 0;
    NoCopyAssign() = default;
    NoCopyAssign(const NoCopyAssign&) = default;
    NoCopyAssign& operator=(const NoCopyAssign&) = delete;
    NoCopyAssign& operator=(NoCopyAssign&&) = delete;
};

/** Default-constructible and equality-comparable, with NO std::hash. */
struct EqualityOnly {
    int value = 0;
    EqualityOnly() = default;
    explicit EqualityOnly(int initial) : value(initial) {}
    bool operator==(const EqualityOnly& other) const { return value == other.value; }
};

} // namespace

// ---------------------------------------------------------------------------
// The hostile instantiations that must stay LEGAL
//
// This is the half a class-scope static_assert would have broken, and the
// reason #2054 put every assert in a member body instead. Measured at
// b294738 (before) and here (after): identical.
// ---------------------------------------------------------------------------

TEST(BuffersGenericRequirementsTests, NamingAHostileInstantiationStaysLegal) {
    ArrayBufferWriter<NoDefault>* writer = nullptr;
    ArrayPool<NoDefault>* arrayPool = nullptr;
    MemoryPool<NoDefault>* memoryPool = nullptr;
    SearchValues<EqualityOnly>* searchValues = nullptr;
    SequenceReader<NoDefault>* reader = nullptr;

    EXPECT_EQ(nullptr, writer);
    EXPECT_EQ(nullptr, arrayPool);
    EXPECT_EQ(nullptr, memoryPool);
    EXPECT_EQ(nullptr, searchValues);
    EXPECT_EQ(nullptr, reader);
}

TEST(BuffersGenericRequirementsTests, ImplicitlyInstantiatingAHostileSpecialisationStaysLegal) {
    // `sizeof` forces the implicit class instantiation -- member declarations,
    // but no member bodies and no vtable -- which is exactly the boundary the
    // asserts must not cross.
    EXPECT_GT(sizeof(ArrayBufferWriter<NoDefault>), 0u);
    EXPECT_GT(sizeof(ArrayPool<NoDefault>), 0u);
    EXPECT_GT(sizeof(MemoryPool<NoDefault>), 0u);
    EXPECT_GT(sizeof(SearchValues<EqualityOnly>), 0u);
    EXPECT_GT(sizeof(SequenceReader<NoDefault>), 0u);
}

TEST(BuffersGenericRequirementsTests, AHostileTypeIsGenuinelyHostile) {
    // If these ever became true the negative fixture would silently stop
    // proving anything, so the properties themselves are pinned here.
    static_assert(!std::is_default_constructible_v<NoDefault>);
    static_assert(std::is_copy_assignable_v<NoDefault>);
    static_assert(std::is_default_constructible_v<NoCopyAssign>);
    static_assert(!std::is_copy_assignable_v<NoCopyAssign>);
    static_assert(std::is_default_constructible_v<EqualityOnly>);
    static_assert(!System::Buffers::detail::searchValuesHashUsable<EqualityOnly>);
    static_assert(System::Buffers::detail::searchValuesEqualityUsable<EqualityOnly>);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// The requirement predicates say yes for every element type the port serves
// ---------------------------------------------------------------------------

TEST(BuffersGenericRequirementsTests, EveryServedElementTypeMeetsTheStatedRequirements) {
    static_assert(std::is_default_constructible_v<std::uint8_t>);
    static_assert(std::is_default_constructible_v<char>);
    static_assert(std::is_default_constructible_v<int>);
    static_assert(std::is_default_constructible_v<std::string>);
    static_assert(std::is_copy_assignable_v<std::uint8_t>);
    static_assert(std::is_copy_assignable_v<char>);
    static_assert(std::is_copy_assignable_v<int>);
    static_assert(std::is_copy_assignable_v<std::string>);
    static_assert(System::Buffers::detail::searchValuesHashUsable<std::uint8_t>);
    static_assert(System::Buffers::detail::searchValuesHashUsable<char>);
    static_assert(System::Buffers::detail::searchValuesHashUsable<int>);
    static_assert(System::Buffers::detail::searchValuesHashUsable<std::string>);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// The requiring members still work, for a trivial AND a non-trivial T
// ---------------------------------------------------------------------------

TEST(BuffersGenericRequirementsTests, ArrayBufferWriterStillServesANonTrivialElement) {
    ArrayBufferWriter<std::string> writer(2);
    ASSERT_EQ(2, writer.getCapacityProperty());

    System::Span<std::string> span = writer.GetSpan(2);
    ASSERT_EQ(2, span.getLengthProperty());
    span[0] = "alpha";
    span[1] = "beta";
    writer.Advance(2);
    EXPECT_EQ(2, writer.getWrittenCountProperty());
    EXPECT_EQ("alpha", writer.getWrittenSpanProperty()[0]);

    writer.Clear();
    EXPECT_EQ(0, writer.getWrittenCountProperty());
    // Clear() writes a value-initialized T over what was written, which for a
    // std::string is the empty string, not untouched storage.
    EXPECT_EQ("", writer.GetSpan(2)[0]);
}

TEST(BuffersGenericRequirementsTests, ArrayBufferWriterResetWrittenCountNeedsNeitherRequirement) {
    // The member the Clear() doc-comment points a caller at when T cannot be
    // assigned. NoCopyAssign IS default-constructible, so construction and
    // growth are fine; only Clear() is out of reach.
    ArrayBufferWriter<NoCopyAssign> writer(4);
    ASSERT_EQ(4, writer.getCapacityProperty());
    writer.Advance(2);
    ASSERT_EQ(2, writer.getWrittenCountProperty());
    writer.ResetWrittenCount();
    EXPECT_EQ(0, writer.getWrittenCountProperty());
    EXPECT_EQ(4, writer.getCapacityProperty());
}

TEST(BuffersGenericRequirementsTests, PoolsStillServeANonTrivialElement) {
    auto owner = MemoryPool<std::string>::Shared().Rent(4);
    ASSERT_NE(nullptr, owner);
    EXPECT_GE(owner->getMemoryProperty().getLengthProperty(), 4);
    EXPECT_EQ("", owner->getMemoryProperty().getSpanProperty()[0]);

    std::vector<std::string> rented = ArrayPool<std::string>::Shared().Rent(3);
    ASSERT_EQ(3u, rented.size());
    rented[0] = "gamma";
    ArrayPool<std::string>::Shared().Return(rented, true);
    EXPECT_EQ(3u, rented.size());
    EXPECT_EQ("", rented[0]);
}

TEST(BuffersGenericRequirementsTests, SearchValuesStillServesANonTrivialElement) {
    SearchValues<std::string> values(std::vector<std::string>{"alpha", "beta"});
    EXPECT_TRUE(values.Contains("alpha"));
    EXPECT_FALSE(values.Contains("gamma"));
    EXPECT_EQ(2u, values.GetValues().size());
}

TEST(BuffersGenericRequirementsTests, SequenceReaderStillServesANonTrivialElement) {
    std::vector<std::string> storage{"alpha"};
    ReadOnlySequence<std::string> sequence(storage);
    SequenceReader<std::string> reader(sequence);

    std::string value = "sentinel";
    ASSERT_TRUE(reader.TryPeek(value));
    EXPECT_EQ("alpha", value);
    ASSERT_TRUE(reader.TryRead(value));
    EXPECT_EQ("alpha", value);

    value = "sentinel";
    EXPECT_FALSE(reader.TryRead(value));
    // The CCF-014 contract the default-constructibility requirement exists for.
    EXPECT_EQ("", value);
}
