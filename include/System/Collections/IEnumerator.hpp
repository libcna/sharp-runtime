// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Collections
{
    /**
     * @brief Supports simple iteration over a non-generic collection.
     *
     * C++ counterpart of the .NET System.Collections.IEnumerator interface.
     */
    class IEnumerator
    {
    public:
        virtual ~IEnumerator() = default;

        /// Advances the enumerator to the next element. Returns false when past the end.
        virtual bool MoveNext() = 0;

        /// Resets the enumerator to before the first element.
        virtual void Reset() = 0;

        /// Returns the current element as a void pointer (caller must cast).
        [[nodiscard]] virtual void* getCurrent() const = 0;
    };
}
