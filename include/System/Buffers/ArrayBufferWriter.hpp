// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <vector>
#include "System/Buffers/IBufferWriter.hpp"
#include "System/ReadOnlyMemory.hpp"

namespace System::Buffers {

    /**
     * @brief Represents a heap-based, array-backed output sink into which T data can be written.
     *
     * C++ counterpart of .NET System.Buffers.ArrayBufferWriter&lt;T&gt;.
     * Implements IBufferWriter&lt;T&gt; using an internal std::vector&lt;T&gt; that grows as needed.
     *
     * @tparam T The type of items in the ArrayBufferWriter.
     */
    template<typename T>
    class ArrayBufferWriter : public IBufferWriter<T> {
        std::vector<T> buffer_;
        int writtenCount_ = 0;

    public:
        /** @brief Default initial capacity used when none is specified. */
        static constexpr int DefaultInitialBufferSize = 256;

        /** @brief Constructs an ArrayBufferWriter with the default initial capacity. */
        ArrayBufferWriter() : buffer_(DefaultInitialBufferSize) {}

        /**
         * @brief Constructs an ArrayBufferWriter with the specified initial capacity.
         * @param initialCapacity The minimum initial capacity of the backing buffer.
         * @throws std::invalid_argument if initialCapacity is zero or negative.
         */
        explicit ArrayBufferWriter(int initialCapacity) {
            if (initialCapacity <= 0)
                throw std::invalid_argument("initialCapacity must be positive");
            buffer_.resize(static_cast<std::size_t>(initialCapacity));
        }

        /**
         * @brief Returns a ReadOnlyMemory view over the data written so far.
         * @return Read-only view of the written portion of the buffer.
         */
        [[nodiscard]] System::ReadOnlyMemory<T> getWrittenMemoryProperty() const noexcept {
            return System::ReadOnlyMemory<T>(buffer_.data(), writtenCount_);
        }

        /**
         * @brief Returns the total number of elements written to the buffer.
         * @return Number of elements written.
         */
        [[nodiscard]] int getWrittenCountProperty() const noexcept { return writtenCount_; }

        /**
         * @brief Returns the total capacity of the backing buffer.
         * @return Total buffer capacity.
         */
        [[nodiscard]] int getCapacityProperty() const noexcept {
            return static_cast<int>(buffer_.size());
        }

        /**
         * @brief Returns the remaining space in the buffer (capacity minus written count).
         * @return Number of elements that can still be written without reallocation.
         */
        [[nodiscard]] int getFreeCapacityProperty() const noexcept {
            return getCapacityProperty() - writtenCount_;
        }

        /** @brief Resets the written count to zero, making the full buffer available again. */
        void Clear() noexcept { writtenCount_ = 0; }

        /**
         * @brief Notifies the writer that @p count elements have been written.
         * @param count Number of elements written to the span returned by GetSpan().
         * @throws std::invalid_argument if count is negative or exceeds free capacity.
         */
        void Advance(int count) override {
            if (count < 0 || count > getFreeCapacityProperty())
                throw std::invalid_argument("count out of range");
            writtenCount_ += count;
        }

        /**
         * @brief Returns a writable Span with at least @p sizeHint free elements.
         * @param sizeHint Minimum elements requested (0 means at least 1).
         * @return Writable Span over the free portion of the buffer.
         */
        System::Span<T> GetSpan(int sizeHint = 0) override {
            if (sizeHint < 0) throw std::invalid_argument("sizeHint must be non-negative");
            int needed = sizeHint == 0 ? 1 : sizeHint;
            if (getFreeCapacityProperty() < needed) {
                int newSize = static_cast<int>(buffer_.size()) * 2;
                if (newSize - writtenCount_ < needed)
                    newSize = writtenCount_ + needed;
                buffer_.resize(static_cast<std::size_t>(newSize));
            }
            return System::Span<T>(buffer_.data() + writtenCount_, getFreeCapacityProperty());
        }
    };

} // namespace System::Buffers
