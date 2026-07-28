// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <any>

#include "System/InvalidOperationException.hpp"

namespace System::Collections
{
    namespace detail
    {
        /**
         * @brief Tracks whether an enumerator is before, on, or after a valid element.
         *
         * Concrete enumerators use this guard before touching native container storage so
         * Current cannot turn an invalid lifecycle state into undefined behaviour.
         */
        class EnumeratorState
        {
            enum class Position
            {
                BeforeFirst,
                Current,
                AfterLast
            };

            Position position_ = Position::BeforeFirst;

        public:
            /** @brief Returns whether enumeration has reached its terminal state. */
            [[nodiscard]] bool isAfterLast() const noexcept
            {
                return position_ == Position::AfterLast;
            }

            /** @brief Records that Current refers to a valid element. */
            void setCurrent() noexcept { position_ = Position::Current; }
            /** @brief Records that enumeration has passed its final element. */
            void setAfterLast() noexcept { position_ = Position::AfterLast; }
            /** @brief Restores the initial state before the first element. */
            void Reset() noexcept { position_ = Position::BeforeFirst; }

            /**
             * @brief Requires Current to refer to a valid element.
             * @throws System::InvalidOperationException before start or after end.
             */
            void requireCurrent() const
            {
                if (position_ == Position::BeforeFirst)
                    throw System::InvalidOperationException("Enumeration has not started. Call MoveNext.");
                if (position_ == Position::AfterLast)
                    throw System::InvalidOperationException("Enumeration already finished.");
            }
        };

        /**
         * @brief Requires an enumerated collection to retain its captured version.
         * @throws System::InvalidOperationException if the version changed.
         */
        inline void requireUnmodified(bool unmodified)
        {
            if (!unmodified)
                throw System::InvalidOperationException("Collection was modified; enumeration operation may not execute.");
        }
    }

    /**
     * @brief Supports simple iteration over a non-generic collection.
     *
     * C++ counterpart of the .NET System.Collections.IEnumerator interface.
     *
     * @par Ownership and lifetime
     * getCurrentProperty() returns an **owning** std::any by value, the direct
     * counterpart of .NET's `object IEnumerator.Current`. The caller owns the
     * returned box outright: nothing the enumerator or the collection does
     * afterwards can invalidate it, and nothing done to the box can reach the
     * collection. Before ticket #1793 this accessor returned a mutable `void*`
     * that aliased live collection storage, with no validity window of any kind;
     * that is the defect this contract closes. See
     * docs/IEnumeratorCurrentSafetyDesign.md.
     *
     * @par Naming
     * System::Collections::Generic::IAsyncEnumerator<T> spells its *typed*
     * accessor `getCurrentProperty()`, while the synchronous
     * Generic::IEnumerator<T> spells the typed one `Current()` and reserves
     * `getCurrentProperty()` for this type-erased one. The two interfaces
     * therefore give one name to two different operations. That inconsistency is
     * **known and deliberately not repaired**: renaming either would be a second,
     * unrelated public break (design record section 4, risk 7).
     */
    class IEnumerator
    {
    public:
        virtual ~IEnumerator() = default;

    /**
     * @brief Advances the enumerator to the next element of the collection.
     *
     * C++ counterpart of .NET IEnumerator.MoveNext().
     * @return true if the enumerator was advanced; false if past the end.
     */
    virtual bool MoveNext() = 0;

    /**
     * @brief Sets the enumerator to its initial position, before the first element.
     *
     * C++ counterpart of .NET IEnumerator.Reset().
     */
    virtual void Reset() = 0;

    /**
     * @brief Gets the current element in the collection, as a boxed copy.
     *
     * C++ counterpart of .NET IEnumerator.Current, which returns `object` -- a
     * value, and for a value type a boxed copy. The returned std::any OWNS its
     * value: it is unaffected by any later MoveNext(), Reset(), mutation of the
     * collection, destruction of the enumerator, or destruction of the
     * collection, and writing to it cannot reach the collection.
     *
     * Recover the value with std::any_cast<T>(); the boxed type is exactly the
     * element type the enumerator was declared over, and std::any::type() can be
     * queried when it is not known statically. A wrong std::any_cast throws
     * std::bad_any_cast -- a `std::` exception, not a `System::` one, consistent
     * with how this port already exposes std::any elsewhere -- instead of
     * silently reinterpreting bytes, which the previous `void*` return did.
     *
     * If the element type has shared reference semantics of its own (for example
     * std::shared_ptr<X>), the box copies the *handle*, exactly as .NET returns
     * the reference for a reference type: mutating the pointee is not mutating
     * the collection, and no mutation counter moves.
     *
     * @return A copy of the current element, boxed.
     * @throws System::InvalidOperationException if iteration has not started
     *         ("Enumeration has not started. Call MoveNext.") or has already
     *         finished ("Enumeration already finished."). The state-machine
     *         check runs before any copy is made or any box is constructed.
     * @throws System::NotSupportedException if the element type cannot be copied
     *         and therefore cannot be boxed. Matches .NET's documented behaviour
     *         for a `ref struct` element type; the typed accessor still works.
     */
    [[nodiscard]] virtual std::any getCurrentProperty() const = 0;
    };
}
