// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <bit>
#include <cstdint>
#include <type_traits>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Collections/Generic/IComparer.hpp"
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
     * both `IComparer<T>` and `IEqualityComparer<T>`, defining equality as `Compare(x, y) == 0`.
     * Each specialization here provides `Equals` and `GetHashCode` with exactly the signatures
     * `System::Collections::Generic::IEqualityComparer<T>` declares, so the total-order semantics
     * -- which distinguish -0 from +0 and one NaN payload from another -- are available to a
     * caller. What is **not** available is *polymorphic binding*: these specializations do not
     * inherit `IEqualityComparer<T>`, so they cannot be passed where that interface is required.
     * Adding the base would grow each specialization from 8 to 16 bytes (a second vptr), a public
     * object-layout change; it is ticket **#2170** and is gated on explicit approval. See
     * `docs/SystemNumericsNamespaceReviewPlan.md` section 6.1 for the measurement.
     */
    template<typename T>
    struct TotalOrderIeee754Comparer;

    /** @brief float specialization: totalOrder over the IEEE 754 binary32 bit pattern. */
    template<>
    struct TotalOrderIeee754Comparer<float> : System::Collections::Generic::IComparer<float>
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
         * Signature note (#2169 / #2170): this deliberately matches
         * `System::Collections::Generic::IEqualityComparer<float>::Equals` exactly, so that adding
         * that interface as a second base becomes a two-line change. It is intentionally
         * **non-virtual** today: adding the base would grow this type from 8 to 16 bytes (a second
         * vptr), a public object-layout change that needs its own approval (#2170).
         */
        [[nodiscard]] bool Equals(const float& x, const float& y) const
        {
            return Detail::EqualsTotalOrderInteger(std::bit_cast<int32_t>(x), std::bit_cast<int32_t>(y));
        }

        /**
         * @brief Returns a hash code consistent with this comparer's equality.
         *
         * Values that compare equal under totalOrder have identical binary32 bit patterns and therefore
         * identical hashes. -0 and +0 hash differently, and so do distinct NaN payloads. Same
         * signature note as Equals.
         */
        [[nodiscard]] SharpRuntime::intcs GetHashCode(const float& obj) const
        {
            return Detail::HashTotalOrderInteger(std::bit_cast<int32_t>(obj));
        }
    };

    /** @brief double specialization: totalOrder over the IEEE 754 binary64 bit pattern. */
    template<>
    struct TotalOrderIeee754Comparer<double> : System::Collections::Generic::IComparer<double>
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
         * Signature note (#2169 / #2170): this deliberately matches
         * `System::Collections::Generic::IEqualityComparer<double>::Equals` exactly, so that adding
         * that interface as a second base becomes a two-line change. It is intentionally
         * **non-virtual** today: adding the base would grow this type from 8 to 16 bytes (a second
         * vptr), a public object-layout change that needs its own approval (#2170).
         */
        [[nodiscard]] bool Equals(const double& x, const double& y) const
        {
            return Detail::EqualsTotalOrderInteger(std::bit_cast<int64_t>(x), std::bit_cast<int64_t>(y));
        }

        /**
         * @brief Returns a hash code consistent with this comparer's equality.
         *
         * Values that compare equal under totalOrder have identical binary64 bit patterns and therefore
         * identical hashes. -0 and +0 hash differently, and so do distinct NaN payloads. Same
         * signature note as Equals.
         */
        [[nodiscard]] SharpRuntime::intcs GetHashCode(const double& obj) const
        {
            return Detail::HashTotalOrderInteger(std::bit_cast<int64_t>(obj));
        }
    };

    /** @brief System::Half specialization: totalOrder over the IEEE 754 binary16 bit pattern. */
    template<>
    struct TotalOrderIeee754Comparer<System::Half> : System::Collections::Generic::IComparer<System::Half>
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
         * Signature note (#2169 / #2170): this deliberately matches
         * `System::Collections::Generic::IEqualityComparer<System::Half>::Equals` exactly, so that adding
         * that interface as a second base becomes a two-line change. It is intentionally
         * **non-virtual** today: adding the base would grow this type from 8 to 16 bytes (a second
         * vptr), a public object-layout change that needs its own approval (#2170).
         */
        [[nodiscard]] bool Equals(const System::Half& x, const System::Half& y) const
        {
            return Detail::EqualsTotalOrderInteger(static_cast<int16_t>(x.bits), static_cast<int16_t>(y.bits));
        }

        /**
         * @brief Returns a hash code consistent with this comparer's equality.
         *
         * Values that compare equal under totalOrder have identical binary16 bit patterns and therefore
         * identical hashes. -0 and +0 hash differently, and so do distinct NaN payloads. Same
         * signature note as Equals.
         */
        [[nodiscard]] SharpRuntime::intcs GetHashCode(const System::Half& obj) const
        {
            return Detail::HashTotalOrderInteger(static_cast<int16_t>(obj.bits));
        }
    };

} // namespace System::Numerics
