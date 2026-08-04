// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace System::Buffers {

    using SharpRuntime::intcs;

    /** Provides a resource pool for reusing instances of arrays of type T. */
    template<typename T>
    class ArrayPool {
    public:
        /** Destroys the pool and releases associated resources. */
        virtual ~ArrayPool() = default;

        /**
         * @brief Retrieves a buffer from the pool with at least the specified minimum length.
         * @throws ArgumentOutOfRangeException if minimumLength is negative.
         */
        virtual std::vector<T> Rent(intcs minimumLength) = 0;
        /** Returns a rented buffer back to the pool, optionally clearing its contents first. */
        virtual void Return(std::vector<T>& array, bool clearArray = false) {
            if (clearArray) array.assign(array.size(), T{});
        }

        /** Returns a shared, process-wide ArrayPool instance. */
        static ArrayPool<T>& Shared();

        /**
         * @brief Creates a new ArrayPool with default capacity limits.
         *
         * C++ counterpart of .NET ArrayPool&lt;T&gt;.Create(). Returns a heap-backed
         * pool that allocates a new vector on each Rent call.
         */
        static std::unique_ptr<ArrayPool<T>> Create();

        /**
         * @brief Creates a new ArrayPool after validating the specified capacity limits.
         *
         * C++ counterpart of .NET ArrayPool&lt;T&gt;.Create(int, int).
         *
         * Both arguments are **validated exactly as .NET's `ConfigurableArrayPool` validates
         * them**, but — unlike .NET — they are **not applied**: this port's pool allocates a
         * fresh vector on every Rent and has no buckets to size, so `Rent` may return a
         * buffer longer than @p maxArrayLength and no bucket count is enforced. The
         * validation is not decoration: it stops a caller silently configuring a pool with
         * zero or negative limits and believing they took effect. Honouring the limits needs
         * a configured pool type this port does not have.
         *
         * Ticket #2053 / SR-AUD-076; see docs/BuffersNamespaceReviewPlan.md §4.6.
         *
         * @param maxArrayLength       Maximum allowed array length. Must be positive.
         * @param maxArraysPerBucket   Maximum number of arrays per bucket. Must be positive.
         * @throws ArgumentOutOfRangeException if either argument is zero or negative.
         */
        static std::unique_ptr<ArrayPool<T>> Create(intcs maxArrayLength, intcs maxArraysPerBucket);
    };

    // Defined after ArrayPool<T> is complete to avoid incomplete-type warning.
    /** Default shared ArrayPool implementation that allocates a new vector on each Rent call. */
    template<typename T>
    struct SharedArrayPool : ArrayPool<T> {
        /**
         * @brief Rents a vector of at least minimumLength elements.
         * @throws ArgumentOutOfRangeException if minimumLength is negative.
         */
        std::vector<T> Rent(intcs minimumLength) override {
            if (minimumLength < 0)
                throw ArgumentOutOfRangeException("minimumLength");
            return std::vector<T>(static_cast<size_t>(minimumLength));
        }
    };

    template<typename T>
    ArrayPool<T>& ArrayPool<T>::Shared() {
        static SharedArrayPool<T> instance;
        return instance;
    }

    template<typename T>
    std::unique_ptr<ArrayPool<T>> ArrayPool<T>::Create() {
        return std::make_unique<SharedArrayPool<T>>();
    }

    template<typename T>
    std::unique_ptr<ArrayPool<T>> ArrayPool<T>::Create(intcs maxArrayLength, intcs maxArraysPerBucket) {
        // .NET's ConfigurableArrayPool constructor rejects a non-positive value for either
        // limit before it builds a single bucket. This port used to discard both arguments
        // unread, so Create(0, 1), Create(1, 0) and Create(-5, -7) all returned a usable pool
        // and the caller had no way to learn that the configuration meant nothing.
        if (maxArrayLength <= 0)
            throw ArgumentOutOfRangeException("maxArrayLength");
        if (maxArraysPerBucket <= 0)
            throw ArgumentOutOfRangeException("maxArraysPerBucket");
        return std::make_unique<SharedArrayPool<T>>();
    }

} // namespace System::Buffers
