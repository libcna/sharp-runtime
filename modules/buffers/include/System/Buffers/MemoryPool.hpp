// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Buffers/IMemoryOwner.hpp"

namespace System::Buffers {

    using SharpRuntime::intcs;

    /**
     * @brief Provides a resource pool that enables reusing instances of T arrays.
     *
     * C++ counterpart of .NET System.Buffers.MemoryPool&lt;T&gt;.
     * Subclasses provide pool-specific allocation strategies. The shared instance
     * provides a simple heap-backed default implementation.
     *
     * @tparam T The type of items in the memory pool.
     *
     * @par Requirements on T
     * .NET places no constraint on `MemoryPool<T>`'s `T`. This port's shared pool hands out
     * `std::vector<T>`-backed owners, so **T must be default-constructible**: a rented block
     * is value-initialized. The requirement is enforced by the shared pool's owner, which is
     * reached from `Shared()` — note that `DefaultMemoryPool_<T>::Rent` is `virtual`, so the
     * requirement bites when the shared pool is instantiated, not only when `Rent` is called.
     * A custom subclass backed by storage that does not value-initialize is free of it.
     *
     * The requirement is `static_assert`ed where it is already enforced, so the same set of
     * programs compiles as before and only the diagnostic changes.
     * Ticket #2054 / SR-AUD-070, family B-C; see docs/BuffersNamespaceReviewPlan.md §4.4.
     */
    template<typename T>
    class MemoryPool {
    public:
        /** Matches .NET's Array.MaxLength (Array.cs), the ceiling MaxBufferSize/Rent() validate against. */
        static constexpr intcs MaxArrayLength = 0x7FFFFFC7;

        /** @brief Virtual destructor. */
        virtual ~MemoryPool() = default;

        /**
         * @brief Returns the maximum buffer size supported by this pool.
         * @return Maximum number of elements a single Rent() may return.
         */
        [[nodiscard]] virtual intcs getMaxBufferSizeProperty() const noexcept {
            return MaxArrayLength;
        }

        /**
         * @brief Rents a memory block with at least @p minBufferSize elements.
         * @param minBufferSize Minimum number of elements; -1 (the default) means
         *        implementation choice. Any other negative value, or a value greater
         *        than getMaxBufferSizeProperty(), throws.
         * @return An IMemoryOwner&lt;T&gt; representing the rented block.
         * @throws System::ArgumentOutOfRangeException if @p minBufferSize is negative
         *         (and not -1) or exceeds getMaxBufferSizeProperty().
         */
        virtual std::unique_ptr<IMemoryOwner<T>> Rent(intcs minBufferSize = -1) = 0;

        /**
         * @brief Returns the shared MemoryPool instance.
         *
         * The shared pool allocates directly from the heap without pooling.
         * @return Reference to the process-wide shared MemoryPool&lt;T&gt;.
         */
        [[nodiscard]] static MemoryPool<T>& Shared();
    };

    // -----------------------------------------------------------------------
    // Implementation helpers (defined after MemoryPool is complete)
    // -----------------------------------------------------------------------

    /**
     * @brief Simple heap-backed IMemoryOwner implementation.
     *
     * @tparam T Must be default-constructible: the block is a value-initialized
     *         `std::vector<T>`. See MemoryPool's "Requirements on T" note.
     */
    template<typename T>
    class MemoryPoolHeapOwner_ : public IMemoryOwner<T> {
        std::vector<T> buf_;
        /**
         * @brief Whether Dispose() has run.
         *
         * Ticket #2056(a). `getMemoryProperty()` had no disposed check and returned a
         * **zero-length** `Memory` after `Dispose()`, where .NET's `ArrayMemoryPoolBuffer.Memory`
         * throws `ObjectDisposedException` (`ArrayMemoryPool.ArrayMemoryPoolBuffer.cs:18-25`).
         * A caller could not tell a disposed owner from a live `Rent(0)`.
         *
         * **Why a flag and not .NET's own discriminator.** .NET needs no flag: it nulls
         * `_array` and tests `array is null`. The port's natural equivalent would be a
         * `std::unique_ptr<std::vector<T>>`, which would even make the object *smaller*. It was
         * rejected deliberately: `reset()` frees the storage **deterministically**, whereas
         * `clear() + shrink_to_fit()` today is non-binding — and half (b) of this ticket, a
         * `Memory<T>` retained across `Dispose()`, is still open. Turning that latent
         * use-after-free from "usually survives" into "always broken" while nothing yet fixes it
         * would be a practical regression dressed as parity. The storage lifetime is therefore
         * left exactly as it was.
         *
         * `sizeof(MemoryPoolHeapOwner_<int>)` 32 → 40 under `docs/StandingApprovals.md` SA-3.
         */
        bool disposed_ = false;
    public:
        explicit MemoryPoolHeapOwner_(intcs size) : buf_(static_cast<std::size_t>(size)) {
            // Deliberately in the CONSTRUCTOR body, not at class scope: the member
            // declaration alone compiles today for a non-default-constructible T, so a
            // class-scope assert would reject a program that merely names the type.
            static_assert(
                std::is_default_constructible_v<T>,
                "System::Buffers::MemoryPool<T>::Shared().Rent() requires T to be "
                "default-constructible: the rented block is a value-initialized "
                "std::vector<T>. See docs/BuffersNamespaceReviewPlan.md, ticket #2054.");
        }
        /**
         * @return The rented block.
         * @throws System::ObjectDisposedException if `Dispose()` has already run (#2056).
         *
         * @warning **Half (b) of #2056 is still open**: a `Memory<T>` obtained *before*
         *          `Dispose()` keeps a pointer and a length over storage `Dispose()` may have
         *          released. Repairing that is a `Memory<T>` ownership change in `Core.Base`,
         *          not a change to this type, and this member cannot defend against it — by the
         *          time the caller holds the `Memory`, this object is no longer in the path.
         */
        System::Memory<T> getMemoryProperty() override {
            System::ObjectDisposedException::ThrowIf(disposed_, "MemoryPool<T>.Rent()");
            return System::Memory<T>(buf_);
        }
        /** @brief Releases the rented block. Idempotent, as .NET's is. */
        void Dispose() override {
            // The storage handling is deliberately unchanged; only the flag is new. See the
            // disposed_ doc-comment for why the .NET-shaped null discriminator was rejected.
            disposed_ = true;
            buf_.clear();
            buf_.shrink_to_fit();
        }
    };

    /** @brief Concrete default pool implementation returned by Shared(). */
    template<typename T>
    class DefaultMemoryPool_ : public MemoryPool<T> {
    public:
        std::unique_ptr<IMemoryOwner<T>> Rent(intcs minBufferSize = -1) override {
            // Matches ArrayMemoryPool<T>.Rent (ArrayMemoryPool.cs): only exactly -1 means
            // "use the implementation default"; any other negative value is invalid. The
            // default itself is sized in *bytes* (~4096), not elements, unlike the prior
            // implementation here which always rented 4096 elements regardless of sizeof(T).
            if (minBufferSize == -1) {
                minBufferSize = 1 + (4095 / static_cast<intcs>(sizeof(T)));
            } else if (minBufferSize < 0 || minBufferSize > MemoryPool<T>::MaxArrayLength) {
                throw System::ArgumentOutOfRangeException("minimumBufferSize");
            }
            return std::make_unique<MemoryPoolHeapOwner_<T>>(minBufferSize);
        }
    };

    template<typename T>
    MemoryPool<T>& MemoryPool<T>::Shared() {
        static DefaultMemoryPool_<T> instance;
        return instance;
    }

} // namespace System::Buffers
