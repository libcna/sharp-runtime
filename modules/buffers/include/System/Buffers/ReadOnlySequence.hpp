// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/SequencePosition.hpp"
#include "System/ReadOnlyMemory.hpp"
#include "System/Span.hpp"

namespace System::Buffers {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

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
        intcs start_ = 0;
        intcs end_   = 0;

        /**
         * @brief Copies @p length elements from @p ptr after validating the metadata.
         *
         * The raw pointer/length constructor used to run `data_(ptr, ptr + length)` straight
         * from its member-initialiser list, so a caller's numbers reached the vector's range
         * constructor before anything could reject them: `(nullptr, 1)` reported a UBSan
         * "load of null pointer of type 'const T'" and then an ASan `SEGV` inside the
         * constructor, and `(ptr, -1)` escaped a native `std::length_error` — not a
         * `System::` exception — through a public door. Validation cannot live in the
         * constructor body because the member is already built by then, so it lives here and
         * the constructor initialises `data_` from the result.
         *
         * `(nullptr, 0)` stays valid and yields the empty sequence: adding 0 to a null
         * pointer is well defined, two pre-existing tests rely on it, and an empty sequence
         * with no buffer is a meaningful value.
         *
         * Ticket #2049 / SR-AUD-072 / CCF-005; see docs/BuffersNamespaceReviewPlan.md §4.1.
         */
        static std::vector<T> validatedCopy(const T* ptr, intcs length) {
            if (length < 0)
                throw System::ArgumentOutOfRangeException("length");
            if (ptr == nullptr && length > 0)
                throw System::ArgumentNullException("ptr");
            return std::vector<T>(ptr, ptr + length);
        }

    public:
        /** @brief Constructs an empty ReadOnlySequence. */
        ReadOnlySequence() = default;

        /**
         * @brief Constructs a ReadOnlySequence from a vector (takes ownership).
         * @param data Source data vector.
         */
        explicit ReadOnlySequence(std::vector<T> data)
            : data_(std::move(data)), start_(0), end_(static_cast<intcs>(data_.size())) {}

        /**
         * @brief Constructs a ReadOnlySequence by copying @p length elements from @p ptr.
         *
         * The elements are copied into this sequence's own storage, so @p ptr need not
         * outlive the sequence.
         *
         * @param ptr    Pointer to the first element. May be null only when @p length is 0.
         * @param length Number of elements. Must not be negative.
         * @throws System::ArgumentNullException if @p ptr is null and @p length is positive.
         * @throws System::ArgumentOutOfRangeException if @p length is negative.
         */
        ReadOnlySequence(const T* ptr, intcs length)
            : data_(validatedCopy(ptr, length)), start_(0),
              end_(static_cast<intcs>(data_.size())) {}

        /**
         * @brief Returns a shared empty ReadOnlySequence.
         * C++ counterpart of .NET ReadOnlySequence&lt;T&gt;.Empty.
         */
        [[nodiscard]] static ReadOnlySequence<T> getEmpty() { return ReadOnlySequence<T>(); }

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
        [[nodiscard]] longcs getLengthProperty() const noexcept {
            return static_cast<longcs>(end_ - start_);
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
            intcs s = start.GetInteger();
            intcs e = end.GetInteger();
            if (s < start_ || e > end_ || s > e)
                throw System::ArgumentOutOfRangeException("start");
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
         * @brief Returns a sub-sequence of @p length elements starting at @p start.
         *
         * C++ counterpart of both .NET ReadOnlySequence&lt;T&gt;.Slice(long, long) and
         * Slice(int, int) — merged into a single longcs-based overload, since intcs
         * arguments widen to longcs unambiguously (unlike .NET, C++ overload
         * resolution has no inherent preference between narrowing int64->int32 and
         * same-rank long long->long conversions, so keeping both would make calls
         * with e.g. a `long long` literal ambiguous).
         */
        [[nodiscard]] ReadOnlySequence<T> Slice(longcs start, longcs length) const {
            // Computing `start + length` directly (both attacker/caller-controlled longcs
            // values) can overflow before either is ever validated -- confirmed via UBSan.
            // Resolve `start` to a position first, then let the (now overflow-safe)
            // GetPosition(offset, origin) combine `length` with it.
            System::SequencePosition begin = GetPosition(start);
            return Slice(begin, GetPosition(length, begin));
        }

        /**
         * @brief Returns a sub-sequence starting at @p start to the sequence end.
         * C++ counterpart of .NET ReadOnlySequence&lt;T&gt;.Slice(long).
         */
        [[nodiscard]] ReadOnlySequence<T> Slice(longcs start) const {
            return Slice(GetPosition(start), getEndProperty());
        }

        /**
         * @brief Returns a sub-sequence from @p start to @p end.
         * C++ counterpart of .NET ReadOnlySequence&lt;T&gt;.Slice(long, SequencePosition).
         */
        [[nodiscard]] ReadOnlySequence<T> Slice(longcs start, System::SequencePosition end) const {
            return Slice(GetPosition(start), end);
        }

        /**
         * @brief Returns a sub-sequence of @p length elements starting at @p start.
         * C++ counterpart of .NET ReadOnlySequence&lt;T&gt;.Slice(SequencePosition, long).
         */
        [[nodiscard]] ReadOnlySequence<T> Slice(System::SequencePosition start, longcs length) const {
            return Slice(start, GetPosition(length, start));
        }

        /**
         * @brief Returns the SequencePosition at the given offset from the start.
         * @param offset Offset from the beginning of the sequence.
         * @return SequencePosition at that offset.
         * @throws ArgumentOutOfRangeException if offset is negative.
         */
        [[nodiscard]] System::SequencePosition GetPosition(longcs offset) const {
            return GetPosition(offset, getStartProperty());
        }

        /**
         * @brief Returns a new SequencePosition at @p offset from @p origin.
         * C++ counterpart of .NET ReadOnlySequence&lt;T&gt;.GetPosition(long, SequencePosition).
         * @throws ArgumentOutOfRangeException if offset is negative.
         */
        [[nodiscard]] System::SequencePosition GetPosition(longcs offset, System::SequencePosition origin) const {
            if (offset < 0)
                throw ArgumentOutOfRangeException("offset");
            // Do the whole computation in longcs and check via subtraction before ever adding
            // offset in, rather than narrowing offset to intcs first (which could silently
            // truncate a huge, clearly-out-of-range offset down to a value that happens to
            // land inside [start_, end_]) or computing origin + offset directly (which can
            // overflow longcs itself for an offset near LONGCS_MAX) -- both confirmed as real
            // bugs via a standalone repro before this fix (a ~4-billion-offset silently
            // "succeeded" with a bogus position, and a huge start+length in Slice below hit
            // genuine UBSan-flagged signed overflow).
            longcs originInt = static_cast<longcs>(origin.GetInteger());
            if (originInt < static_cast<longcs>(start_) || originInt > static_cast<longcs>(end_) ||
                offset > static_cast<longcs>(end_) - originInt)
                throw ArgumentOutOfRangeException("offset");
            return System::SequencePosition(nullptr, static_cast<intcs>(originInt + offset));
        }

        /**
         * @brief Tries to retrieve the segment at/after @p position.
         *
         * C++ counterpart of .NET ReadOnlySequence&lt;T&gt;.TryGet(ref SequencePosition, out ReadOnlyMemory&lt;T&gt;, bool).
         * Because this implementation is always a single contiguous segment, this
         * returns the remaining data once (from @p position to the end) and then false.
         * @param position Position to start from; advanced to the end if @p advance is true.
         *        Must lie within this sequence's own range — a position obtained from a
         *        different sequence, or from this sequence before it was sliced, is rejected.
         * @param memory   Receives the segment's data, or an empty view if there is none.
         * @param advance  If true, advances @p position past the returned segment.
         * @return true if data was returned; false if @p position was already at the end.
         * @throws System::ArgumentOutOfRangeException if @p position lies outside
         *         [getStartProperty(), getEndProperty()].
         */
        [[nodiscard]] bool TryGet(System::SequencePosition& position, System::ReadOnlyMemory<T>& memory,
                                  bool advance = true) const {
            intcs pos = position.GetInteger();
            // Only `pos >= end_` used to be treated as invalid, so a position *below* start_
            // reached the view arithmetic untouched. Two measured consequences, neither of
            // which needs a hand-forged SequencePosition: `getStartProperty()` of an unsliced
            // sequence, held across a Slice and passed to the slice's TryGet, returned a view
            // covering elements the slice does not contain; and a negative integer produced
            // `data_.data() + pos`, whose first element is an ASan-confirmed
            // heap-buffer-overflow READ before the allocation. Validating the whole range —
            // rather than only the segment marker, which this single-segment adaptation does
            // not use — closes both. `pos == end_` keeps its pinned end-of-sequence result.
            //
            // Ticket #2050 / SR-AUD-073 / CCF-005; see docs/BuffersNamespaceReviewPlan.md §4.2.
            if (pos < start_ || pos > end_)
                throw System::ArgumentOutOfRangeException("position");
            if (pos >= end_) {
                memory = System::ReadOnlyMemory<T>();
                return false;
            }
            memory = System::ReadOnlyMemory<T>(data_.data() + pos, end_ - pos);
            if (advance) position = getEndProperty();
            return true;
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
         * @throws System::ArgumentOutOfRangeException if destination is too small.
         */
        void CopyTo(System::Span<T> destination) const {
            intcs len = end_ - start_;
            if (destination.getLengthProperty() < len)
                throw System::ArgumentOutOfRangeException("destination");
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
            [[nodiscard]] System::ReadOnlyMemory<T> getCurrentProperty() const {
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
