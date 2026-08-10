// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Span.hpp"
#include "System/Collections/IEnumerable.hpp"

namespace System::Collections {

using SharpRuntime::intcs;

/**
 * @brief Canonical destination for the non-generic collection copy boundary:
 *        a length-aware view over already-constructed std::any elements.
 *
 * Not a .NET type name. C++ has no runtime Array object carrying an element
 * Type, a Rank, a lower bound, and a Length, so the boxed-object element type is
 * fixed at compile time (std::any, this port's boxed `object`) and the element
 * count travels with the destination instead of being runtime metadata.
 *
 * The elements a span points at must already be constructed; CopyTo assigns into
 * them and never constructs in place.
 */
using ObjectSpan = System::Span<std::any>;

namespace detail {

/**
 * @brief Validates a copy destination exactly once, before any element is written.
 *
 * Shared by System::Collections::ICollection::CopyTo and by every typed CopyTo
 * overload on the concrete non-generic collections, so all of them diagnose the
 * same invalid-argument classes with the same exception types and messages.
 * Follows the System::Collections::detail::EnumeratorState precedent introduced
 * by ticket #1767 in IEnumerator.hpp.
 *
 * The capacity test is written as a subtraction (`length - index < count`)
 * rather than `index + count > length` so that a large @p index cannot make the
 * arithmetic overflow and silently pass, matching .NET's own Array.Copy guard.
 *
 * A destination with a null pointer and a zero length is a valid empty
 * destination -- `ObjectSpan` has no distinct managed-null-array state, so
 * `ObjectSpan{nullptr, 0}` and a default-constructed empty `std::vector<std::any>`
 * (whose `data()` is typically null) both describe "no storage, no elements",
 * not ".NET null". Only a null pointer paired with a *positive* length is
 * rejected as malformed: no valid destination can claim elements it has no
 * storage for. Ticket #1774 corrects the stricter rule ticket #1771 shipped,
 * which rejected every null-data destination outright (see
 * docs/ICollectionCopyToDesign.md section 22).
 *
 * Checked in this exact order, so the diagnosis is deterministic when more than
 * one condition holds at once:
 * 1. negative index;
 * 2. index past the destination end;
 * 3. null data paired with a positive length;
 * 4. insufficient remaining capacity.
 *
 * @param data   First element of the destination storage.
 * @param length Number of elements the destination can hold.
 * @param index  Zero-based destination index at which copying would begin.
 * @param count  Number of elements that would be written.
 * @throws System::ArgumentOutOfRangeException if @p index is negative.
 * @throws System::ArgumentException           if @p index is past the end of the
 *         destination, or the destination cannot hold @p count elements from
 *         @p index onwards.
 * @throws System::ArgumentNullException       if @p data is null while
 *         @p length is positive -- a destination that claims elements it has no
 *         storage for. A null @p data with a @p length of zero is a valid empty
 *         destination and is never rejected on that basis.
 */
inline void requireValidCopyDestination(const void* data, intcs length,
                                        intcs index, intcs count) {
    if (index < 0)
        throw System::ArgumentOutOfRangeException("index", "Non-negative number required.");
    if (index > length)
        throw System::ArgumentException(
            "Destination array is not long enough to copy all the items in the "
            "collection. Check array index and length.", "destination");
    if (data == nullptr && length > 0)
        throw System::ArgumentNullException("destination");
    if (length - index < count)
        throw System::ArgumentException(
            "Destination array is not long enough to copy all the items in the "
            "collection. Check array index and length.", "destination");
}

/**
 * @brief Same contract as the pointer/length overload, for the typed
 *        std::vector destinations of the concrete non-generic collections.
 * @tparam T Destination element type.
 * @param destination Destination vector; its size() supplies the capacity.
 * @param index Zero-based destination index at which copying would begin.
 * @param count Number of elements that would be written.
 */
template<typename T>
inline void requireValidCopyDestination(std::vector<T>& destination,
                                        intcs index, intcs count) {
    requireValidCopyDestination(destination.data(),
                                static_cast<intcs>(destination.size()), index, count);
}

} // namespace detail

/**
 * @brief Defines size, enumerators, and synchronization methods for all non-generic collections.
 *
 * C++ counterpart of .NET System.Collections.ICollection.
 *
 * The copy boundary is a non-virtual interface: the public CopyTo overloads
 * validate the destination once and then dispatch to the single protected
 * copyToCore hook, so no implementation can skip, weaken, or diverge from the
 * shared validation (audit findings SR-AUD-358 / CCF-020).
 *
 * @note Intentional, permanent deviations from .NET's ICollection.CopyTo(Array, int),
 * all stemming from the documented reflection/%System::Type deviation: a
 * destination has no rank and no lower bound, so ArgumentException for
 * `Arg_RankMultiDimNotSupported` and `Arg_NonZeroLowerBound` has no
 * representable input; element-type compatibility is fixed at compile time, so
 * ArrayTypeMismatchException and InvalidCastException are statically
 * unreachable; and overlapping source/destination storage is unsupported
 * (.NET's Array.Copy defines it through Memmove).
 */
class ICollection : public IEnumerable {
public:
    /** @brief Virtual destructor for safe polymorphic destruction. */
    virtual ~ICollection() = default;

    /**
     * @brief Gets the number of elements contained in the collection.
     *
     * C++ counterpart of .NET ICollection.Count.
     */
    [[nodiscard]] virtual intcs getCountProperty() const = 0;

    /**
     * @brief Copies every element, boxed as std::any, into @p destination starting at @p index.
     *
     * C++ counterpart of .NET ICollection.CopyTo(Array, int). Elements are
     * copy-assigned into existing, constructed destination elements; the
     * destination is never resized, nothing is constructed in place, and no
     * element is written if validation fails.
     *
     * @param destination Length-aware view over constructed std::any elements. A
     *        zero-length destination (including one with a null pointer, such as
     *        the default-constructed `ObjectSpan()`) is valid when
     *        getCountProperty() is also zero.
     * @param index       Zero-based index at which copying begins.
     * @throws System::ArgumentOutOfRangeException if @p index is negative.
     * @throws System::ArgumentException           if @p index is past the end of
     *         @p destination, or @p destination cannot hold getCountProperty()
     *         elements starting at @p index -- including a non-empty collection
     *         copied into a zero-length destination.
     * @throws System::ArgumentNullException       if @p destination has a null
     *         pointer and a positive length: a destination that claims elements
     *         it has no storage for.
     */
    void CopyTo(ObjectSpan destination, intcs index) {
        detail::requireValidCopyDestination(destination.getPointer(),
                                            destination.getLengthProperty(),
                                            index, getCountProperty());
        copyToCore(destination, index);
    }

    /**
     * @brief std::vector convenience overload; identical contract to CopyTo(ObjectSpan, intcs).
     * @param destination Destination vector, sized by the caller; never resized here.
     * @param index       Zero-based index at which copying begins.
     */
    void CopyTo(std::vector<std::any>& destination, intcs index) {
        CopyTo(ObjectSpan(destination), index);
    }

    /**
     * @brief Gets an object that can be used to synchronize access to the collection.
     *
     * C++ counterpart of .NET ICollection.SyncRoot.
     * @return A pointer that can be used as a synchronization lock.
     */
    [[nodiscard]] virtual const void* getSyncRootProperty() const { return this; }

    /**
     * @brief Gets a value indicating whether access to the collection is synchronized (thread-safe).
     *
     * C++ counterpart of .NET ICollection.IsSynchronized.
     */
    [[nodiscard]] virtual bool getIsSynchronizedProperty() const { return false; }

protected:
    /**
     * @brief Single copy implementation hook, one per concrete collection.
     *
     * Called only after CopyTo has validated the destination, so an
     * implementation must not re-validate and must not throw for argument
     * reasons. It writes exactly getCountProperty() elements into
     * [@p index, @p index + getCountProperty()) and leaves every other
     * destination element untouched.
     *
     * @param destination Validated destination view.
     * @param index       Validated zero-based destination index.
     */
    virtual void copyToCore(ObjectSpan destination, intcs index) = 0;
};

} // namespace System::Collections
