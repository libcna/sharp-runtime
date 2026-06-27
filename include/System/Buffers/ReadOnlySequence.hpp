// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <stdexcept>
#include <vector>
#include "System/SequencePosition.hpp"
#include "System/ReadOnlyMemory.hpp"
#include "System/Span.hpp"

namespace System::Buffers {

    /**
     * @brief Represents a sequence of memory regions that may be non-contiguous in memory.
     *
     * C++ counterpart of .NET System.Buffers.ReadOnlySequence&lt;T&gt;.
     * This implementation wraps a single contiguous segment backed by a std::vector&lt;T&gt;.
     * SequencePosition values encode byte offsets into that segment.
     *
     * @tparam T The type of items in the ReadOnlySequence.
     */
    template<typename T>
    class ReadOnlySequence {
        std::vector<T> data_;
        int start_ = 0;
        int end_   = 0;

    public:
        /** @brief Constructs an empty ReadOnlySequence. */
        ReadOnlySequence() = default;

        /**
         * @brief Constructs a ReadOnlySequence from a vector (takes ownership).
         * @param data Source data vector.
         */
        explicit ReadOnlySequence(std::vector<T> data)
            : data_(std::move(data)), start_(0), end_(static_cast<int>(data_.size())) {}

        /**
         * @brief Constructs a ReadOnlySequence from a pointer and length.
         * @param ptr    Pointer to the first element.
         * @param length Number of elements.
         */
        ReadOnlySequence(const T* ptr, int length)
            : data_(ptr, ptr + length), start_(0), end_(length) {}

        /**
         * @brief Returns the SequencePosition representing the start of the sequence.
         * @return Start position.
         */
        [[nodiscard]] System::SequencePosition getStartProperty() const noexcept {
            return System::SequencePosition(nullptr, start_);
        }

        /**
         * @brief Returns the SequencePosition representing the end of the sequence.
         * @return End position.
         */
        [[nodiscard]] System::SequencePosition getEndProperty() const noexcept {
            return System::SequencePosition(nullptr, end_);
        }

        /**
         * @brief Returns the number of elements in the sequence.
         * @return Sequence length.
         */
        [[nodiscard]] long long getLengthProperty() const noexcept {
            return static_cast<long long>(end_ - start_);
        }

        /**
         * @brief Returns true if the sequence contains no elements.
         * @return true if empty.
         */
        [[nodiscard]] bool getIsEmptyProperty() const noexcept {
            return end_ <= start_;
        }

        /**
         * @brief Returns a read-only memory view over the sequence data.
         * @return ReadOnlyMemory&lt;T&gt; spanning the current start–end range.
         */
        [[nodiscard]] System::ReadOnlyMemory<T> First() const noexcept {
            if (getIsEmptyProperty()) return System::ReadOnlyMemory<T>();
            return System::ReadOnlyMemory<T>(data_.data() + start_, end_ - start_);
        }

        /**
         * @brief Returns a sub-sequence starting at @p start and ending at @p end.
         * @param start  Starting SequencePosition.
         * @param end    Ending SequencePosition.
         * @return A new ReadOnlySequence&lt;T&gt; covering the specified range.
         */
        [[nodiscard]] ReadOnlySequence<T> Slice(System::SequencePosition start,
                                                 System::SequencePosition end) const {
            int s = start.GetInteger();
            int e = end.GetInteger();
            if (s < start_ || e > end_ || s > e)
                throw std::out_of_range("ReadOnlySequence::Slice out of range");
            ReadOnlySequence<T> result;
            result.data_  = data_;
            result.start_ = s;
            result.end_   = e;
            return result;
        }

        /**
         * @brief Returns a sub-sequence starting at @p start to the sequence end.
         * @param start Starting SequencePosition.
         * @return A new ReadOnlySequence&lt;T&gt; from start to end.
         */
        [[nodiscard]] ReadOnlySequence<T> Slice(System::SequencePosition start) const {
            return Slice(start, getEndProperty());
        }

        /**
         * @brief Returns the SequencePosition at the given offset from the start.
         * @param offset Byte offset from the beginning of the sequence.
         * @return SequencePosition at that offset.
         */
        [[nodiscard]] System::SequencePosition GetPosition(long long offset) const {
            int pos = start_ + static_cast<int>(offset);
            if (pos < start_ || pos > end_)
                throw std::out_of_range("ReadOnlySequence::GetPosition out of range");
            return System::SequencePosition(nullptr, pos);
        }

        /**
         * @brief Returns true if the sequence is backed by a single contiguous segment.
         *
         * C++ counterpart of .NET ReadOnlySequence&lt;T&gt;.IsSingleSegment.
         * This stub implementation always returns true because ReadOnlySequence here
         * wraps a single contiguous vector.
         */
        [[nodiscard]] bool getIsSingleSegmentProperty() const noexcept { return true; }

        /**
         * @brief Copies all elements of the sequence into a std::vector.
         *
         * C++ counterpart of .NET ReadOnlySequence&lt;T&gt;.ToArray().
         */
        [[nodiscard]] std::vector<T> ToArray() const {
            return std::vector<T>(data_.begin() + start_, data_.begin() + end_);
        }

        /**
         * @brief Copies the sequence into @p destination.
         *
         * C++ counterpart of .NET ReadOnlySequence&lt;T&gt;.CopyTo(Span&lt;T&gt;).
         * @throws std::out_of_range if destination is too small.
         */
        void CopyTo(System::Span<T> destination) const {
            int len = end_ - start_;
            if (destination.getLengthProperty() < len)
                throw std::out_of_range("ReadOnlySequence::CopyTo destination too small");
            std::copy(data_.begin() + start_, data_.begin() + end_,
                      destination.getPointer());
        }

        // -----------------------------------------------------------------------
        // Enumerator
        // -----------------------------------------------------------------------

        /**
         * @brief Enumerates the ReadOnlyMemory&lt;T&gt; segments of a ReadOnlySequence&lt;T&gt;.
         *
         * C++ counterpart of .NET System.Buffers.ReadOnlySequence&lt;T&gt;.Enumerator.
         * Because this implementation is always a single contiguous segment,
         * the enumerator yields exactly one ReadOnlyMemory view.
         */
        struct Enumerator {
        private:
            const ReadOnlySequence<T>* seq_;
            int step_ = -1;  // -1: before first, 0: on first (and only), 1: done

        public:
            /**
             * @brief Constructs an Enumerator for the specified sequence.
             * @param sequence The sequence to enumerate.
             */
            explicit Enumerator(const ReadOnlySequence<T>& sequence)
                : seq_(&sequence) {}

            /**
             * @brief Gets the current ReadOnlyMemory segment.
             *
             * C++ counterpart of .NET ReadOnlySequence&lt;T&gt;.Enumerator.Current.
             */
            [[nodiscard]] System::ReadOnlyMemory<T> getCurrent() const {
                return seq_->First();
            }

            /**
             * @brief Advances the enumerator to the next segment.
             *
             * C++ counterpart of .NET ReadOnlySequence&lt;T&gt;.Enumerator.MoveNext().
             * @return true on the first call (the single segment); false thereafter.
             */
            bool MoveNext() {
                ++step_;
                return step_ == 0;
            }
        };

        /**
         * @brief Returns an Enumerator for iterating the sequence segments.
         *
         * C++ counterpart of .NET ReadOnlySequence&lt;T&gt;.GetEnumerator().
         */
        [[nodiscard]] Enumerator GetEnumerator() const { return Enumerator(*this); }
    };

} // namespace System::Buffers
