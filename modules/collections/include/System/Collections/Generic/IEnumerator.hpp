// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <type_traits>

#include "System/Collections/IEnumerator.hpp"
#include "System/NotSupportedException.hpp"

namespace System::Collections::Generic {

/**
 * @brief Supports simple iteration over a strongly typed generic collection.
 *
 * C++ counterpart of .NET System.Collections.Generic.IEnumerator<T>.
 * Extends the non-generic System::Collections::IEnumerator with a typed Current() accessor.
 *
 * @par The two accessors are different operations
 * Current() is the typed, zero-copy read path and returns `const T&`.
 * getCurrentProperty() is the type-erased path and returns an owning std::any
 * holding a **copy**. It converts rather than aliases, so no caller of the
 * non-generic interface can reach the collection's storage. Before ticket #1793
 * the bridge was `return const_cast<T*>(&Current());`, which republished the very
 * object Current() protects as a writable void*.
 *
 * @tparam T The type of objects to enumerate.
 */
template<typename T>
class IEnumerator : public System::Collections::IEnumerator {
public:
    /** @brief Virtual destructor for safe polymorphic destruction. */
    ~IEnumerator() override = default;

    /**
     * @brief Gets the element in the collection at the current position of the enumerator.
     *
     * C++ counterpart of .NET IEnumerator<T>.Current.
     *
     * @par Validity window
     * The returned reference is valid until the next MoveNext() or Reset() on
     * this enumerator, or the next operation that mutates the collection,
     * whichever comes first. It follows the same invalidation rules as a
     * reference into the collection's underlying storage. Retaining it beyond
     * that window is undefined behaviour; copy the value, or use
     * getCurrentProperty()'s owning box, if it must outlive the current
     * position. This is a **sharp edge that ticket #1793 deliberately did not
     * close**: closing it would require returning `T` by value, which makes a
     * move-only T uninstantiable (design record section 13.1, risk 1).
     *
     * @return A const reference to the current element.
     * @throws System::InvalidOperationException if iteration has not started
     *         or has already finished. Unlike managed IEnumerator<T>, this
     *         C++ adaptation cannot safely return a default value through a
     *         reference when no current element exists.
     */
    [[nodiscard]] virtual const T& Current() const = 0;

    /**
     * @brief Boxes the current element for the non-generic interface.
     *
     * Converts rather than aliases: the returned std::any holds a COPY, so no
     * caller of the non-generic interface can reach the collection's storage.
     * Current()'s own state machine runs first, so the before-start and
     * after-end exceptions are unchanged and still precede any boxing.
     *
     * @return A copy of the current element, boxed.
     * @throws System::InvalidOperationException before the first MoveNext() or
     *         after enumeration has finished, propagated from Current().
     * @throws System::NotSupportedException if T is not copy-constructible and
     *         therefore cannot be boxed. This is the C++ analogue of .NET's
     *         `ref struct` element type, for which the managed interface
     *         documents exactly this answer. Only this type-erased path is
     *         affected: Current(), MoveNext(), and Reset() remain usable, and
     *         the IEnumerator<T> specialization stays instantiable.
     *
     * @note The non-copyable branch calls Current() and discards it, purely so
     *       that the state-machine check still runs FIRST. Without that call the
     *       `if constexpr` would discard the only use of Current(), and a
     *       before-start or after-end read on a move-only T would report
     *       NotSupportedException where the pre-#1793 bridge reported
     *       InvalidOperationException -- silently converting an existing
     *       exception path. The discarded call keeps the ordering the exception
     *       matrix specifies: position first, boxability second.
     */
    [[nodiscard]] std::any getCurrentProperty() const override {
        if constexpr (std::is_copy_constructible_v<T>) {
            return std::any(Current());
        } else {
            (void)Current();
            throw System::NotSupportedException(
                "The element type cannot be boxed; use the typed Current() accessor.");
        }
    }
};

} // namespace System::Collections::Generic
