// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <vector>
#include "System/IDisposable.hpp"
#include "System/Memory.hpp"

namespace System::Buffers {

    /**
     * @brief Identifies an owner of a memory buffer, providing access to and lifetime
     * management for the buffer.
     *
     * @warning A `Memory<T>` returned by this interface is a non-owning C++ view and is valid
     * only while its owner remains alive and undisposed (SR-AUD-071b).
     *
     * `getMemoryProperty()` after `Dispose()` has a terminal disposed state and throws
     * `ObjectDisposedException`, matching .NET. A `Memory<T>` obtained **before** `Dispose()`
     * keeps its pointer and its original
     *   length across the disposal, over storage the owner has already released. Reading
     * through such a retained view is undefined behaviour. **Do not retain a `Memory<T>` past
     * its owner's `Dispose()` or destruction.** This is the same explicit borrowing rule as
     * `Memory<T>`'s vector constructor and is an accepted C++ lifetime deviation: sharp-runtime
     * has no garbage collector capable of keeping the managed array alive through the view.
     *
     * Ticket #2056 repaired the post-dispose getter. The final audit reconciliation records
     * the retained-view rule as an accepted deviation rather than an unfinished remediation.
     */
    template<typename T>
    class IMemoryOwner : public System::IDisposable {
    public:
        /** Destroys the memory owner and releases associated resources. */
        virtual ~IMemoryOwner() = default;
        /**
         * @brief Returns the memory buffer owned by this instance.
         *
         * C++ counterpart of .NET IMemoryOwner&lt;T&gt;.Memory.
         *
         * @return The owned buffer.
         * @throws System::ObjectDisposedException if this owner has been disposed.
         */
        virtual System::Memory<T> getMemoryProperty() = 0;
    };

} // namespace System::Buffers
