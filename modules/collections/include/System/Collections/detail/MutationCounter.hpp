// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <type_traits>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace SharpRuntime::Testing
{
    /**
     * @brief Test-only access seam for a collection's private mutation counter.
     *
     * Declared here and befriended by `System::Collections::detail::BasicMutationCounter`
     * and by every collection that owns one; **never defined in production code**, so no
     * production translation unit can name a complete type and nothing can call it. A
     * regression that needs the counter positioned near a boundary specialises this in its
     * own translation unit, exactly as ticket #1786 did with
     * `SharpRuntime::Testing::SortedSetVersionAccess<T>`.
     *
     * A friend declaration changes no object layout, no signature, and no mangled symbol;
     * that is measured, not assumed (`docs/CollectionVersionCounterSweep.md` §12).
     *
     * @tparam TOwner The collection (or counter) whose private state the seam reaches.
     */
    template <typename TOwner>
    struct CollectionVersionAccess;
}

namespace System::Collections
{
    namespace detail
    {
        /**
         * @brief The shared mutation counter behind every fail-fast enumerator in this
         *        module.
         *
         * A collection bumps this on each *effective* structural mutation; an enumerator
         * snapshots it at construction and compares the snapshot for equality before
         * touching the collection's storage, so a mutation that happened underneath an
         * outstanding enumerator is reported as
         * `System::InvalidOperationException` rather than read as valid memory. That is the
         * fail-fast contract ticket 1713 introduced and ticket #1787 preserves exactly.
         *
         * The type exists because a bare integer field got three separate things wrong,
         * each reproduced against the pre-#1787 headers before anything changed
         * (`docs/CollectionVersionCounterSweep.md` §4):
         *
         * 1. **Signed-overflow undefined behaviour.** The field was
         *    `SharpRuntime::intcs`, and `++version_` at `INTCS_MAX` is undefined behaviour
         *    in C++ -- fourteen distinct UBSan reports, one per collection. `value_type` is
         *    therefore unsigned, and unsigned arithmetic is modulo 2^N by definition
         *    ([basic.fundamental]), so the increment is defined for **every** representable
         *    prior value including the maximum, where it yields 0.
         * 2. **Snapshot reuse (ABA).** A 32-bit counter returns to a value an outstanding
         *    enumerator captured after 2^32 effective mutations, and the equality guard
         *    then silently accepts a stale enumerator. The 64-bit alias
         *    (`MutationCounter`) pushes that to 2^64 -- centuries of uninterrupted
         *    mutation of one instance -- rather than pretending it is impossible.
         * 3. **Assignment transplanting the counter.** This is the one that needs no
         *    overflow at all. With a bare integer field the implicitly declared copy/move
         *    assignment operator copies the *source's* counter into the destination, so an
         *    enumerator outstanding over the destination sees no change even though every
         *    element it could refer to was just destroyed. Six of these were reproduced as
         *    AddressSanitizer heap-use-after-free / heap-buffer-overflow errors, not merely
         *    as wrong answers.
         *
         * The third point is why this is a class rather than a type alias: **assignment
         * advances the destination's own counter instead of taking the source's value.**
         * Copy and move *construction* keep copying the value, matching .NET's
         * `ArrayList.Clone`/`Hashtable.Clone`, which do the same (`ArrayList.cs:229`,
         * `Hashtable.cs:450`) -- a freshly constructed object cannot have an enumerator
         * outstanding over it, so there is nothing to invalidate.
         *
         * Self-assignment (`c = c`) also advances the counter. That is deliberate and
         * conservative: member-wise self-assignment of the backing `std::unordered_map` or
         * `std::unordered_set` is permitted to reallocate the nodes an outstanding
         * iterator points at, so failing fast is the memory-safe answer. .NET has no
         * analogue to compare against, because its collections are reference types and
         * cannot be assigned at all.
         *
         * @note This is a private implementation detail of the collection headers. It is
         *       never part of a public signature, never appears in a mangled name, and
         *       occupies the padding the surrounding class already had -- every affected
         *       container's and enumerator's `sizeof` and `alignof` are unchanged
         *       (`docs/CollectionVersionCounterSweep.md` §12).
         *
         * @tparam TValue The unsigned integral representation. Use the `MutationCounter`
         *                alias unless the owning class has provably no room for eight
         *                bytes, in which case `NarrowMutationCounter` documents the
         *                constraint explicitly.
         */
        template <typename TValue>
        class BasicMutationCounter
        {
            static_assert(std::is_unsigned_v<TValue>,
                          "A mutation counter must be unsigned so that its increment is "
                          "defined for every representable prior value.");

            TValue value_ = 0;

            /**
             * Grants the test-only seam access to @c value_ so a regression can position
             * the counter at a boundary instead of performing billions of real mutations.
             * The primary template is never defined outside a test translation unit.
             */
            template <typename>
            friend struct SharpRuntime::Testing::CollectionVersionAccess;

        public:
            /** @brief The unsigned representation an enumerator snapshots. */
            using value_type = TValue;

            /** @brief Starts a new counter at zero. */
            constexpr BasicMutationCounter() noexcept = default;

            /**
             * @brief Copies the value into a newly constructed counter.
             *
             * A copy-constructed collection is a fresh object with no enumerator
             * outstanding over it, so inheriting the value is harmless and matches .NET's
             * `Clone` implementations.
             */
            constexpr BasicMutationCounter(const BasicMutationCounter&) noexcept = default;

            /** @brief Move construction behaves exactly like copy construction. */
            constexpr BasicMutationCounter(BasicMutationCounter&&) noexcept = default;

            /** @brief Trivial destruction; the counter owns nothing. */
            ~BasicMutationCounter() = default;

            /**
             * @brief Advances this counter rather than taking @p other's value.
             *
             * Assigning a whole collection replaces every element in the destination, which
             * must invalidate every enumerator outstanding over the destination. Copying
             * the source's counter would instead make those enumerators appear valid over
             * storage that no longer exists.
             *
             * @return A reference to this counter.
             */
            constexpr BasicMutationCounter& operator=(const BasicMutationCounter&) noexcept
            {
                ++value_;
                return *this;
            }

            /** @brief Move assignment invalidates the destination exactly as copy does. */
            constexpr BasicMutationCounter& operator=(BasicMutationCounter&&) noexcept
            {
                ++value_;
                return *this;
            }

            /**
             * @brief Records one effective structural mutation.
             *
             * Defined for every representable prior value; at the maximum it wraps to zero,
             * which is modular unsigned arithmetic rather than undefined behaviour.
             *
             * @return A reference to this counter.
             */
            constexpr BasicMutationCounter& operator++() noexcept
            {
                ++value_;
                return *this;
            }

            /**
             * @brief Reads the current value, for an enumerator taking a snapshot or
             *        comparing one.
             *
             * Deliberately implicit: it keeps every existing snapshot initialisation and
             * equality comparison in the collection headers spelled exactly as it was when
             * the field was a bare integer.
             */
            constexpr operator value_type() const noexcept { return value_; }

            /**
             * @brief Reads the current value explicitly.
             * @return The number of effective mutations modulo 2^N.
             */
            [[nodiscard]] constexpr value_type getValueProperty() const noexcept
            {
                return value_;
            }
        };

        /**
         * @brief The 64-bit mutation counter every collection should use.
         *
         * Reaching a repeated value takes 2^64 effective mutations on one instance.
         */
        using MutationCounter = BasicMutationCounter<SharpRuntime::ulongcs>;

        /** @brief The value type an enumerator stores when it snapshots a MutationCounter. */
        using MutationVersion = MutationCounter::value_type;

        /**
         * @brief A 32-bit mutation counter, for the two classes whose measured object
         *        layout has no room for eight bytes.
         *
         * `LinkedList<T>`'s counter shares its last eight bytes with `count_`, and
         * `BitArray::Enumerator`'s snapshot shares its tail with three further members;
         * widening either grows a public object (`sizeof(LinkedList<int>)` 40 → 48,
         * `sizeof(BitArray::Enumerator)` 32 → 40), which needs explicit user approval this
         * repository has not been given. They therefore keep a 32-bit counter, which still
         * removes the signed-overflow undefined behaviour and the assignment transplant,
         * but leaves the 2^32 snapshot-reuse horizon open -- tracked by the blocked
         * follow-up tickets recorded in `docs/CollectionVersionCounterSweep.md` §8.
         *
         * Do not use this alias for a new collection. Use MutationCounter.
         */
        using NarrowMutationCounter = BasicMutationCounter<SharpRuntime::uintcs>;

        /** @brief The value type an enumerator stores when it snapshots a NarrowMutationCounter. */
        using NarrowMutationVersion = NarrowMutationCounter::value_type;
    }
}
