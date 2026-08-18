// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <functional>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/detail/SpanLength.hpp"
#include "System/detail/OverlapCopy.hpp"

namespace System {

    using SharpRuntime::intcs;

    /**
     * @brief Delimits a section of a one-dimensional array.
     *
     * C++ counterpart of .NET System.ArraySegment<T>.
     * Stores a non-owning pointer into a std::vector<T> together with an
     * offset and a count, providing a view over a contiguous sub-range of
     * the underlying vector.
     *
     * @tparam T The element type of the array.
     */
    template<typename T>
    class ArraySegment {
        std::vector<T>* array_ = nullptr;
        intcs offset_ = 0;
        intcs count_  = 0;

        /**
         * @brief Rejects the default (invalid) segment before any operation that needs a
         * real underlying array.
         *
         * C++ counterpart of .NET ArraySegment&lt;T&gt;.ThrowInvalidOperationIfDefault(),
         * which every .NET member that touches the array calls first, with exactly this
         * message (`SR.InvalidOperation_NullArray`).
         *
         * The default segment deliberately stores a null array pointer, matching .NET's
         * default value. Before ticket #2214 nothing checked it, and the consequences were
         * measured (SR-AUD-054): both `Slice` overloads bound a reference to the null
         * pointer -- UndefinedBehaviorSanitizer reported *reference binding to null pointer*
         * -- then called `size()` through it and reached an AddressSanitizer **SEGV**; and
         * `ToArray`, all three `CopyTo` forms, `Contains` and `IndexOf` completed silently,
         * reporting an empty result instead of an invalid state. Both indexers already threw,
         * but `ArgumentOutOfRangeException` rather than .NET's `InvalidOperationException`,
         * because `count_ == 0` made the range check fire first.
         *
         * @note `begin()`/`end()` ARE guarded since ticket #2215, which dropped their
         * `noexcept` under `docs/StandingApprovals.md` SA-10. They are the port's
         * `GetEnumerator()` counterpart and .NET's `GetEnumerator()` calls this same check
         * (`ArraySegment.cs:95-99`), so a range-`for` over a default segment now throws
         * instead of silently performing zero iterations.
         *
         * @throws System::InvalidOperationException if this is a default segment.
         * @see docs/CoreMemorySafetyFamilyPlan.md (family CMS-B)
         */
        void throwIfDefault() const {
            if (array_ == nullptr)
                throw System::InvalidOperationException("The underlying array is null.");
        }

    public:
        // -----------------------------------------------------------------------
        // Constructors
        // -----------------------------------------------------------------------

        /**
         * @brief Initializes a new default (empty) ArraySegment.
         *
         * C++ counterpart of .NET ArraySegment<T>() default constructor.
         * The array pointer, offset, and count are all zero/null.
         */
        ArraySegment() = default;

        /**
         * @brief Initializes a new ArraySegment that delimits all elements in @p array.
         *
         * C++ counterpart of .NET ArraySegment<T>(T[]).
         * @param array The underlying vector to wrap.
         * @throws System::ArgumentOutOfRangeException if @p array holds more than INT32_MAX elements.
         */
        explicit ArraySegment(std::vector<T>& array)
            : array_(&array), offset_(0), count_(System::detail::checkedSpanLength(array.size(), "array")) {}

        /**
         * @brief Initializes a new ArraySegment that delimits the specified sub-range.
         *
         * C++ counterpart of .NET ArraySegment<T>(T[], int, int).
         * @param array  The underlying vector.
         * @param offset Zero-based index of the first element in the segment.
         * @param count  Number of elements in the segment.
         * @throws System::ArgumentOutOfRangeException if offset or count are negative or exceed the array bounds.
         */
        ArraySegment(std::vector<T>& array, intcs offset, intcs count)
            : array_(&array), offset_(offset), count_(count)
        {
            // offset+count (both intcs/int32) can itself signed-overflow for large offset/count,
            // silently bypassing this check instead of throwing -- confirmed real UB via a
            // standalone UBSan repro on the identical pattern in Span<T>::Slice (ticket 265);
            // fixed the same way real .NET's Span<T>.Slice(int,int) does: unsigned comparison,
            // subtraction instead of addition (sz-offset can't overflow once offset<=sz is known).
            auto sz = static_cast<intcs>(array.size());
            if (static_cast<SharpRuntime::uintcs>(offset) > static_cast<SharpRuntime::uintcs>(sz) ||
                static_cast<SharpRuntime::uintcs>(count) > static_cast<SharpRuntime::uintcs>(sz - offset))
                throw System::ArgumentOutOfRangeException("offset", "offset/count out of range");
        }

        // -----------------------------------------------------------------------
        // Static factory
        // -----------------------------------------------------------------------

        /**
         * @brief Returns an empty ArraySegment backed by a persistent empty vector.
         *
         * C++ counterpart of .NET ArraySegment<T>.Empty.
         */
        [[nodiscard]] static ArraySegment<T> getEmpty()
        {
            static std::vector<T> emptyVec;
            return ArraySegment<T>(emptyVec);
        }

        // -----------------------------------------------------------------------
        // Properties
        // -----------------------------------------------------------------------

        /**
         * @brief Gets a pointer to the underlying vector.
         *
         * C++ counterpart of .NET ArraySegment<T>.Array.
         * Returns nullptr for a default-constructed segment.
         */
        [[nodiscard]] std::vector<T>* getArrayProperty() const noexcept { return array_; }

        /**
         * @brief Gets the zero-based index of the first element in the segment.
         *
         * C++ counterpart of .NET ArraySegment<T>.Offset.
         */
        [[nodiscard]] intcs getOffsetProperty() const noexcept { return offset_; }

        /**
         * @brief Gets the number of elements in the segment.
         *
         * C++ counterpart of .NET ArraySegment<T>.Count.
         */
        [[nodiscard]] intcs getCountProperty() const noexcept { return count_; }

        // -----------------------------------------------------------------------
        // Indexer
        // -----------------------------------------------------------------------

        /**
         * @brief Gets or sets the element at the specified position in the segment.
         *
         * C++ counterpart of .NET ArraySegment<T>[int] setter.
         * @param index Zero-based index within the segment.
         * @throws System::InvalidOperationException if this is a default segment (checked
         *         first, matching .NET).
         * @throws System::ArgumentOutOfRangeException if @p index is outside [0, Count).
         */
        [[nodiscard]] T& operator[](intcs index)
        {
            throwIfDefault();
            if (index < 0 || index >= count_)
                throw System::ArgumentOutOfRangeException("index", "Index was out of range. Must be non-negative and less than the size of the collection.");
            return (*array_)[offset_ + index];
        }

        /**
         * @brief Gets the element at the specified position in the segment (const).
         *
         * C++ counterpart of .NET ArraySegment<T>[int] getter.
         * @param index Zero-based index within the segment.
         * @throws System::InvalidOperationException if this is a default segment (checked
         *         first, matching .NET).
         * @throws System::ArgumentOutOfRangeException if @p index is outside [0, Count).
         */
        [[nodiscard]] const T& operator[](intcs index) const
        {
            throwIfDefault();
            if (index < 0 || index >= count_)
                throw System::ArgumentOutOfRangeException("index", "Index was out of range. Must be non-negative and less than the size of the collection.");
            return (*array_)[offset_ + index];
        }

        // -----------------------------------------------------------------------
        // Range-for / iteration support
        // -----------------------------------------------------------------------

        /**
         * @brief Returns a pointer to the first element of the segment.
         *
         * This is the port's `GetEnumerator()` counterpart, and like every other
         * array-touching member it rejects a default segment. .NET's `GetEnumerator()` calls
         * `ThrowInvalidOperationIfDefault()` first (`ArraySegment.cs:95-99`).
         *
         * **These four are deliberately NOT `noexcept`**, and that is the whole of ticket
         * #2215: they used to be, which is why they were the last unguarded door in the type
         * and why a range-`for` over a default segment silently performed zero iterations.
         * The `noexcept` drop is a public signature change and landed under
         * `docs/StandingApprovals.md` SA-10 with SA-2's five conditions. See
         * docs/Migration-ArraySegmentEnumerationGuard.md.
         *
         * @throws System::InvalidOperationException if this is a default segment.
         */
        T*       begin()       { throwIfDefault(); return array_->data() + offset_; }
        /** @brief Returns a pointer past the last element of the segment.
         *  @throws System::InvalidOperationException if this is a default segment. */
        T*       end()         { throwIfDefault(); return array_->data() + offset_ + count_; }
        /** @brief Returns a const pointer to the first element of the segment.
         *  @throws System::InvalidOperationException if this is a default segment. */
        const T* begin() const { throwIfDefault(); return array_->data() + offset_; }
        /** @brief Returns a const pointer past the last element of the segment.
         *  @throws System::InvalidOperationException if this is a default segment. */
        const T* end()   const { throwIfDefault(); return array_->data() + offset_ + count_; }

        // -----------------------------------------------------------------------
        // Slice
        // -----------------------------------------------------------------------

        /**
         * @brief Forms a slice from the specified position to the end of the segment.
         *
         * C++ counterpart of .NET ArraySegment<T>.Slice(int).
         * @param index The zero-based starting index of the slice within the segment.
         * @throws System::InvalidOperationException if this is a default segment (checked
         *         first, matching .NET; before #2214 this reached a null dereference).
         * @throws System::ArgumentOutOfRangeException if @p index is out of range.
         */
        [[nodiscard]] ArraySegment<T> Slice(intcs index) const
        {
            throwIfDefault();
            if (index < 0 || index > count_)
                throw System::ArgumentOutOfRangeException("index", "ArraySegment::Slice: index out of range");
            return ArraySegment<T>(*array_, offset_ + index, count_ - index);
        }

        /**
         * @brief Forms a slice of the specified length starting at the specified position.
         *
         * C++ counterpart of .NET ArraySegment<T>.Slice(int, int).
         * @param index Zero-based start index within the segment.
         * @param count Number of elements in the slice.
         * @throws System::InvalidOperationException if this is a default segment (checked
         *         first, matching .NET; before #2214 this reached a null dereference).
         * @throws System::ArgumentOutOfRangeException if the parameters are out of range.
         */
        [[nodiscard]] ArraySegment<T> Slice(intcs index, intcs count) const
        {
            throwIfDefault();
            // See the (vector&, offset, count) constructor above: same overflow-bypasses-the-
            // check bug, same fix.
            if (static_cast<SharpRuntime::uintcs>(index) > static_cast<SharpRuntime::uintcs>(count_) ||
                static_cast<SharpRuntime::uintcs>(count) > static_cast<SharpRuntime::uintcs>(count_ - index))
                throw System::ArgumentOutOfRangeException("index", "ArraySegment::Slice: out of range");
            return ArraySegment<T>(*array_, offset_ + index, count);
        }

        // -----------------------------------------------------------------------
        // CopyTo
        // -----------------------------------------------------------------------

        /**
         * @brief Copies the segment's elements into a destination vector (starting at index 0).
         *
         * C++ counterpart of .NET ArraySegment<T>.CopyTo(T[]).
         *
         * @note Corrected by ticket #2329: this comment used to require the destination to
         * "have capacity for at least Count elements" and describe the elements as "written
         * via push_back". Neither is true, and the second overload -- the one this forwards
         * to -- documents the opposite. There is no push_back, and an undersized destination
         * is not rejected: it is resized. Whether that resize is the intended adaptation or a
         * divergence to repair is SR-AUD-055's open question and is not settled here.
         *
         * @param destination The target vector. It is resized to hold Count elements if it
         *                    is shorter than that; see CopyTo(std::vector<T>&, intcs).
         * @throws System::InvalidOperationException if this is a default segment.
         */
        void CopyTo(std::vector<T>& destination) const
        {
            CopyTo(destination, 0);
        }

        /**
         * @brief Copies the segment's elements into a destination vector starting at @p destinationIndex.
         *
         * C++ counterpart of .NET ArraySegment<T>.CopyTo(T[], int).
         * Ensures the destination vector has enough capacity; fills with default-constructed
         * elements if necessary.
         * @param destination      The target vector.
         * @param destinationIndex Zero-based index in @p destination at which to start writing.
         * The destination vector is automatically resized (never throws for insufficient room).
         * @throws System::InvalidOperationException if this is a default segment (checked
         *         first, matching .NET, and therefore before the destination is resized).
         */
        void CopyTo(std::vector<T>& destination, intcs destinationIndex) const
        {
            throwIfDefault();
            if (destinationIndex < 0)
                throw System::ArgumentOutOfRangeException("destinationIndex", "Non-negative number required.");
            // Widen to SharpRuntime::longcs so a destinationIndex near INT32_MAX cannot
            // overflow the addition (the same overflow-bypasses-bounds-check pattern
            // already fixed elsewhere in this file's constructor/Slice).
            SharpRuntime::longcs needed = static_cast<SharpRuntime::longcs>(destinationIndex) +
                                           static_cast<SharpRuntime::longcs>(count_);
            if (static_cast<SharpRuntime::longcs>(destination.size()) < needed)
                destination.resize(static_cast<size_t>(needed));
            // The destination may BE the backing vector, so this must be overlap-safe
            // (SR-AUD-044). The pointers are taken after the resize above, so a
            // reallocation cannot leave them dangling.
            System::detail::copyOverlapAware(begin(), static_cast<std::size_t>(count_),
                                             destination.data() + destinationIndex);
        }

        /**
         * @brief Copies the segment's elements into another ArraySegment.
         *
         * C++ counterpart of .NET ArraySegment<T>.CopyTo(ArraySegment<T>).
         * @param destination The target segment; must be at least as large as this segment.
         * @throws System::InvalidOperationException if this segment, or @p destination, is a
         *         default segment -- this one checked first, exactly as .NET orders it.
         * @throws System::ArgumentException if the destination is too short.
         */
        void CopyTo(ArraySegment<T>& destination) const
        {
            throwIfDefault();
            destination.throwIfDefault();
            if (count_ > destination.count_)
                throw System::ArgumentException("Destination ArraySegment is too short.");
            // Two segments over the same vector overlap; see CopyTo(std::vector<T>&, intcs).
            System::detail::copyOverlapAware(begin(), static_cast<std::size_t>(count_),
                                             destination.begin());
        }

        // -----------------------------------------------------------------------
        // ToArray
        // -----------------------------------------------------------------------

        /**
         * @brief Copies the segment's elements into a new vector and returns it.
         *
         * C++ counterpart of .NET ArraySegment<T>.ToArray().
         * @return A new std::vector<T> containing exactly the Count elements of this segment.
         * @throws System::InvalidOperationException if this is a default segment.
         */
        [[nodiscard]] std::vector<T> ToArray() const
        {
            throwIfDefault();
            return std::vector<T>(begin(), end());
        }

        // -----------------------------------------------------------------------
        // Search
        // -----------------------------------------------------------------------

        /**
         * @brief Determines whether the segment contains @p item.
         *
         * C++ counterpart of ICollection<T>.Contains from .NET ArraySegment<T>.
         * @param item The element to locate.
         * @return true if the item is found; otherwise false.
         * @throws System::InvalidOperationException if this is a default segment.
         */
        [[nodiscard]] bool Contains(const T& item) const
        {
            throwIfDefault();
            return std::find(begin(), end(), item) != end();
        }

        /**
         * @brief Returns the zero-based segment-local index of the first occurrence of @p item.
         *
         * C++ counterpart of IList<T>.IndexOf from .NET ArraySegment<T>.
         * @param item The element to locate.
         * @return The segment-local index, or -1 if not found.
         * @throws System::InvalidOperationException if this is a default segment.
         */
        [[nodiscard]] intcs IndexOf(const T& item) const
        {
            throwIfDefault();
            const T* p = std::find(begin(), end(), item);
            if (p == end()) return -1;
            return static_cast<intcs>(p - begin());
        }

        // -----------------------------------------------------------------------
        // Equality
        // -----------------------------------------------------------------------

        /**
         * @brief Determines whether this segment equals another.
         *
         * C++ counterpart of .NET ArraySegment<T>.Equals(ArraySegment<T>).
         * Two segments are equal if they point to the same underlying array at the same
         * offset with the same count.
         */
        [[nodiscard]] bool Equals(const ArraySegment<T>& other) const noexcept
        {
            return array_ == other.array_ && offset_ == other.offset_ && count_ == other.count_;
        }

        /** @brief Returns true if both segments refer to the same sub-range of the same array. */
        bool operator==(const ArraySegment<T>& other) const noexcept { return Equals(other); }

        /** @brief Returns true if the segments differ. */
        bool operator!=(const ArraySegment<T>& other) const noexcept { return !Equals(other); }

        // -----------------------------------------------------------------------
        // GetHashCode
        // -----------------------------------------------------------------------

        /**
         * @brief Returns a hash code for this segment.
         *
         * C++ counterpart of .NET ArraySegment<T>.GetHashCode().
         * Combines the array pointer, offset, and count.
         */
        [[nodiscard]] int GetHashCode() const noexcept
        {
            size_t h = std::hash<void*>{}(array_);
            h ^= std::hash<intcs>{}(offset_) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<intcs>{}(count_)  + 0x9e3779b9 + (h << 6) + (h >> 2);
            return static_cast<int>(h & 0x7fffffff);
        }
    };

} // namespace System
