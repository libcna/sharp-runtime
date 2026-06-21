// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <stdexcept>
#include <vector>
#include "System/SequencePosition.hpp"
#include "System/ReadOnlyMemory.hpp"

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
    };

} // namespace System::Buffers
