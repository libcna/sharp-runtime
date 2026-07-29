// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <type_traits>
#include <utility>

#include "System/Collections/detail/MutationCounter.hpp"

namespace System::Collections::detail
{
    /**
     * @brief The tracked result of an indexed write on a mutable collection.
     *
     * Real .NET's `List<T>` index **setter** advances `_version` unconditionally
     * (`List.cs:161-162`), so `list[i] = value` makes an in-progress enumeration fail fast.
     * A C++ `operator[]` that returns a plain `T&` cannot reproduce that: once the caller
     * holds the reference, no member call happens on assignment, so the collection is never
     * notified. That is a property of the language, not a gap in this port -- there is no
     * C++ mechanism by which a container learns of a write through a reference it previously
     * handed out (`docs/ListIndexerVersioningDesign.md` §8.3).
     *
     * This proxy is the interception point. The non-const indexer returns one **by value**;
     * it reads as a `const T&` and routes every write back through the owning collection's
     * mutation counter. It is selected and specified by design ticket #1790 §13.1 and
     * implemented by ticket #1791.
     *
     * ### What it guarantees
     *
     * - **Every tracked write advances the counter exactly once** -- `operator=` in all three
     *   forms, every compound assignment, and both forms of `++`/`--`.
     * - **An equal-value write still advances it**, matching .NET, which never compares the
     *   old value. It also avoids requiring @p T to be equality-comparable.
     * - **Reads never advance it** -- the conversion operator, `getValueProperty()`, and
     *   `operator->`.
     * - **It never publishes a `T*` or `T&` into the backing storage.** The conversion yields
     *   `const T&` and `operator->` yields `const T*`, so the reproduced use-after-free shapes
     *   of #1790 §5.3 are no longer expressible through the ordinary indexer.
     *
     * ### What it deliberately cannot do
     *
     * `operator.` is not overloadable in C++, so `list[i].member = v` and
     * `list[i].method()` do **not** compile for a value-type element. That is the principal,
     * unavoidable cost of this design, and it is the same restriction C# imposes on a
     * value-type element (CS1612). Migrate with `T copy = list.getItem(i); copy.member = v;
     * list.setItem(i, copy);` for a write, or `list.getItem(i).method()` / `list[i]->method()`
     * for a read.
     *
     * ### Lifetime
     *
     * The indexer yields a **prvalue**, valid for the full-expression that created it. It
     * stores a raw `T*` into the collection's storage and a raw `MutationCounter*`, owns
     * nothing, and allocates nothing. Storing one (`auto r = list[0];`) is legal C++ but binds
     * to a slot that any structural mutation invalidates -- exactly as the old `T&` did, and
     * with the same undefined behaviour if it is then used. Do not retain one across a
     * mutation.
     *
     * @tparam T The element type of the owning collection.
     */
    template <typename T>
    class ElementReference
    {
        T* slot_;
        MutationCounter* version_;

        /**
         * Advances the owning collection's counter exactly once when the enclosing scope
         * ends -- on the normal path *and* on the throwing one.
         *
         * The throwing path is deliberate and is a strengthening over .NET, which cannot
         * reach the case: if `T`'s own assignment throws part-way, the element is left in an
         * unspecified state, and an enumerator that did not fail fast would go on reading it
         * as though nothing had happened. Bounds are validated by the collection *before* a
         * proxy is ever constructed, so an out-of-range index cannot reach this and cannot
         * advance the counter.
         */
        struct Advance
        {
            MutationCounter* version;
            ~Advance() { ++*version; }
        };

        template <typename TApply>
        ElementReference& tracked(TApply&& apply)
        {
            const Advance advance{version_};
            apply(*slot_);
            return *this;
        }

    public:
        /** @brief The element type this proxy refers to. */
        using value_type = T;

        /**
         * @brief Binds a proxy to one already bounds-checked slot and its owner's counter.
         *
         * Called only by a collection's non-const indexer, after that indexer has validated
         * the index. Constructing a proxy performs no mutation and advances nothing.
         *
         * @param slot    The element to read and write through.
         * @param version The owning collection's mutation counter.
         */
        constexpr ElementReference(T* slot, MutationCounter* version) noexcept
            : slot_(slot), version_(version) {}

        /** @brief Copies the alias, like copying a pointer; mutates nothing. */
        constexpr ElementReference(const ElementReference&) noexcept = default;

        /** @brief Trivial destruction; the proxy owns nothing. */
        ~ElementReference() = default;

        // ---- reads: never advance the counter ---------------------------------------

        /**
         * @brief Reads the element as `const T&`.
         *
         * Deliberately implicit, so every ordinary read (`int x = list[0];`,
         * `takesConstRef(list[0])`, `list[0] + 1`) keeps the spelling it had when the
         * indexer returned a reference.
         */
        constexpr operator const T&() const noexcept { return *slot_; }

        /**
         * @brief Reads the element explicitly.
         * @return A const reference to the element, following `std::vector` invalidation
         *         rules -- valid until the next reallocating or erasing operation.
         */
        [[nodiscard]] constexpr const T& getValueProperty() const noexcept { return *slot_; }

        /**
         * @brief Read-only member access: `list[i]->constMethod()`.
         *
         * The pointer is `const T*` on purpose -- a mutable member write through the ordinary
         * indexer is exactly what this type exists to prevent.
         */
        const T* operator->() const noexcept { return slot_; }

        // ---- tracked writes: each advances the counter exactly once -------------------

        /** @brief Replaces the element and advances the owner's mutation counter. */
        ElementReference& operator=(const T& value)
        {
            return tracked([&value](T& slot) { slot = value; });
        }

        /** @brief Move-replaces the element and advances the owner's mutation counter. */
        ElementReference& operator=(T&& value)
        {
            return tracked([&value](T& slot) { slot = std::move(value); });
        }

        /**
         * @brief Assigns **through** to the element, so `list[i] = list[j]` copies the value.
         *
         * This is not a rebinding copy-assignment: a proxy never changes which slot it refers
         * to, so a proxy for one collection can never be silently substituted for a proxy for
         * another. Advances this element's owner's counter, not the source's.
         */
        ElementReference& operator=(const ElementReference& other)
        {
            const T& value = *other.slot_;
            return tracked([&value](T& slot) { slot = value; });
        }

        /**
         * @brief Assigns anything the element itself is assignable from, without materialising
         *        a temporary @p T.
         *
         * Without this, `stringList[i] = "abc"` would have to convert the `const char*` to a
         * `std::string` to reach `operator=(const T&)`, and that temporary costs **one heap
         * allocation per write** -- measured at 1.83 ns/op and 0 allocations before ticket
         * #1791 against 6.47 ns/op and one allocation per operation with only the two
         * `T`-typed overloads (`build-probe/1791_perf.log`). Forwarding straight to the
         * element's own assignment restores the old cost exactly, and is the same treatment
         * the compound assignments below already use.
         *
         * Constrained away from `ElementReference` itself so it can never displace the
         * assign-through copy-assignment above, and away from `T` so the two exact overloads
         * keep their meaning. It preserves every safety property: the write still goes through
         * `tracked()`, still advances the counter exactly once, and still publishes no alias.
         */
        template <typename U>
            requires (!std::is_same_v<std::remove_cvref_t<U>, ElementReference>)
                  && (!std::is_same_v<std::remove_cvref_t<U>, T>)
                  && std::is_assignable_v<T&, U&&>
        ElementReference& operator=(U&& value)
        {
            return tracked([&](T& slot) { slot = std::forward<U>(value); });
        }

        /** @brief Tracked `+=`. */
        template <typename U> ElementReference& operator+=(U&& rhs)
        { return tracked([&](T& s) { s += std::forward<U>(rhs); }); }
        /** @brief Tracked `-=`. */
        template <typename U> ElementReference& operator-=(U&& rhs)
        { return tracked([&](T& s) { s -= std::forward<U>(rhs); }); }
        /** @brief Tracked `*=`. */
        template <typename U> ElementReference& operator*=(U&& rhs)
        { return tracked([&](T& s) { s *= std::forward<U>(rhs); }); }
        /** @brief Tracked `/=`. */
        template <typename U> ElementReference& operator/=(U&& rhs)
        { return tracked([&](T& s) { s /= std::forward<U>(rhs); }); }
        /** @brief Tracked `%=`. */
        template <typename U> ElementReference& operator%=(U&& rhs)
        { return tracked([&](T& s) { s %= std::forward<U>(rhs); }); }
        /** @brief Tracked `&=`. */
        template <typename U> ElementReference& operator&=(U&& rhs)
        { return tracked([&](T& s) { s &= std::forward<U>(rhs); }); }
        /** @brief Tracked `|=`. */
        template <typename U> ElementReference& operator|=(U&& rhs)
        { return tracked([&](T& s) { s |= std::forward<U>(rhs); }); }
        /** @brief Tracked `^=`. */
        template <typename U> ElementReference& operator^=(U&& rhs)
        { return tracked([&](T& s) { s ^= std::forward<U>(rhs); }); }
        /** @brief Tracked `<<=`. */
        template <typename U> ElementReference& operator<<=(U&& rhs)
        { return tracked([&](T& s) { s <<= std::forward<U>(rhs); }); }
        /** @brief Tracked `>>=`. */
        template <typename U> ElementReference& operator>>=(U&& rhs)
        { return tracked([&](T& s) { s >>= std::forward<U>(rhs); }); }

        /** @brief Tracked pre-increment. */
        ElementReference& operator++() { return tracked([](T& s) { ++s; }); }
        /** @brief Tracked pre-decrement. */
        ElementReference& operator--() { return tracked([](T& s) { --s; }); }

        /**
         * @brief Tracked post-increment.
         * @return The value **before** the increment, by value -- a proxy cannot return an
         *         lvalue to a temporary, and returning the old value is what post-increment
         *         means.
         */
        T operator++(int)
        {
            T previous = *slot_;
            const Advance advance{version_};
            ++*slot_;
            return previous;
        }

        /**
         * @brief Tracked post-decrement.
         * @return The value **before** the decrement, by value.
         */
        T operator--(int)
        {
            T previous = *slot_;
            const Advance advance{version_};
            --*slot_;
            return previous;
        }

        // ---- comparison ---------------------------------------------------------------
        //
        // Hidden friends. An implicit conversion is not considered during template argument
        // deduction, so a proxy convertible to `const T&` would still fail to match, for
        // example, the `operator==(const basic_string<C,Tr,A>&, const C*)` that resolves
        // `EXPECT_EQ(list[0], "abc")`. These restore that spelling. Being hidden friends,
        // they are found by ADL only when one operand really is an ElementReference, so they
        // cannot hijack an unrelated comparison. `!=` follows from `==` in C++20.

        /** @brief Compares the referenced element with @p rhs. */
        template <typename U>
        friend bool operator==(const ElementReference& lhs, const U& rhs)
        {
            return *lhs.slot_ == rhs;
        }

        /** @brief Compares @p lhs with the referenced element. */
        template <typename U>
        friend bool operator==(const U& lhs, const ElementReference& rhs)
        {
            return lhs == *rhs.slot_;
        }

        /** @brief Compares two referenced elements by value. */
        friend bool operator==(const ElementReference& lhs, const ElementReference& rhs)
        {
            return *lhs.slot_ == *rhs.slot_;
        }

        /**
         * @brief Streams the referenced element, so a failing test prints the value rather
         *        than a hex dump of the proxy's two pointers.
         *
         * Constrained on the element actually being streamable, and a hidden friend, so it
         * participates in no unrelated overload set.
         */
        template <typename TStream>
            requires requires(TStream& stream, const T& value) { stream << value; }
        friend TStream& operator<<(TStream& stream, const ElementReference& ref)
        {
            return stream << *ref.slot_;
        }
    };
}
