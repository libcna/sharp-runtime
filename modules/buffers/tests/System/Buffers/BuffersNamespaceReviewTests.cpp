// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for the System::Buffers namespace review (ticket #2048,
// docs/BuffersNamespaceReviewPlan.md). One section per repair ticket.
#include <gtest/gtest.h>
#include <optional>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Buffers/ArrayBufferWriter.hpp"
#include "System/Buffers/ArrayPool.hpp"
#include "System/Buffers/BuffersExtensions.hpp"
#include "System/Buffers/ReadOnlySequence.hpp"
#include "System/Buffers/StandardFormat.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/OutOfMemoryException.hpp"

using SharpRuntime::intcs;
using SharpRuntime::INTCS_MAX;
using SharpRuntime::INTCS_MIN;
using System::Buffers::ArrayBufferWriter;
using System::Buffers::ArrayPool;
using System::Buffers::ReadOnlySequence;
using System::Buffers::StandardFormat;
namespace BuffersExtensions = System::Buffers::BuffersExtensions;

// ===========================================================================
// #2049 / SR-AUD-072 — the raw pointer/length constructor validates its metadata
//
// Before this ticket `data_(ptr, ptr + length)` ran straight from the member
// initialiser: (nullptr, 1) reached a UBSan null load and an ASan SEGV inside the
// constructor, and (ptr, -1) escaped a native std::length_error.
// ===========================================================================

TEST(ReadOnlySequenceRawCtorTests, NullPointerWithPositiveLength_ThrowsArgumentNull) {
    EXPECT_THROW(ReadOnlySequence<int>(static_cast<const int*>(nullptr), 1),
                 System::ArgumentNullException);
}

TEST(ReadOnlySequenceRawCtorTests, NullPointerWithMaximumLength_ThrowsArgumentNull) {
    EXPECT_THROW(ReadOnlySequence<int>(static_cast<const int*>(nullptr), INTCS_MAX),
                 System::ArgumentNullException);
}

TEST(ReadOnlySequenceRawCtorTests, NegativeLength_ThrowsArgumentOutOfRangeNotLengthError) {
    int data[3] = {1, 2, 3};
    EXPECT_THROW(ReadOnlySequence<int>(data, -1), System::ArgumentOutOfRangeException);
}

TEST(ReadOnlySequenceRawCtorTests, MinimumLength_ThrowsArgumentOutOfRange) {
    int data[3] = {1, 2, 3};
    EXPECT_THROW(ReadOnlySequence<int>(data, INTCS_MIN), System::ArgumentOutOfRangeException);
}

TEST(ReadOnlySequenceRawCtorTests, NegativeLengthWithNullPointer_RejectsLengthFirst) {
    // The length domain is checked before the null check, so the caller learns about the
    // impossible length rather than about a pointer that could never have been used anyway.
    EXPECT_THROW(ReadOnlySequence<int>(static_cast<const int*>(nullptr), -1),
                 System::ArgumentOutOfRangeException);
}

TEST(ReadOnlySequenceRawCtorTests, NullPointerWithZeroLength_StaysValid) {
    ReadOnlySequence<int> seq(static_cast<const int*>(nullptr), 0);
    EXPECT_TRUE(seq.getIsEmptyProperty());
    EXPECT_EQ(seq.getLengthProperty(), 0LL);
}

TEST(ReadOnlySequenceRawCtorTests, ValidPointerWithZeroLength_IsEmpty) {
    int data[3] = {1, 2, 3};
    ReadOnlySequence<int> seq(data, 0);
    EXPECT_TRUE(seq.getIsEmptyProperty());
    EXPECT_EQ(seq.getLengthProperty(), 0LL);
}

TEST(ReadOnlySequenceRawCtorTests, ValidPointerWithOneElement_CopiesIt) {
    int data[3] = {7, 8, 9};
    ReadOnlySequence<int> seq(data, 1);
    ASSERT_EQ(seq.getLengthProperty(), 1LL);
    EXPECT_EQ(seq.First().getSpanProperty()[0], 7);
}

TEST(ReadOnlySequenceRawCtorTests, SourceNeedNotOutliveTheSequence) {
    // The constructor copies, so a sequence built from a block that then dies stays readable.
    ReadOnlySequence<int> seq = [] {
        std::vector<int> scratch{4, 5, 6};
        return ReadOnlySequence<int>(scratch.data(), 3);
    }();
    ASSERT_EQ(seq.getLengthProperty(), 3LL);
    auto span = seq.First().getSpanProperty();
    EXPECT_EQ(span[0], 4);
    EXPECT_EQ(span[2], 6);
}

// ===========================================================================
// #2050 / SR-AUD-073 — TryGet validates the position before forming a view
// ===========================================================================

TEST(ReadOnlySequenceTryGetTests, PositionBelowStart_ThrowsInsteadOfExposingOutOfSliceData) {
    ReadOnlySequence<int> seq(std::vector<int>{10, 20, 30});
    auto sliced = seq.Slice(seq.GetPosition(1), seq.getEndProperty());
    ASSERT_EQ(sliced.getLengthProperty(), 2LL);

    System::SequencePosition forged(nullptr, 0);
    System::ReadOnlyMemory<int> mem;
    EXPECT_THROW((void)sliced.TryGet(forged, mem, false), System::ArgumentOutOfRangeException);
}

TEST(ReadOnlySequenceTryGetTests, ParentStartPositionOnASlice_IsRejected) {
    // This needs no forgery at all: getStartProperty() is a legitimately obtained position,
    // and holding it across a Slice is an ordinary caller mistake.
    ReadOnlySequence<int> seq(std::vector<int>{10, 20, 30});
    System::SequencePosition parentStart = seq.getStartProperty();
    auto sliced = seq.Slice(seq.GetPosition(2), seq.getEndProperty());

    System::ReadOnlyMemory<int> mem;
    EXPECT_THROW((void)sliced.TryGet(parentStart, mem, false), System::ArgumentOutOfRangeException);
}

TEST(ReadOnlySequenceTryGetTests, NegativePosition_ThrowsInsteadOfReadingBeforeTheAllocation) {
    ReadOnlySequence<int> seq(std::vector<int>{10, 20, 30});
    System::SequencePosition forged(nullptr, -1);
    System::ReadOnlyMemory<int> mem;
    EXPECT_THROW((void)seq.TryGet(forged, mem, false), System::ArgumentOutOfRangeException);
}

TEST(ReadOnlySequenceTryGetTests, PositionBeyondEnd_Throws) {
    ReadOnlySequence<int> seq(std::vector<int>{10, 20, 30});
    System::SequencePosition beyond(nullptr, 4);
    System::ReadOnlyMemory<int> mem;
    EXPECT_THROW((void)seq.TryGet(beyond, mem, false), System::ArgumentOutOfRangeException);
}

TEST(ReadOnlySequenceTryGetTests, PositionAtEnd_StillReturnsFalseWithEmptyMemory) {
    ReadOnlySequence<int> seq(std::vector<int>{10, 20, 30});
    System::SequencePosition pos = seq.getEndProperty();
    System::ReadOnlyMemory<int> mem(nullptr, 0);
    EXPECT_FALSE(seq.TryGet(pos, mem, false));
    EXPECT_EQ(mem.getLengthProperty(), 0);
}

TEST(ReadOnlySequenceTryGetTests, PositionAtStart_StillReturnsTheWholeRemainder) {
    ReadOnlySequence<int> seq(std::vector<int>{10, 20, 30});
    System::SequencePosition pos = seq.getStartProperty();
    System::ReadOnlyMemory<int> mem;
    ASSERT_TRUE(seq.TryGet(pos, mem, false));
    ASSERT_EQ(mem.getLengthProperty(), 3);
    EXPECT_EQ(mem.getSpanProperty()[0], 10);
}

TEST(ReadOnlySequenceTryGetTests, SliceStartPosition_ReturnsOnlyTheSliceContents) {
    ReadOnlySequence<int> seq(std::vector<int>{10, 20, 30});
    auto sliced = seq.Slice(seq.GetPosition(1), seq.getEndProperty());
    System::SequencePosition pos = sliced.getStartProperty();
    System::ReadOnlyMemory<int> mem;
    ASSERT_TRUE(sliced.TryGet(pos, mem, false));
    ASSERT_EQ(mem.getLengthProperty(), 2);
    EXPECT_EQ(mem.getSpanProperty()[0], 20);
    EXPECT_EQ(mem.getSpanProperty()[1], 30);
}

TEST(ReadOnlySequenceTryGetTests, AdvanceStillLandsOnEndAndThenReturnsFalse) {
    ReadOnlySequence<int> seq(std::vector<int>{1, 2});
    System::SequencePosition pos = seq.getStartProperty();
    System::ReadOnlyMemory<int> mem;
    ASSERT_TRUE(seq.TryGet(pos, mem, true));
    EXPECT_EQ(mem.getLengthProperty(), 2);
    EXPECT_EQ(pos.GetInteger(), seq.getEndProperty().GetInteger());
    EXPECT_FALSE(seq.TryGet(pos, mem, true));
    EXPECT_EQ(mem.getLengthProperty(), 0);
}

TEST(ReadOnlySequenceTryGetTests, EmptySequenceAtItsOwnStart_ReturnsFalseWithoutThrowing) {
    ReadOnlySequence<int> seq;
    System::SequencePosition pos = seq.getStartProperty();
    System::ReadOnlyMemory<int> mem;
    EXPECT_FALSE(seq.TryGet(pos, mem, false));
    EXPECT_EQ(mem.getLengthProperty(), 0);
}

// ===========================================================================
// #2051 — ArrayBufferWriter growth: no signed overflow, and the declared exception
//
// `currentLength + growBy` used to be computed in intcs. A one-element writer and
// GetSpan(INTCS_MAX) reached UBSan "signed integer overflow: 1 + 2147483647" and
// then escaped a native std::length_error.
// ===========================================================================

TEST(ArrayBufferWriterGrowthTests, HugeSizeHint_ThrowsOutOfMemoryNotLengthError) {
    ArrayBufferWriter<char> w(1);
    EXPECT_THROW((void)w.GetSpan(INTCS_MAX), System::OutOfMemoryException);
}

TEST(ArrayBufferWriterGrowthTests, HugeSizeHint_LeavesTheWriterUntouched) {
    ArrayBufferWriter<char> w(4);
    auto span = w.GetSpan(4);
    span[0] = 'a';
    span[1] = 'b';
    w.Advance(2);
    const intcs capacityBefore = w.getCapacityProperty();
    const intcs writtenBefore  = w.getWrittenCountProperty();

    EXPECT_THROW((void)w.GetSpan(INTCS_MAX), System::OutOfMemoryException);

    EXPECT_EQ(w.getCapacityProperty(), capacityBefore);
    EXPECT_EQ(w.getWrittenCountProperty(), writtenBefore);
    ASSERT_EQ(w.getWrittenSpanProperty().getLengthProperty(), 2);
    EXPECT_EQ(w.getWrittenSpanProperty()[0], 'a');
    EXPECT_EQ(w.getWrittenSpanProperty()[1], 'b');
}

TEST(ArrayBufferWriterGrowthTests, HugeSizeHintOnGetMemory_ThrowsOutOfMemoryToo) {
    ArrayBufferWriter<char> w(1);
    EXPECT_THROW((void)w.GetMemory(INTCS_MAX), System::OutOfMemoryException);
}

TEST(ArrayBufferWriterGrowthTests, HugeSizeHintOnAnEmptyWriter_ThrowsOutOfMemory) {
    ArrayBufferWriter<char> w;
    EXPECT_THROW((void)w.GetSpan(INTCS_MAX), System::OutOfMemoryException);
    EXPECT_EQ(w.getCapacityProperty(), 0);
}

TEST(ArrayBufferWriterGrowthTests, NegativeSizeHint_StillArgumentException) {
    ArrayBufferWriter<char> w(4);
    EXPECT_THROW((void)w.GetSpan(-1), System::ArgumentException);
    EXPECT_THROW((void)w.GetSpan(INTCS_MIN), System::ArgumentException);
}

TEST(ArrayBufferWriterGrowthTests, OrdinaryGrowthShapeIsUnchanged) {
    ArrayBufferWriter<char> w;
    EXPECT_EQ(w.getCapacityProperty(), 0);
    (void)w.GetSpan(1);
    EXPECT_EQ(w.getCapacityProperty(), ArrayBufferWriter<char>::DefaultInitialBufferSize);
    w.Advance(w.getCapacityProperty());
    (void)w.GetSpan(1);
    EXPECT_EQ(w.getCapacityProperty(), 2 * ArrayBufferWriter<char>::DefaultInitialBufferSize);
}

TEST(ArrayBufferWriterGrowthTests, AdvancePastCapacity_StillInvalidOperation) {
    ArrayBufferWriter<char> w(4);
    EXPECT_THROW(w.Advance(5), System::InvalidOperationException);
}
