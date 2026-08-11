// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>

namespace System {

    /**
     * @brief Encapsulates a memory slot to store local data.
     *
     * C++ counterpart of .NET System.LocalDataStoreSlot.
     * In .NET, LocalDataStoreSlot is used with Thread.AllocateDataSlot() and
     * Thread.GetData()/SetData() for thread-local storage.
     * In sharp-runtime the slot stores a single std::any value (not per-thread;
     * thread-local semantics require the caller to manage per-thread state).
     *
     * @note Status: Stub — thread-local per-slot semantics are not implemented.
     *       Use the `thread_local` storage-duration specifier, or
     *       `Threading::ThreadLocal<T>`, for actual TLS.
     *
     * @warning **The gap is wider than "not per-thread", and none of it is
     * repaired by this note** (SR-AUD-129; the decision is ticket #2298).
     *
     * - **There is no door.** In .NET this type's constructor is *internal*: a
     *   public caller never names it, and reaches a slot only through
     *   `Thread.AllocateDataSlot`/`AllocateNamedDataSlot` and reads or writes it
     *   through `Thread.GetData`/`SetData`. This repository has **no such
     *   `Thread` API at all**, so the public default constructor and the
     *   `getData`/`setData` pair below are a project-owned surface wearing a
     *   .NET name, not a counterpart of one.
     * - **One value, shared by every thread.** The single `std::any` below is
     *   the whole storage. A write from any thread replaces what every other
     *   thread reads — the opposite of what the .NET name promises.
     * - **No synchronization policy, and none is implied.** Concurrent calls
     *   touching the same slot, at least one of them a write, are a data race
     *   with undefined behaviour; the `noexcept` on the mutators is a statement
     *   about exceptions only. Callers must supply their own mutual exclusion.
     *
     * The `noexcept` specifications themselves are sound and were checked:
     * `std::any`'s move assignment and `reset()` are both `noexcept`, and the
     * by-value parameter of `setData` is constructed at the call site, outside
     * this function's exception specification, so no allocation failure can
     * cross a `noexcept` boundary here.
     */
    class LocalDataStoreSlot final {
        std::any data_;

    public:
        /** @brief Constructs an empty LocalDataStoreSlot. */
        LocalDataStoreSlot() = default;

        /**
         * @brief Returns the data stored in this slot.
         *
         * C++ counterpart of reading back the value set via Thread.SetData.
         * @return The stored std::any value (empty if nothing has been set).
         */
        [[nodiscard]] const std::any& getData() const noexcept { return data_; }

        /**
         * @brief Stores a value in this slot.
         *
         * C++ counterpart of Thread.SetData(LocalDataStoreSlot, object).
         * @param value The value to store.
         */
        void setData(std::any value) noexcept { data_ = std::move(value); }

        /** @brief Returns true if a value has been stored in this slot. */
        [[nodiscard]] bool hasData() const noexcept { return data_.has_value(); }

        /** @brief Clears the stored value. */
        void clear() noexcept { data_.reset(); }
    };

} // namespace System
