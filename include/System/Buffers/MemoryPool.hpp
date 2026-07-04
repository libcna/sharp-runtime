// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <limits>
#include <memory>
#include <vector>
#include "System/Buffers/IMemoryOwner.hpp"

namespace System::Buffers {

    /**
     * @brief Provides a resource pool that enables reusing instances of T arrays.
     *
     * C++ counterpart of .NET System.Buffers.MemoryPool&lt;T&gt;.
     * Subclasses provide pool-specific allocation strategies. The shared instance
     * provides a simple heap-backed default implementation.
     *
     * @tparam T The type of items in the memory pool.
     */
    template<typename T>
    class MemoryPool {
    public:
        /** @brief Virtual destructor. */
        virtual ~MemoryPool() = default;

        /**
         * @brief Returns the maximum buffer size supported by this pool.
         * @return Maximum number of elements a single Rent() may return.
         */
        [[nodiscard]] virtual int getMaxBufferSizeProperty() const noexcept {
            return std::numeric_limits<int>::max();
        }

        /**
         * @brief Rents a memory block with at least @p minBufferSize elements.
         * @param minBufferSize Minimum number of elements (default −1 means implementation choice).
         * @return An IMemoryOwner&lt;T&gt; representing the rented block.
         */
        virtual std::unique_ptr<IMemoryOwner<T>> Rent(int minBufferSize = -1) = 0;

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

    /** @brief Simple heap-backed IMemoryOwner implementation. */
    template<typename T>
    class MemoryPoolHeapOwner_ : public IMemoryOwner<T> {
        std::vector<T> buf_;
    public:
        explicit MemoryPoolHeapOwner_(int size)
            : buf_(static_cast<std::size_t>(size > 0 ? size : 4096)) {}
        System::Memory<T> getMemoryProperty() override { return System::Memory<T>(buf_); }
        void Dispose() override { buf_.clear(); buf_.shrink_to_fit(); }
    };

    /** @brief Concrete default pool implementation returned by Shared(). */
    template<typename T>
    class DefaultMemoryPool_ : public MemoryPool<T> {
    public:
        std::unique_ptr<IMemoryOwner<T>> Rent(int minBufferSize = -1) override {
            int size = (minBufferSize <= 0) ? 4096 : minBufferSize;
            return std::make_unique<MemoryPoolHeapOwner_<T>>(size);
        }
    };

    template<typename T>
    MemoryPool<T>& MemoryPool<T>::Shared() {
        static DefaultMemoryPool_<T> instance;
        return instance;
    }

} // namespace System::Buffers
