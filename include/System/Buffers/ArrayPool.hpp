// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <vector>

namespace System::Buffers {

    /// Provides a resource pool for reusing instances of arrays of type T.
    template<typename T>
    class ArrayPool {
    public:
        /// Destroys the pool and releases associated resources.
        virtual ~ArrayPool() = default;

        /// Retrieves a buffer from the pool with at least the specified minimum length.
        virtual std::vector<T> Rent(int minimumLength) = 0;
        /// Returns a rented buffer back to the pool, optionally clearing its contents first.
        virtual void Return(std::vector<T>& array, bool clearArray = false) {
            if (clearArray) array.assign(array.size(), T{});
        }

        /// Returns a shared, process-wide ArrayPool instance.
        static ArrayPool<T>& Shared();
    };

    // Defined after ArrayPool<T> is complete to avoid incomplete-type warning.
    /// Default shared ArrayPool implementation that allocates a new vector on each Rent call.
    template<typename T>
    struct SharedArrayPool : ArrayPool<T> {
        /// Rents a vector of at least minimumLength elements.
        std::vector<T> Rent(int minimumLength) override {
            return std::vector<T>(static_cast<size_t>(minimumLength));
        }
    };

    template<typename T>
    ArrayPool<T>& ArrayPool<T>::Shared() {
        static SharedArrayPool<T> instance;
        return instance;
    }

} // namespace System::Buffers
