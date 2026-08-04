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
 * @warning **This type performs no RAII cleanup: the caller MUST call Dispose()
 * explicitly.** There is no destructor that calls Dispose, and the inherited
 * `~IDisposable` does nothing, so letting a MemoryHandle go out of scope leaves its
 * IPinnable pinned. The header previously claimed the destructor would do it; it
 * never did (SR-AUD-088).
 *
 * Adding such a destructor is **not** a small correction and is deliberately not made
 * here: this is a copyable aggregate with public members, so an unpinning destructor
 * would make every copy unpin the same IPinnable more than once. A correct repair needs
 * move-only or reference-counted semantics on a type that `Memory.hpp` and
 * `ReadOnlyMemory.hpp` include, i.e. reaching all of `Core.Base`. That is blocked ticket
 * **#2059** (CCF-019); see docs/BuffersNamespaceReviewPlan.md §4.11. The behaviour
 * documented above is pinned by a permanent test, so it cannot change silently.
 */
struct MemoryHandle : System::IDisposable {
    void*      pointer_  = nullptr;
    IPinnable* pinnable_ = nullptr;

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
};

} // namespace System::Buffers
