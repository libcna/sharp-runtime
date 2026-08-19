// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IDisposable.hpp"

namespace System::Buffers {

// Forward declaration to break circular dependency with IPinnable.
struct IPinnable;

/**
 * @brief A handle for pinned memory.
 *
 * C++ counterpart of .NET System.Buffers.MemoryHandle.
 * Wraps a raw pointer to pinned memory and optionally holds a reference to an
 * IPinnable that must be unpinned on disposal.
 *
 * @note **The caller must call Dispose() explicitly. Scope exit does not unpin — and
 * that MATCHES .NET, which is why no destructor is added here.** .NET's
 * `MemoryHandle` is a `public unsafe struct` with no finalizer
 * (`MemoryHandle.cs:12`), so letting one go out of scope leaves its `IPinnable`
 * pinned there too. `using var handle = memory.Pin();` is a *language* construct that
 * calls `Dispose()`; it is not something the type does for you.
 *
 * SR-AUD-088 reported that this header *promised* RAII cleanup — "should call
 * Dispose() explicitly (or let the destructor do it)" — that it never performed. That
 * promise was the defect and it has been removed. Ticket **#2059** then measured the
 * proposed repair against the reference and **declined it**: adding
 * `~MemoryHandle(){ Dispose(); }` would be a divergence from .NET rather than a repair,
 * and the hazard is real as well as theoretical — this is a copyable handle, so an
 * unpinning destructor would unpin once per copy for a single pin. The absence is pinned
 * by `MemoryHandlePinTests`.
 *
 * `Dispose()` is idempotent, exactly as .NET's is (`MemoryHandle.cs:41-53`): it unpins,
 * clears the `IPinnable` and nulls the pointer, so a second call does nothing.
 */
struct MemoryHandle : System::IDisposable {
    /** @brief Constructs a default (null) MemoryHandle. */
    MemoryHandle() = default;

    /**
     * @brief Constructs a MemoryHandle from a raw pointer and optional IPinnable.
     * @param pointer  Pointer to the pinned memory.
     * @param pinnable The IPinnable that pinned the memory, or nullptr.
     */
    explicit MemoryHandle(void* pointer, IPinnable* pinnable = nullptr)
        : pointer_(pointer), pinnable_(pinnable) {}

    /**
     * @brief Gets the pointer to the pinned memory.
     * @return Raw pointer to pinned memory, or nullptr.
     */
    [[nodiscard]] void* getPointerProperty() const noexcept { return pointer_; }

    /**
     * @brief Frees the pinned handle and releases IPinnable.
     *
     * Defined after IPinnable is complete (see IPinnable.hpp).
     */
    void Dispose() override;

private:
    // PRIVATE, matching .NET's `private void* _pointer` / `private IPinnable? _pinnable`
    // (MemoryHandle.cs:14-16). This port published both, so a caller could retarget a live
    // handle at an unrelated address, or detach its IPinnable and leak the pin, behind the
    // owner's back. .NET publishes only `Pointer`, and only as a getter.
    //
    // .NET's third field, `private GCHandle _handle`, is deliberately absent: this runtime
    // has no moving collector, so there is no GC handle to free.
    void*      pointer_  = nullptr;
    IPinnable* pinnable_ = nullptr;
};

} // namespace System::Buffers
