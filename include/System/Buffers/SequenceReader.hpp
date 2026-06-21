// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Buffers/ReadOnlySequence.hpp"

namespace System::Buffers {

    /**
     * @brief Provides forward-only, low-allocation reading of a ReadOnlySequence&lt;T&gt;.
     *
     * C++ counterpart of .NET System.Buffers.SequenceReader&lt;T&gt;.
     * Tracks a current read position within the sequence. Callers advance through
     * elements using TryRead(), IsNext(), Advance(), and related helpers.
     *
     * @tparam T The type of items in the underlying sequence.
     */
    template<typename T>
    class SequenceReader {
        const ReadOnlySequence<T>& sequence_;
        System::ReadOnlyMemory<T>  segment_;
        int consumed_ = 0;   // absolute offset from sequence start

        [[nodiscard]] int total() const noexcept {
            return static_cast<int>(sequence_.getLengthProperty());
        }

    public:
        /**
         * @brief Constructs a SequenceReader over the given sequence.
         * @param sequence The sequence to read from. Must outlive this reader.
         */
        explicit SequenceReader(const ReadOnlySequence<T>& sequence)
            : sequence_(sequence), segment_(sequence.First()), consumed_(0) {}

        /**
         * @brief Returns the number of elements consumed so far.
         * @return Consumed element count.
         */
        [[nodiscard]] long long getConsumedProperty() const noexcept {
            return static_cast<long long>(consumed_);
        }

        /**
         * @brief Returns the number of elements remaining to be read.
         * @return Remaining element count.
         */
        [[nodiscard]] long long getRemainingProperty() const noexcept {
            return static_cast<long long>(total() - consumed_);
        }

        /**
         * @brief Returns true if all elements have been consumed.
         * @return true if at end of sequence.
         */
        [[nodiscard]] bool getEndProperty() const noexcept {
            return consumed_ >= total();
        }

        /**
         * @brief Returns the current SequencePosition.
         * @return Current position within the underlying sequence.
         */
        [[nodiscard]] System::SequencePosition getPositionProperty() const noexcept {
            return sequence_.GetPosition(static_cast<long long>(consumed_));
        }

        /**
         * @brief Tries to read the next element from the sequence.
         * @param value Receives the next element if successful.
         * @return true if an element was read; false if at end of sequence.
         */
        bool TryRead(T& value) {
            if (getEndProperty()) return false;
            value = segment_[consumed_];
            ++consumed_;
            return true;
        }

        /**
         * @brief Returns true if the next element equals @p next without advancing.
         * @param next The value to compare against the next element.
         * @return true if the next element equals @p next.
         */
        [[nodiscard]] bool IsNext(const T& next) const {
            if (getEndProperty()) return false;
            return segment_[consumed_] == next;
        }

        /**
         * @brief Advances the reader by @p count elements.
         * @param count Number of elements to skip.
         * @throws std::out_of_range if count exceeds remaining elements.
         */
        void Advance(long long count) {
            if (count < 0 || count > getRemainingProperty())
                throw std::out_of_range("SequenceReader::Advance out of range");
            consumed_ += static_cast<int>(count);
        }

        /**
         * @brief Tries to advance past @p next if it is the next element.
         * @param next The element to match and skip.
         * @return true if the element was matched and skipped; false otherwise.
         */
        bool TryAdvancePast(const T& next) {
            if (!IsNext(next)) return false;
            ++consumed_;
            return true;
        }

        /** @brief Rewinds the reader to the beginning of the sequence. */
        void Rewind() noexcept { consumed_ = 0; }
    };

} // namespace System::Buffers
