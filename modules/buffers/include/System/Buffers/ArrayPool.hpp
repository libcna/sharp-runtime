// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <type_traits>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace System::Buffers {

    using SharpRuntime::intcs;

    /**
     * @brief Provides a resource pool for reusing instances of arrays of type T.
     *
     * @tparam T The element type of the pooled arrays.
     *
     * @par Requirements on T
     * .NET places no constraint on `ArrayPool<T>`'s `T`. This port is built on
     * `std::vector<T>`, which constrains it, and **both** constraints bite as soon as a
     * concrete pool is instantiated rather than when a particular member is called: `Rent`
     * and `Return` are `virtual`, so their bodies are instantiated for the vtable. Naming
     * `ArrayPool<T>` (a pointer or reference, a typedef) stays legal for any `T`; reaching
     * `Shared()`, `Create()` or any subclass object requires
     * - **default-constructible** — `Rent` returns a value-initialized `std::vector<T>`, and
     *   `Return(array, true)` overwrites with a value-initialized `T`;
     * - **copy-assignable** — `Return(array, true)` assigns that value over each element.
     *
     * Both are `static_assert`ed where they are already enforced, so the same set of
     * programs compiles as before and only the diagnostic changes.
     * Ticket #2054 / SR-AUD-070, family B-C; see docs/BuffersNamespaceReviewPlan.md §4.4.
     */
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
        /**
         * @brief Returns a rented buffer back to the pool, optionally clearing its contents.
         *
         * @warning **This is not .NET's ownership transfer.** `Rent` hands back a
         * `std::vector<T>` **by value**, so the caller owns that storage and keeps owning it
         * after `Return`: continuing to read or write the vector afterwards is legal here and
         * forbidden in .NET, and this implementation does not retain the buffer for reuse.
         * `Return` is therefore advisory, and @p clearArray zeroes *the caller's own vector*
         * rather than scrubbing something the pool is about to hand to someone else. Do not
         * rely on it as a guarantee that sensitive data has left the process.
         *
         * @param array      The buffer to return; it remains owned by the caller.
         * @param clearArray If true, value-initializes every element of @p array first.
         * @note Requires T to be default-constructible and copy-assignable; because this
         *       member is virtual, the requirement applies to every instantiation of the
         *       pool, not only to calls that pass @p clearArray as true. See the
         *       class-level "Requirements on T" note.
         */
        virtual void Return(std::vector<T>& array, bool clearArray = false) {
            static_assert(
                std::is_default_constructible_v<T>,
                "System::Buffers::ArrayPool<T> requires T to be default-constructible: "
                "Return(array, true) overwrites every element with a value-initialized T. "
                "Return is virtual, so this applies to every pool instantiation. "
                "See docs/BuffersNamespaceReviewPlan.md, ticket #2054.");
            static_assert(
                std::is_copy_assignable_v<T>,
                "System::Buffers::ArrayPool<T> requires T to be copy-assignable: "
                "Return(array, true) assigns a value-initialized T over every element. "
                "Return is virtual, so this applies to every pool instantiation. "
                "See docs/BuffersNamespaceReviewPlan.md, ticket #2054.");
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
         * @note Requires T to be default-constructible; see ArrayPool's "Requirements on T".
         */
        std::vector<T> Rent(intcs minimumLength) override {
            static_assert(
                std::is_default_constructible_v<T>,
                "System::Buffers::ArrayPool<T>::Shared()/Create() require T to be "
                "default-constructible: Rent returns a value-initialized std::vector<T>. "
                "Rent is virtual, so this applies to every pool instantiation. "
                "See docs/BuffersNamespaceReviewPlan.md, ticket #2054.");
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
