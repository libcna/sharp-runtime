// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/ReadOnlyMemory.hpp"

namespace System::Buffers {

/**
 * @brief Represents a node in a linked list of ReadOnlyMemory&lt;T&gt; segments.
 *
 * C++ counterpart of .NET System.Buffers.ReadOnlySequenceSegment&lt;T&gt;.
 * Subclasses fill in the protected fields via the set-property helpers.
 *
 * @warning **A chain of these segments cannot currently be turned into a
 * ReadOnlySequence&lt;T&gt;.** In .NET that is this type's entire reason for existing, and
 * this header used to claim the same; it is not true here. `ReadOnlySequence<T>` offers
 * only default, `std::vector` and raw pointer/length construction, is backed by a single
 * contiguous vector, has no multi-segment state, and its
 * `getIsSingleSegmentProperty()` is a hard-coded `true`. Linking nodes therefore builds a
 * list nothing consumes (SR-AUD-087).
 *
 * The node shape is retained so that ported C# code declaring segment types still
 * compiles. Implementing the segment-chain constructor is blocked ticket **#2058**: it
 * needs new public members on `ReadOnlySequence<T>` (an object-layout change) plus
 * multi-segment rewrites of `First`, all six `Slice` overloads, both `GetPosition`
 * overloads, `TryGet`, `ToArray`, `CopyTo` and the enumerator, and of
 * `SequenceReader<T>`'s single-segment snapshot. See
 * docs/BuffersNamespaceReviewPlan.md §4.8. The current limitation is pinned by a
 * permanent test.
 *
 * @tparam T The element type.
 */
template<typename T>
class ReadOnlySequenceSegment {
protected:
    System::ReadOnlyMemory<T>     memory_;
    ReadOnlySequenceSegment<T>*   next_         = nullptr;
    long long                     runningIndex_ = 0;

public:
    virtual ~ReadOnlySequenceSegment() = default;

    /**
     * @brief Gets the memory for the current node.
     * C++ counterpart of .NET ReadOnlySequenceSegment&lt;T&gt;.Memory.
     */
    [[nodiscard]] const System::ReadOnlyMemory<T>& getMemoryProperty() const noexcept {
        return memory_;
    }

    /**
     * @brief Gets the next node in the linked list, or nullptr if this is the last.
     * C++ counterpart of .NET ReadOnlySequenceSegment&lt;T&gt;.Next.
     */
    [[nodiscard]] ReadOnlySequenceSegment<T>* getNextProperty() const noexcept {
        return next_;
    }

    /**
     * @brief Gets the sum of node lengths before this node (its start offset in the sequence).
     * C++ counterpart of .NET ReadOnlySequenceSegment&lt;T&gt;.RunningIndex.
     */
    [[nodiscard]] long long getRunningIndexProperty() const noexcept {
        return runningIndex_;
    }

protected:
    /** @brief Sets the memory for this segment. */
    void setMemoryProperty(System::ReadOnlyMemory<T> memory) { memory_ = std::move(memory); }
    /** @brief Sets the next segment. */
    void setNextProperty(ReadOnlySequenceSegment<T>* next) { next_ = next; }
    /** @brief Sets the running index. */
    void setRunningIndexProperty(long long index) { runningIndex_ = index; }
};

} // namespace System::Buffers
