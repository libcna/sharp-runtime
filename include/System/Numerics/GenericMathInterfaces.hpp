// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Stub interfaces from System.Numerics generic math (.NET 7+).
// C++ templates naturally cover the same use cases; these stubs exist for API name compatibility.
#pragma once

namespace System::Numerics {

    /** Base interface for all numeric types, mirroring .NET INumberBase<TSelf>. */
    template<typename TSelf>
    struct INumberBase {
        static constexpr int Radix = 2; ///< Radix (base) of the numeric representation.
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

    /** Stub interface exposing MinValue/MaxValue constants, mirroring .NET IMinMaxValue<TSelf>. */
    template<typename TSelf>
    struct IMinMaxValue {
        static TSelf MinValue(); ///< Returns the minimum representable value of TSelf.
        static TSelf MaxValue(); ///< Returns the maximum representable value of TSelf.
    };

    /** Stub interface exposing the additive identity, mirroring .NET IAdditiveIdentity<TSelf>. */
    template<typename TSelf>
    struct IAdditiveIdentity { static TSelf AdditiveIdentity(); ///< Returns the additive identity (zero) of TSelf.
    };

    /** Stub interface exposing the multiplicative identity, mirroring .NET IMultiplicativeIdentity<TSelf>. */
    template<typename TSelf>
    struct IMultiplicativeIdentity { static TSelf MultiplicativeIdentity(); ///< Returns the multiplicative identity (one) of TSelf.
    };

    /** Stub interface for equality operators, mirroring .NET IEqualityOperators<TSelf,TOther,TResult>. */
    template<typename TSelf, typename TOther, typename TResult>
    struct IEqualityOperators {};

    /** Stub interface for trigonometric functions, mirroring .NET ITrigonometricFunctions<TSelf>. */
    template<typename TSelf>
    struct ITrigonometricFunctions {
        static TSelf Acos(TSelf x);
        static TSelf AcosPi(TSelf x);
        static TSelf Asin(TSelf x);
        static TSelf AsinPi(TSelf x);
        static TSelf Atan(TSelf x);
        static TSelf AtanPi(TSelf x);
        static TSelf Cos(TSelf x);
        static TSelf CosPi(TSelf x);
        static TSelf Sin(TSelf x);
        static TSelf SinPi(TSelf x);
        static TSelf Tan(TSelf x);
        static TSelf TanPi(TSelf x);
    };

    /** Stub interface for hyperbolic functions, mirroring .NET IHyperbolicFunctions<TSelf>. */
    template<typename TSelf>
    struct IHyperbolicFunctions {
        static TSelf Acosh(TSelf x);
        static TSelf Asinh(TSelf x);
        static TSelf Atanh(TSelf x);
        static TSelf Cosh(TSelf x);
        static TSelf Sinh(TSelf x);
        static TSelf Tanh(TSelf x);
    };

    /** Stub interface for logarithmic functions, mirroring .NET ILogarithmicFunctions<TSelf>. */
    template<typename TSelf>
    struct ILogarithmicFunctions {
        static TSelf Log(TSelf x);
        static TSelf Log(TSelf x, TSelf newBase);
        static TSelf Log2(TSelf x);
        static TSelf Log10(TSelf x);
    };

    /** Stub interface for exponential functions, mirroring .NET IExponentialFunctions<TSelf>. */
    template<typename TSelf>
    struct IExponentialFunctions {
        static TSelf Exp(TSelf x);
        static TSelf Exp2(TSelf x);
        static TSelf Exp10(TSelf x);
    };

    /** Stub interface for power functions, mirroring .NET IPowerFunctions<TSelf>. */
    template<typename TSelf>
    struct IPowerFunctions {
        static TSelf Pow(TSelf x, TSelf y);
    };

    /** Stub interface for root functions, mirroring .NET IRootFunctions<TSelf>. */
    template<typename TSelf>
    struct IRootFunctions {
        static TSelf Cbrt(TSelf x);
        static TSelf Hypot(TSelf x, TSelf y);
        static TSelf RootN(TSelf x, int n);
        static TSelf Sqrt(TSelf x);
    };

    /** Stub interface exposing E/Pi/Tau constants, mirroring .NET IFloatingPointConstants<TSelf>. */
    template<typename TSelf>
    struct IFloatingPointConstants {
        static TSelf E();   ///< Returns the mathematical constant e.
        static TSelf Pi();  ///< Returns the mathematical constant pi.
        static TSelf Tau(); ///< Returns the mathematical constant tau (2*pi).
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
