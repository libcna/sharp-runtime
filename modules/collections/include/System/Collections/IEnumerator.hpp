// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

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
     * @brief Gets the current element in the collection.
     *
     * C++ counterpart of .NET IEnumerator.Current.
     * @return Pointer to the current element; cast to the appropriate type.
     * @throws System::InvalidOperationException if iteration has not started
     *         or has already finished.
     */
    [[nodiscard]] virtual void* getCurrentProperty() const = 0;
    };
}
