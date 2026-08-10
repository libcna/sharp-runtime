// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Stub interfaces from System.Numerics generic math (.NET 7+).
// C++ templates naturally cover the same use cases; these stubs exist for API name compatibility.
//
// SR-AUD-278 / ticket #2168 — why every static member below is `= delete`:
//
// Generic-math *conformance* is a settled, documented reduction in this runtime. `System/Half.hpp`
// states it ("Generic math interface conformance (INumber<Half>, IFloatingPointIeee754<Half>,
// IMinMaxValue<Half>, etc.) is out of scope, consistent with this codebase's position on C#
// generic-math machinery elsewhere"), `System/Numerics/DivisionRounding.hpp` states it, and the
// preamble above states it. The defect SR-AUD-278 recorded was not the reduction — it was the
// *shape* of the reduction: 44 static members across 9 interface templates were **declared and
// never defined anywhere**, so a consumer that called one compiled cleanly and then failed at
// final link with an unresolved reference to a mangled template specialisation, arbitrarily far
// from the call site and with nothing to point at.
//
// Deleting them moves that failure from the linker to the call, where the compiler can name the
// member and the caller can see it. Nothing that builds today is affected: a call that exists
// today already fails to link, so no program that currently links calls one.
//
// The C++ replacement for each family is named on the interface. Deriving from these interfaces
// and *defining* your own statics is unaffected — a derived type's own member hides the base's.
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Numerics {

    using SharpRuntime::intcs;

    /** Base interface for all numeric types, mirroring .NET INumberBase<TSelf>. */
    template<typename TSelf>
    struct INumberBase {
        static constexpr intcs Radix = 2; ///< Radix (base) of the numeric representation.
        virtual ~INumberBase() = default;
    };

    /** Interface for general numeric types, mirroring .NET INumber<TSelf>. */
    template<typename TSelf>
    struct INumber : INumberBase<TSelf> {};

    /** Interface for signed numeric types, mirroring .NET ISignedNumber<TSelf>. */
    template<typename TSelf>
    struct ISignedNumber : INumber<TSelf> {};

    /** Interface for unsigned numeric types, mirroring .NET IUnsignedNumber<TSelf>. */
    template<typename TSelf>
    struct IUnsignedNumber : INumber<TSelf> {};

    /** Interface for floating-point numeric types, mirroring .NET IFloatingPoint<TSelf>. */
    template<typename TSelf>
    struct IFloatingPoint : INumber<TSelf> {};

    /** Interface for IEEE 754 floating-point types, mirroring .NET IFloatingPointIeee754<TSelf>. */
    template<typename TSelf>
    struct IFloatingPointIeee754 : IFloatingPoint<TSelf> {};

    /** Interface for binary integer types, mirroring .NET IBinaryInteger<TSelf>. */
    template<typename TSelf>
    struct IBinaryInteger : INumber<TSelf> {};

    /** Interface for binary numeric types, mirroring .NET IBinaryNumber<TSelf>. */
    template<typename TSelf>
    struct IBinaryNumber : INumberBase<TSelf> {};

    // Arithmetic operator interfaces (C++ satisfies these implicitly via operators)

    /** Stub interface for addition operators, mirroring .NET IAdditionOperators<TLeft,TRight,TResult>. */
    template<typename TLeft, typename TRight, typename TResult>
    struct IAdditionOperators {};

    /** Stub interface for subtraction operators, mirroring .NET ISubtractionOperators<TLeft,TRight,TResult>. */
    template<typename TLeft, typename TRight, typename TResult>
    struct ISubtractionOperators {};

    /** Stub interface for multiplication operators, mirroring .NET IMultiplyOperators<TLeft,TRight,TResult>. */
    template<typename TLeft, typename TRight, typename TResult>
    struct IMultiplyOperators {};

    /** Stub interface for division operators, mirroring .NET IDivisionOperators<TLeft,TRight,TResult>. */
    template<typename TLeft, typename TRight, typename TResult>
    struct IDivisionOperators {};

    /** Stub interface for modulus operators, mirroring .NET IModulusOperators<TLeft,TRight,TResult>. */
    template<typename TLeft, typename TRight, typename TResult>
    struct IModulusOperators {};

    /** Stub interface for unary negation, mirroring .NET IUnaryNegationOperators<TSelf>. */
    template<typename TSelf>
    struct IUnaryNegationOperators {};

    /** Stub interface for unary plus, mirroring .NET IUnaryPlusOperators<TSelf>. */
    template<typename TSelf>
    struct IUnaryPlusOperators {};

    /** Stub interface for comparison operators, mirroring .NET IComparisonOperators<TLeft,TRight>. */
    template<typename TLeft, typename TRight>
    struct IComparisonOperators {};

    /** Stub interface for bitwise operators, mirroring .NET IBitwiseOperators<TLeft,TRight,TResult>. */
    template<typename TLeft, typename TRight, typename TResult>
    struct IBitwiseOperators {};

    /** Stub interface for shift operators, mirroring .NET IShiftOperators<TLeft,TRight,TResult>. */
    template<typename TLeft, typename TRight, typename TResult>
    struct IShiftOperators {};

    /** Stub interface for increment operators, mirroring .NET IIncrementOperators<TSelf>. */
    template<typename TSelf>
    struct IIncrementOperators {};

    /** Stub interface for decrement operators, mirroring .NET IDecrementOperators<TSelf>. */
    template<typename TSelf>
    struct IDecrementOperators {};

    /**
     * Stub interface exposing MinValue/MaxValue constants, mirroring .NET IMinMaxValue<TSelf>.
     * Both members are deleted (SR-AUD-278, #2168); use `std::numeric_limits<TSelf>::lowest()`
     * and `std::numeric_limits<TSelf>::max()`, or this runtime's own `System::Int32::MinValue`
     * family, instead.
     */
    template<typename TSelf>
    struct IMinMaxValue {
        static TSelf MinValue() = delete; ///< **Deleted.** Use std::numeric_limits<TSelf>::lowest().
        static TSelf MaxValue() = delete; ///< **Deleted.** Use std::numeric_limits<TSelf>::max().
    };

    /**
     * Stub interface exposing the additive identity, mirroring .NET IAdditiveIdentity<TSelf>.
     * The member is deleted (SR-AUD-278, #2168); write `TSelf{}` instead.
     */
    template<typename TSelf>
    struct IAdditiveIdentity {
        static TSelf AdditiveIdentity() = delete; ///< **Deleted.** Write TSelf{}.
    };

    /**
     * Stub interface exposing the multiplicative identity, mirroring .NET
     * IMultiplicativeIdentity<TSelf>. The member is deleted (SR-AUD-278, #2168); write
     * `TSelf{1}` instead.
     */
    template<typename TSelf>
    struct IMultiplicativeIdentity {
        static TSelf MultiplicativeIdentity() = delete; ///< **Deleted.** Write TSelf{1}.
    };

    /** Stub interface for equality operators, mirroring .NET IEqualityOperators<TSelf,TOther,TResult>. */
    template<typename TSelf, typename TOther, typename TResult>
    struct IEqualityOperators {};

    /**
     * Stub interface for the trigonometric family, mirroring .NET ITrigonometricFunctions<TSelf>.
     * Every member is deleted (SR-AUD-278, #2168). Use `<cmath>`: `std::acos`, `std::asin`,
     * `std::atan`, `std::cos`, `std::sin`, `std::tan`. The `*Pi` variants are `f(x) / pi` for the
     * inverse functions and `f(x * pi)` for the direct ones.
     */
    template<typename TSelf>
    struct ITrigonometricFunctions {
        static TSelf Acos(TSelf x) = delete;   ///< **Deleted.** Returns the arc cosine of x, in radians.
        static TSelf AcosPi(TSelf x) = delete; ///< **Deleted.** Returns the arc cosine of x, divided by pi.
        static TSelf Asin(TSelf x) = delete;   ///< **Deleted.** Returns the arc sine of x, in radians.
        static TSelf AsinPi(TSelf x) = delete; ///< **Deleted.** Returns the arc sine of x, divided by pi.
        static TSelf Atan(TSelf x) = delete;   ///< **Deleted.** Returns the arc tangent of x, in radians.
        static TSelf AtanPi(TSelf x) = delete; ///< **Deleted.** Returns the arc tangent of x, divided by pi.
        static TSelf Cos(TSelf x) = delete;    ///< **Deleted.** Returns the cosine of x (radians).
        static TSelf CosPi(TSelf x) = delete;  ///< **Deleted.** Returns the cosine of x*pi.
        static TSelf Sin(TSelf x) = delete;    ///< **Deleted.** Returns the sine of x (radians).
        static TSelf SinPi(TSelf x) = delete;  ///< **Deleted.** Returns the sine of x*pi.
        static TSelf Tan(TSelf x) = delete;    ///< **Deleted.** Returns the tangent of x (radians).
        static TSelf TanPi(TSelf x) = delete;  ///< **Deleted.** Returns the tangent of x*pi.
    };

    /**
     * Stub interface for the hyperbolic family, mirroring .NET IHyperbolicFunctions<TSelf>.
     * Every member is deleted (SR-AUD-278, #2168). Use `<cmath>`: `std::acosh`, `std::asinh`,
     * `std::atanh`, `std::cosh`, `std::sinh`, `std::tanh`.
     */
    template<typename TSelf>
    struct IHyperbolicFunctions {
        static TSelf Acosh(TSelf x) = delete; ///< **Deleted.** Returns the inverse hyperbolic cosine of x.
        static TSelf Asinh(TSelf x) = delete; ///< **Deleted.** Returns the inverse hyperbolic sine of x.
        static TSelf Atanh(TSelf x) = delete; ///< **Deleted.** Returns the inverse hyperbolic tangent of x.
        static TSelf Cosh(TSelf x) = delete;  ///< **Deleted.** Returns the hyperbolic cosine of x.
        static TSelf Sinh(TSelf x) = delete;  ///< **Deleted.** Returns the hyperbolic sine of x.
        static TSelf Tanh(TSelf x) = delete;  ///< **Deleted.** Returns the hyperbolic tangent of x.
    };

    /**
     * Stub interface for the logarithmic family, mirroring .NET ILogarithmicFunctions<TSelf>.
     * Every member is deleted (SR-AUD-278, #2168). Use `<cmath>`: `std::log`, `std::log2`,
     * `std::log10`; a base-`b` logarithm is `std::log(x) / std::log(b)`.
     */
    template<typename TSelf>
    struct ILogarithmicFunctions {
        static TSelf Log(TSelf x) = delete;                  ///< **Deleted.** Returns the natural (base e) logarithm of x.
        static TSelf Log(TSelf x, TSelf newBase) = delete;    ///< **Deleted.** Returns the logarithm of x in the given base.
        static TSelf Log2(TSelf x) = delete;                  ///< **Deleted.** Returns the base-2 logarithm of x.
        static TSelf Log10(TSelf x) = delete;                 ///< **Deleted.** Returns the base-10 logarithm of x.
    };

    /**
     * Stub interface for the exponential family, mirroring .NET IExponentialFunctions<TSelf>.
     * Every member is deleted (SR-AUD-278, #2168). Use `<cmath>`: `std::exp`, `std::exp2`, and
     * `std::pow(TSelf{10}, x)`.
     */
    template<typename TSelf>
    struct IExponentialFunctions {
        static TSelf Exp(TSelf x) = delete;   ///< **Deleted.** Returns e raised to the power x.
        static TSelf Exp2(TSelf x) = delete;  ///< **Deleted.** Returns 2 raised to the power x.
        static TSelf Exp10(TSelf x) = delete; ///< **Deleted.** Returns 10 raised to the power x.
    };

    /**
     * Stub interface for the power family, mirroring .NET IPowerFunctions<TSelf>.
     * The member is deleted (SR-AUD-278, #2168). Use `std::pow` from `<cmath>`.
     */
    template<typename TSelf>
    struct IPowerFunctions {
        static TSelf Pow(TSelf x, TSelf y) = delete; ///< **Deleted.** Returns x raised to the power y.
    };

    /**
     * Stub interface for the root family, mirroring .NET IRootFunctions<TSelf>.
     * Every member is deleted (SR-AUD-278, #2168). Use `<cmath>`: `std::cbrt`, `std::hypot`,
     * `std::sqrt`. `RootN` has no standard-library spelling, and its sign convention for a
     * negative operand with an odd `n` is not measured against .NET here (#2174), which is a
     * second reason not to invent a definition for it.
     */
    template<typename TSelf>
    struct IRootFunctions {
        static TSelf Cbrt(TSelf x) = delete;            ///< **Deleted.** Returns the cube root of x.
        static TSelf Hypot(TSelf x, TSelf y) = delete;  ///< **Deleted.** Returns sqrt(x*x + y*y), computed without undue overflow/underflow.
        static TSelf RootN(TSelf x, intcs n) = delete;  ///< **Deleted.** Returns the n-th root of x.
        static TSelf Sqrt(TSelf x) = delete;            ///< **Deleted.** Returns the square root of x.
    };

    /**
     * Stub interface for the floating-point constants, mirroring .NET
     * IFloatingPointConstants<TSelf>. Every member is deleted (SR-AUD-278, #2168). Use
     * `<numbers>`: `std::numbers::e_v<TSelf>`, `std::numbers::pi_v<TSelf>`, and
     * `2 * std::numbers::pi_v<TSelf>`.
     */
    template<typename TSelf>
    struct IFloatingPointConstants {
        static TSelf E() = delete;   ///< **Deleted.** Returns the mathematical constant e.
        static TSelf Pi() = delete;  ///< **Deleted.** Returns the mathematical constant pi.
        static TSelf Tau() = delete; ///< **Deleted.** Returns the mathematical constant tau (2*pi).
    };

    /**
     * @brief Stub interface aggregating the IEEE 754 binary floating-point contract, mirroring
     * .NET IBinaryFloatingPointIeee754<TSelf>. Combines IFloatingPointIeee754, IFloatingPointConstants,
     * and the transcendental function families; C++ types satisfy the equivalent contract via
     * <cmath> free functions rather than interface conformance.
     */
    template<typename TSelf>
    struct IBinaryFloatingPointIeee754
        : IFloatingPointIeee754<TSelf>,
          IFloatingPointConstants<TSelf>,
          IExponentialFunctions<TSelf>,
          IHyperbolicFunctions<TSelf>,
          ILogarithmicFunctions<TSelf>,
          IPowerFunctions<TSelf>,
          IRootFunctions<TSelf>,
          ITrigonometricFunctions<TSelf>
    {};

} // namespace System::Numerics
