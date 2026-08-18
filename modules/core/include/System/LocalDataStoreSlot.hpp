// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>

namespace System::Threading { class Thread; }

namespace System {

    /**
     * @brief Encapsulates a memory slot to store thread-local data.
     *
     * C++ counterpart of .NET `System.LocalDataStoreSlot`
     * (`LocalDataStoreSlot.cs:9-23`), whose constructor is **internal**: a public caller never
     * names it, and reaches a slot only through `Thread.AllocateDataSlot`,
     * `Thread.AllocateNamedDataSlot`, `Thread.GetNamedDataSlot`, `Thread.FreeNamedDataSlot`,
     * `Thread.GetData` and `Thread.SetData`.
     *
     * @par What ticket #2298 changed
     * Before it, this type had a **public default constructor** and a `getData`/`setData` pair,
     * and this repository had **no `Thread` data-slot API at all** — so the whole surface was
     * project-owned, wearing a .NET name. Worse, the single `std::any` it held was **one value
     * shared by every thread**: a write from any thread replaced what every other thread read,
     * which is the opposite of what the name promises, and two threads touching one slot with at
     * least one write was a data race.
     *
     * Route **B** of the ticket's four was taken, under `docs/StandingApprovals.md` SA-9, which
     * authorised the new `Thread` surface explicitly: thread-indexed storage behind the .NET
     * doors, **plus** a non-public constructor so the .NET door is the only one.
     *
     * @par Why the slot holds an id rather than the storage
     * .NET's slot holds a `ThreadLocal<object?>`. This type lives in `Core.Base` and `Thread`
     * lives in `modules/threading`, which depends on `Core.Base` — so a slot holding a
     * thread-local would invert the module graph. It holds an opaque **id** instead, and `Thread`
     * keeps the per-thread storage. The observable contract is identical, and the graph is
     * unchanged at 41/92.
     *
     * @note `FreeNamedDataSlot` removes a name from the map; it does not destroy a slot a caller
     *       still holds, exactly as in .NET, where the map drops its reference and the GC decides
     *       the rest. Here the slot stays valid and keeps its own per-thread values.
     */
    class LocalDataStoreSlot final {
        std::size_t id_ = 0;

        /// Only `Thread` may mint a slot, matching .NET's internal constructor.
        explicit LocalDataStoreSlot(std::size_t id) noexcept : id_(id) {}

        [[nodiscard]] std::size_t id() const noexcept { return id_; }

        friend class System::Threading::Thread;

    public:
        /// Copyable and movable: a slot is a handle, and .NET's is a reference passed around
        /// freely. Two copies name the same storage.
        LocalDataStoreSlot(const LocalDataStoreSlot&) = default;
        LocalDataStoreSlot(LocalDataStoreSlot&&) = default;
        LocalDataStoreSlot& operator=(const LocalDataStoreSlot&) = default;
        LocalDataStoreSlot& operator=(LocalDataStoreSlot&&) = default;
        ~LocalDataStoreSlot() = default;

        /// Two slots are the same slot when they name the same storage.
        [[nodiscard]] bool Equals(const LocalDataStoreSlot& other) const noexcept {
            return id_ == other.id_;
        }
        bool operator==(const LocalDataStoreSlot& o) const noexcept { return Equals(o); }
        bool operator!=(const LocalDataStoreSlot& o) const noexcept { return !Equals(o); }
    };

} // namespace System
