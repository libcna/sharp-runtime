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
     * @warning **This port has no terminal disposed state, and that has two observable
     * consequences a caller must know about** (SR-AUD-071):
     *
     * - `getMemoryProperty()` **after** `Dispose()` returns an **empty** `Memory<T>`. .NET's
     *   own pool owner throws `ObjectDisposedException` instead, so code ported from C#
     *   that relies on the exception to detect a use-after-dispose will silently observe a
     *   zero-length buffer here.
     * - A `Memory<T>` obtained **before** `Dispose()` keeps its pointer and its original
     *   length across the disposal, over storage the owner has already released. Reading
     *   through such a retained view is undefined behaviour, and an AddressSanitizer probe
     *   recorded a native fault. **Do not retain a `Memory<T>` past its owner's `Dispose()`
     *   or destruction.**
     *
     * Both are blocked ticket **#2056** (CCF-019): the first needs a flag that this port's
     * measured object layout has no room for, and the second needs an ownership change to
     * `Memory<T>` in `Core.Base`. See docs/BuffersNamespaceReviewPlan.md §4.3. Both
     * behaviours are pinned by permanent tests, so neither can change silently.
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
         * @return The owned buffer, or an **empty** `Memory<T>` if this owner has been
         *         disposed — see the class-level warning; .NET throws instead.
         */
        virtual System::Memory<T> getMemoryProperty() = 0;
    };

} // namespace System::Buffers
