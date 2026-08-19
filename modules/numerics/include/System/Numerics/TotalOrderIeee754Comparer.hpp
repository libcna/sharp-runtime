// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <bit>
#include <cstdint>
#include <type_traits>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Collections/Generic/IComparer.hpp"
#include "System/Collections/Generic/IEqualityComparer.hpp"
#include "System/Half.hpp"

namespace System::Numerics {

    namespace Detail {
        /**
         * IEEE 754 floating-point numbers use sign-magnitude, not two's complement, so when both
         * operands are negative (as signed integers of the same width as the float), the ordinary
         * integer order is the reverse of totalOrder; ties fall out of the equality check either way.
         */
        template<typename TInt>
        [[nodiscard]] inline SharpRuntime::intcs CompareTotalOrderInteger(TInt x, TInt y) noexcept
        {
            if (x < 0 && y < 0)
                return y < x ? -1 : (y > x ? 1 : 0);
            return x < y ? -1 : (x > y ? 1 : 0);
        }

        /**
         * Total-order equality: the bit patterns are identical. Equivalent to
         * `CompareTotalOrderInteger(x, y) == 0` for every input, including every NaN payload and
         * both signed zeros, but written directly because the equality question does not need the
         * sign-magnitude reversal that ordering does.
         */
        template<typename TInt>
        [[nodiscard]] inline bool EqualsTotalOrderInteger(TInt x, TInt y) noexcept
        {
            return x == y;
        }

        /**
         * Total-order hash: the bit pattern, widened or narrowed to intcs. Equal total-order
         * values have identical bit patterns and therefore identical hashes, which is the
         * contract a hash-based collection needs. -0 and +0 differ, and so do distinct NaN
         * payloads -- deliberately, because that is exactly what total order distinguishes and
         * what ordinary floating equality would lose.
         */
        template<typename TInt>
        [[nodiscard]] inline SharpRuntime::intcs HashTotalOrderInteger(TInt x) noexcept
        {
            using TUnsigned = std::make_unsigned_t<TInt>;
            const auto bits = static_cast<TUnsigned>(x);
            if constexpr (sizeof(TInt) > sizeof(SharpRuntime::intcs)) {
                // Fold the wide pattern rather than truncating it, so binary64 values that differ
                // only in their high half do not collide. Accumulated unsigned: the XOR and the
                // shift are defined for every pattern, and one conversion happens at the end.
                const auto folded = static_cast<uint32_t>(bits ^ (bits >> 32));
                return static_cast<SharpRuntime::intcs>(folded);
            } else {
                return static_cast<SharpRuntime::intcs>(static_cast<uint32_t>(bits));
            }
        }
    }

    /**
     * @brief Compares floating-point numbers using the IEEE 754 totalOrder predicate.
     *
     * C++ counterpart of .NET System.Numerics.TotalOrderIeee754Comparer<T>. .NET constrains T to
     * IFloatingPointIeee754<T> via generic math and dispatches on typeof(T) at the call site; this
     * port instead specializes directly for the IEEE 754 binary floating-point types this runtime
     * supports (float, double, System::Half).
     *
     * @note Equality contract (SR-AUD-042, tickets #2169 and #2170). .NET's comparer implements
     * `IComparer<T>`, `IEqualityComparer<T>` and `IEquatable<TotalOrderIeee754Comparer<T>>`
     * (`TotalOrderIeee754Comparer.cs:16`), defining equality as `Compare(x, y) == 0`. #2169
     * delivered the equality *semantics*; **#2170 added `IEqualityComparer<T>` as a second base**,
     * so a specialization can now be passed wherever that interface is required. The cost, granted
     * per action on 2026-08-19, is a second vptr: `sizeof` 8 -> 16, with the `IEqualityComparer<T>`
     * subobject at offset 8. Consumers must rebuild.
     *
     * @note The 8 -> 16 growth is a **C++ artifact with no .NET counterpart**. .NET's comparer is a
     * `readonly struct` and implementing an interface costs a struct no storage at all, so .NET
     * pays nothing for the same surface. In C++ the only way to obtain polymorphic binding is a
     * base class, and a base class with virtual members is a vtable pointer.
     *
     * @note `IEquatable<TotalOrderIeee754Comparer<T>>` is deliberately **not** reproduced. .NET's
     * implementation is `Equals(TotalOrderIeee754Comparer<T> other) => true` -- every instance of a
     * stateless comparer is equal to every other -- which in C++ is what a defaulted `operator==`
     * on an empty type already expresses. Adding a third base to say it would buy nothing and cost
     * a third vptr.
     *
     * @note **`GetHashCode` diverges from .NET, and the divergence is a DECISION** — ticket
     * **#2392**, decided 2026-08-19. .NET's is `obj.GetHashCode()`
     * (`TotalOrderIeee754Comparer.cs:198-202`), i.e. the *value's own* hash, which
     * `Double.GetHashCode` normalizes so that, in its own comment, "all NaNs and both zeros have
     * the same hash code" — and **this port's `Double::GetHashCode` already matches .NET exactly**,
     * normalization included. This comparer instead hashes the **bit pattern**:
     *
     * | | .NET | this port |
     * |---|---|---|
     * | `GetHashCode(-0.0) == GetHashCode(+0.0)` | `true` | `false` |
     * | two distinct NaN payloads hash equal | `true` | `false` |
     *
     * **The hash contract holds either way**, and that is what makes the choice available:
     * equality here *is* bit-pattern identity, so two values that compare equal always have
     * identical patterns and therefore identical hashes. .NET's is simply **coarser** — it
     * collides values this comparer distinguishes. Neither is wrong; .NET's loses distribution
     * that total order exists to provide.
     *
     * Adopting .NET's was offered and **declined**: it would invert five shipped pins and remove a
     * distinction the total-order predicate is defined to make. The finer hash is therefore kept
     * **deliberately**, not by omission, and the pins below are what say so.
     */
    template<typename T>
    struct TotalOrderIeee754Comparer;

    /** @brief float specialization: totalOrder over the IEEE 754 binary32 bit pattern. */
    template<>
    struct TotalOrderIeee754Comparer<float>
        : System::Collections::Generic::IComparer<float>
        , System::Collections::Generic::IEqualityComparer<float>
    {
        [[nodiscard]] SharpRuntime::intcs Compare(const float& x, const float& y) const override
        {
            return Detail::CompareTotalOrderInteger(std::bit_cast<int32_t>(x), std::bit_cast<int32_t>(y));
        }

        /**
         * @brief Determines whether @p x and @p y are equal under the totalOrder predicate.
         *
         * Equivalent to `Compare(x, y) == 0`. Distinguishes -0 from +0 and distinct NaN payloads
         * from each other, which ordinary floating equality cannot.
         *
         * Signature note (#2169 / #2170): this `override`s
         * `System::Collections::Generic::IEqualityComparer<float>::Equals`. #2169 wrote the signature
         * to match exactly so that #2170 would be an `override` rather than a silent overload; it
         * is now virtual, and the second vptr that buys is the 8 -> 16 growth #2170 was granted.
         */
        [[nodiscard]] bool Equals(const float& x, const float& y) const override
        {
            return Detail::EqualsTotalOrderInteger(std::bit_cast<int32_t>(x), std::bit_cast<int32_t>(y));
        }

        /**
         * @brief Returns a hash code consistent with this comparer's equality.
         *
         * Values that compare equal under totalOrder have identical binary32 bit patterns and therefore
         * identical hashes. -0 and +0 hash differently, and so do distinct NaN payloads -- which is
         * where this port and .NET part company; see the type-level note. Same signature note as
         * Equals.
         */
        [[nodiscard]] SharpRuntime::intcs GetHashCode(const float& obj) const override
        {
            return Detail::HashTotalOrderInteger(std::bit_cast<int32_t>(obj));
        }
    };

    /** @brief double specialization: totalOrder over the IEEE 754 binary64 bit pattern. */
    template<>
    struct TotalOrderIeee754Comparer<double>
        : System::Collections::Generic::IComparer<double>
        , System::Collections::Generic::IEqualityComparer<double>
    {
        [[nodiscard]] SharpRuntime::intcs Compare(const double& x, const double& y) const override
        {
            return Detail::CompareTotalOrderInteger(std::bit_cast<int64_t>(x), std::bit_cast<int64_t>(y));
        }

        /**
         * @brief Determines whether @p x and @p y are equal under the totalOrder predicate.
         *
         * Equivalent to `Compare(x, y) == 0`. Distinguishes -0 from +0 and distinct NaN payloads
         * from each other, which ordinary floating equality cannot.
         *
         * Signature note (#2169 / #2170): this `override`s
         * `System::Collections::Generic::IEqualityComparer<double>::Equals`. #2169 wrote the signature
         * to match exactly so that #2170 would be an `override` rather than a silent overload; it
         * is now virtual, and the second vptr that buys is the 8 -> 16 growth #2170 was granted.
         */
        [[nodiscard]] bool Equals(const double& x, const double& y) const override
        {
            return Detail::EqualsTotalOrderInteger(std::bit_cast<int64_t>(x), std::bit_cast<int64_t>(y));
        }

        /**
         * @brief Returns a hash code consistent with this comparer's equality.
         *
         * Values that compare equal under totalOrder have identical binary64 bit patterns and therefore
         * identical hashes. -0 and +0 hash differently, and so do distinct NaN payloads -- which is
         * where this port and .NET part company; see the type-level note. Same signature note as
         * Equals.
         */
        [[nodiscard]] SharpRuntime::intcs GetHashCode(const double& obj) const override
        {
            return Detail::HashTotalOrderInteger(std::bit_cast<int64_t>(obj));
        }
    };

    /** @brief System::Half specialization: totalOrder over the IEEE 754 binary16 bit pattern. */
    template<>
    struct TotalOrderIeee754Comparer<System::Half>
        : System::Collections::Generic::IComparer<System::Half>
        , System::Collections::Generic::IEqualityComparer<System::Half>
    {
        [[nodiscard]] SharpRuntime::intcs Compare(const System::Half& x, const System::Half& y) const override
        {
            return Detail::CompareTotalOrderInteger(static_cast<int16_t>(x.bits), static_cast<int16_t>(y.bits));
        }

        /**
         * @brief Determines whether @p x and @p y are equal under the totalOrder predicate.
         *
         * Equivalent to `Compare(x, y) == 0`. Distinguishes -0 from +0 and distinct NaN payloads
         * from each other, which ordinary floating equality cannot.
         *
         * Signature note (#2169 / #2170): this `override`s
         * `System::Collections::Generic::IEqualityComparer<System::Half>::Equals`. #2169 wrote the signature
         * to match exactly so that #2170 would be an `override` rather than a silent overload; it
         * is now virtual, and the second vptr that buys is the 8 -> 16 growth #2170 was granted.
         */
        [[nodiscard]] bool Equals(const System::Half& x, const System::Half& y) const override
        {
            return Detail::EqualsTotalOrderInteger(static_cast<int16_t>(x.bits), static_cast<int16_t>(y.bits));
        }

        /**
         * @brief Returns a hash code consistent with this comparer's equality.
         *
         * Values that compare equal under totalOrder have identical binary16 bit patterns and therefore
         * identical hashes. -0 and +0 hash differently, and so do distinct NaN payloads -- which is
         * where this port and .NET part company; see the type-level note. Same signature note as
         * Equals.
         */
        [[nodiscard]] SharpRuntime::intcs GetHashCode(const System::Half& obj) const override
        {
            return Detail::HashTotalOrderInteger(static_cast<int16_t>(obj.bits));
        }
    };

} // namespace System::Numerics
