// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <type_traits>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/Buffers/IBufferWriter.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Memory.hpp"
#include "System/OutOfMemoryException.hpp"
#include "System/ReadOnlyMemory.hpp"
#include "System/Span.hpp"

namespace System::Buffers {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    /**
     * @brief Represents a heap-based, array-backed output sink into which T data can be written.
     *
     * C++ counterpart of .NET System.Buffers.ArrayBufferWriter&lt;T&gt;.
     * Implements IBufferWriter&lt;T&gt; using an internal std::vector&lt;T&gt; that grows as needed.
     *
     * @tparam T The type of items in the ArrayBufferWriter.
     *
     * @par Requirements on T
     * .NET places no constraint on `ArrayBufferWriter<T>`'s `T`, but this port is built on
     * `std::vector<T>`, which constrains it.
     * - **Default-constructible** — `ArrayBufferWriter(intcs)`, `GetSpan()` and `GetMemory()`
     *   size the backing vector, which value-initializes the new elements. Because
     *   `GetSpan`/`GetMemory` are `virtual` overrides of IBufferWriter&lt;T&gt;, their bodies
     *   are instantiated for the vtable, so this requirement bites when an
     *   `ArrayBufferWriter<T>` object is **constructed** — including by the default
     *   constructor — not only when a span is asked for.
     * - **Copy-assignable**, in addition — `Clear()` assigns a value-initialized `T` over
     *   every written element. `ResetWrittenCount()` requires neither.
     *
     * Measured: naming the type (`ArrayBufferWriter<T>*`, a typedef) and implicitly
     * instantiating it (`sizeof`) stay legal for **any** `T`, before and after this
     * disclosure, which is why the assertions are in member bodies and not at class scope.
     *
     * Each requirement is `static_assert`ed in the member that already enforces it, so the
     * same set of programs compiles as before and only the diagnostic changes.
     * Ticket #2054 / SR-AUD-070, family B-C; see docs/BuffersNamespaceReviewPlan.md §4.4.
     */
    template<typename T>
    class ArrayBufferWriter : public IBufferWriter<T> {
        std::vector<T> buffer_;
        intcs writtenCount_ = 0;

        /**
         * @brief The largest backing buffer this writer will allocate.
         *
         * Matches .NET's private `ArrayBufferWriter<T>.MaxArrayLength`, which is
         * `Array.MaxLength`. Private and `static constexpr`, so it is neither public
         * surface nor part of the object layout.
         */
        static constexpr intcs MaxArrayLength = 0x7FFFFFC7;

        void checkAndResizeBuffer(intcs sizeHint) {
            static_assert(
                std::is_default_constructible_v<T>,
                "System::Buffers::ArrayBufferWriter<T>::GetSpan/GetMemory require T to be "
                "default-constructible: growing the backing std::vector<T> value-initializes "
                "the new elements. See docs/BuffersNamespaceReviewPlan.md, ticket #2054.");
            if (sizeHint < 0)
                throw System::ArgumentException("sizeHint must be non-negative", "sizeHint");
            if (sizeHint == 0) sizeHint = 1;
            if (sizeHint > getFreeCapacityProperty()) {
                intcs currentLength = static_cast<intcs>(buffer_.size());
                // Matches .NET: grow by the larger of sizeHint and the current length,
                // special-casing an empty buffer to jump straight to DefaultInitialBufferSize.
                intcs growBy = std::max(sizeHint, currentLength);
                if (currentLength == 0) growBy = std::max(growBy, DefaultInitialBufferSize);
                // .NET adds these two in `int` and relies on C#'s DEFINED unchecked wrap,
                // catching the wrap immediately with `if ((uint)newSize > MaxArrayLength)`.
                // Signed overflow is UNDEFINED in C++, so that idiom cannot be ported as
                // written: a one-element writer and GetSpan(INTCS_MAX) reached
                // "signed integer overflow: 1 + 2147483647 cannot be represented in type
                // 'int'" under UBSan and then escaped a native std::length_error instead of
                // a System:: exception. Widening the sum to longcs makes the same check
                // exact while keeping .NET's outcome, and the throw happens before resize so
                // capacity and written count survive the failure unchanged.
                //
                // Ticket #2051 / CCF-004; see docs/BuffersNamespaceReviewPlan.md §5.2.
                longcs newSize = static_cast<longcs>(currentLength) + static_cast<longcs>(growBy);
                if (newSize > static_cast<longcs>(MaxArrayLength)) {
                    // .NET's own fallback: the caller only truly needs what is already
                    // written plus the hint, so clamp to the ceiling unless even that fails.
                    longcs needed = static_cast<longcs>(writtenCount_) + static_cast<longcs>(sizeHint);
                    if (needed > static_cast<longcs>(MaxArrayLength))
                        throw System::OutOfMemoryException(
                            "The requested buffer size exceeds the maximum array length.");
                    newSize = static_cast<longcs>(MaxArrayLength);
                }
                buffer_.resize(static_cast<std::size_t>(newSize));
            }
        }

    public:
        /** @brief Default initial capacity used when growing an empty buffer. */
        static constexpr intcs DefaultInitialBufferSize = 256;

        /**
         * @brief Constructs an ArrayBufferWriter with zero initial capacity.
         * C++ counterpart of .NET ArrayBufferWriter() — the backing buffer starts
         * empty; DefaultInitialBufferSize is only applied on the first growth.
         */
        ArrayBufferWriter() = default;

        /**
         * @brief Constructs an ArrayBufferWriter with the specified initial capacity.
         * @param initialCapacity The minimum initial capacity of the backing buffer.
         * @throws System::ArgumentException if initialCapacity is zero or negative.
         * @note Requires T to be default-constructible; see the class-level
         *       "Requirements on T" note.
         */
        explicit ArrayBufferWriter(intcs initialCapacity) {
            static_assert(
                std::is_default_constructible_v<T>,
                "System::Buffers::ArrayBufferWriter<T>(intcs) requires T to be "
                "default-constructible: sizing the backing std::vector<T> value-initializes "
                "its elements. See docs/BuffersNamespaceReviewPlan.md, ticket #2054.");
            if (initialCapacity <= 0)
                throw System::ArgumentException("initialCapacity must be positive", "initialCapacity");
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
         * @brief Returns a ReadOnlySpan view over the data written so far.
         * @return Read-only view of the written portion of the buffer.
         */
        [[nodiscard]] System::ReadOnlySpan<T> getWrittenSpanProperty() const noexcept {
            return System::ReadOnlySpan<T>(buffer_.data(), writtenCount_);
        }

        /**
         * @brief Returns the total number of elements written to the buffer.
         * @return Number of elements written.
         */
        [[nodiscard]] intcs getWrittenCountProperty() const noexcept { return writtenCount_; }

        /**
         * @brief Returns the total capacity of the backing buffer.
         * @return Total buffer capacity.
         */
        [[nodiscard]] intcs getCapacityProperty() const noexcept {
            return static_cast<intcs>(buffer_.size());
        }

        /**
         * @brief Returns the remaining space in the buffer (capacity minus written count).
         * @return Number of elements that can still be written without reallocation.
         */
        [[nodiscard]] intcs getFreeCapacityProperty() const noexcept {
            return getCapacityProperty() - writtenCount_;
        }

        /**
         * @brief Clears the written data, zeroing the buffer's content, and resets
         * the written count to zero.
         *
         * C++ counterpart of .NET ArrayBufferWriter&lt;T&gt;.Clear(). Slower than
         * ResetWrittenCount() since it also zeroes the buffer; use ResetWrittenCount()
         * if the contents don't need to be cleared.
         *
         * @note Requires T to be default-constructible **and** copy-assignable; see the
         *       class-level "Requirements on T" note. ResetWrittenCount() requires neither.
         */
        void Clear() {
            static_assert(
                std::is_default_constructible_v<T>,
                "System::Buffers::ArrayBufferWriter<T>::Clear() requires T to be "
                "default-constructible: it writes a value-initialized T over every written "
                "element. See docs/BuffersNamespaceReviewPlan.md, ticket #2054.");
            static_assert(
                std::is_copy_assignable_v<T>,
                "System::Buffers::ArrayBufferWriter<T>::Clear() requires T to be "
                "copy-assignable: it assigns a value-initialized T over every written "
                "element. Use ResetWrittenCount() if T cannot be assigned. "
                "See docs/BuffersNamespaceReviewPlan.md, ticket #2054.");
            std::fill(buffer_.begin(), buffer_.begin() + writtenCount_, T{});
            writtenCount_ = 0;
        }

        /**
         * @brief Resets the written count to zero without zeroing the buffer's content.
         * C++ counterpart of .NET ArrayBufferWriter&lt;T&gt;.ResetWrittenCount().
         */
        void ResetWrittenCount() noexcept { writtenCount_ = 0; }

        /**
         * @brief Notifies the writer that @p count elements have been written.
         * @param count Number of elements written to the span returned by GetSpan().
         * @throws System::ArgumentException if count is negative.
         * @throws InvalidOperationException if count would advance past the end of the buffer.
         */
        void Advance(intcs count) override {
            if (count < 0)
                throw System::ArgumentException("count must be non-negative", "count");
            if (writtenCount_ > getCapacityProperty() - count)
                throw InvalidOperationException("Cannot advance past the end of the buffer.");
            writtenCount_ += count;
        }

        /**
         * @brief Returns a writable Memory with at least @p sizeHint free elements.
         * @param sizeHint Minimum elements requested (0 means at least 1).
         * @return Writable Memory over the free portion of the buffer.
         */
        System::Memory<T> GetMemory(intcs sizeHint = 0) override {
            checkAndResizeBuffer(sizeHint);
            return System::Memory<T>(buffer_, writtenCount_, getFreeCapacityProperty());
        }

        /**
         * @brief Returns a writable Span with at least @p sizeHint free elements.
         * @param sizeHint Minimum elements requested (0 means at least 1).
         * @return Writable Span over the free portion of the buffer.
         */
        System::Span<T> GetSpan(intcs sizeHint = 0) override {
            checkAndResizeBuffer(sizeHint);
            return System::Span<T>(buffer_.data() + writtenCount_, getFreeCapacityProperty());
        }
    };

} // namespace System::Buffers
