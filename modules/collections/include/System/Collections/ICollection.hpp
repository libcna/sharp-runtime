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
 * @param data   First element of the destination storage.
 * @param length Number of elements the destination can hold.
 * @param index  Zero-based destination index at which copying would begin.
 * @param count  Number of elements that would be written.
 * @throws System::ArgumentNullException       if @p data is null. A destination
 *         with no storage at all is rejected even when @p count is zero, so a
 *         null destination is always a diagnosed error rather than a silent
 *         no-op (note that a default-constructed empty std::vector may itself
 *         have a null data() and is therefore rejected).
 * @throws System::ArgumentOutOfRangeException if @p index is negative.
 * @throws System::ArgumentException           if @p index is past the end of the
 *         destination, or the destination cannot hold @p count elements from
 *         @p index onwards.
 */
inline void requireValidCopyDestination(const void* data, intcs length,
                                        intcs index, intcs count) {
    if (data == nullptr)
        throw System::ArgumentNullException("destination");
    if (index < 0)
        throw System::ArgumentOutOfRangeException("index", "Non-negative number required.");
    if (index > length || length - index < count)
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
     * @param destination Length-aware view over constructed std::any elements.
     * @param index       Zero-based index at which copying begins.
     * @throws System::ArgumentNullException       if @p destination has no storage.
     * @throws System::ArgumentOutOfRangeException if @p index is negative.
     * @throws System::ArgumentException           if @p destination cannot hold
     *         getCountProperty() elements starting at @p index.
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
