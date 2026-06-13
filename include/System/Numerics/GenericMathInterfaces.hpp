// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Stub interfaces from System.Numerics generic math (.NET 7+).
// C++ templates naturally cover the same use cases; these stubs exist for API name compatibility.
#pragma once

namespace System::Numerics {

    /// Base interface for all numeric types, mirroring .NET INumberBase<TSelf>.
    template<typename TSelf>
    struct INumberBase {
        static constexpr int Radix = 2; ///< Radix (base) of the numeric representation.
        virtual ~INumberBase() = default;
    };

    /// Interface for general numeric types, mirroring .NET INumber<TSelf>.
    template<typename TSelf>
    struct INumber : INumberBase<TSelf> {};

    /// Interface for signed numeric types, mirroring .NET ISignedNumber<TSelf>.
    template<typename TSelf>
    struct ISignedNumber : INumber<TSelf> {};

    /// Interface for unsigned numeric types, mirroring .NET IUnsignedNumber<TSelf>.
    template<typename TSelf>
    struct IUnsignedNumber : INumber<TSelf> {};

    /// Interface for floating-point numeric types, mirroring .NET IFloatingPoint<TSelf>.
    template<typename TSelf>
    struct IFloatingPoint : INumber<TSelf> {};

    /// Interface for IEEE 754 floating-point types, mirroring .NET IFloatingPointIeee754<TSelf>.
    template<typename TSelf>
    struct IFloatingPointIeee754 : IFloatingPoint<TSelf> {};

    /// Interface for binary integer types, mirroring .NET IBinaryInteger<TSelf>.
    template<typename TSelf>
    struct IBinaryInteger : INumber<TSelf> {};

    /// Interface for binary numeric types, mirroring .NET IBinaryNumber<TSelf>.
    template<typename TSelf>
    struct IBinaryNumber : INumberBase<TSelf> {};

    // Arithmetic operator interfaces (C++ satisfies these implicitly via operators)

    /// Stub interface for addition operators, mirroring .NET IAdditionOperators<TLeft,TRight,TResult>.
    template<typename TLeft, typename TRight, typename TResult>
    struct IAdditionOperators {};

    /// Stub interface for subtraction operators, mirroring .NET ISubtractionOperators<TLeft,TRight,TResult>.
    template<typename TLeft, typename TRight, typename TResult>
    struct ISubtractionOperators {};

    /// Stub interface for multiplication operators, mirroring .NET IMultiplyOperators<TLeft,TRight,TResult>.
    template<typename TLeft, typename TRight, typename TResult>
    struct IMultiplyOperators {};

    /// Stub interface for division operators, mirroring .NET IDivisionOperators<TLeft,TRight,TResult>.
    template<typename TLeft, typename TRight, typename TResult>
    struct IDivisionOperators {};

    /// Stub interface for modulus operators, mirroring .NET IModulusOperators<TLeft,TRight,TResult>.
    template<typename TLeft, typename TRight, typename TResult>
    struct IModulusOperators {};

    /// Stub interface for unary negation, mirroring .NET IUnaryNegationOperators<TSelf>.
    template<typename TSelf>
    struct IUnaryNegationOperators {};

    /// Stub interface for unary plus, mirroring .NET IUnaryPlusOperators<TSelf>.
    template<typename TSelf>
    struct IUnaryPlusOperators {};

    /// Stub interface for comparison operators, mirroring .NET IComparisonOperators<TLeft,TRight>.
    template<typename TLeft, typename TRight>
    struct IComparisonOperators {};

    /// Stub interface for bitwise operators, mirroring .NET IBitwiseOperators<TLeft,TRight,TResult>.
    template<typename TLeft, typename TRight, typename TResult>
    struct IBitwiseOperators {};

    /// Stub interface for shift operators, mirroring .NET IShiftOperators<TLeft,TRight,TResult>.
    template<typename TLeft, typename TRight, typename TResult>
    struct IShiftOperators {};

    /// Stub interface for increment operators, mirroring .NET IIncrementOperators<TSelf>.
    template<typename TSelf>
    struct IIncrementOperators {};

    /// Stub interface for decrement operators, mirroring .NET IDecrementOperators<TSelf>.
    template<typename TSelf>
    struct IDecrementOperators {};

    /// Stub interface exposing MinValue/MaxValue constants, mirroring .NET IMinMaxValue<TSelf>.
    template<typename TSelf>
    struct IMinMaxValue {
        static TSelf MinValue(); ///< Returns the minimum representable value of TSelf.
        static TSelf MaxValue(); ///< Returns the maximum representable value of TSelf.
    };

    /// Stub interface exposing the additive identity, mirroring .NET IAdditiveIdentity<TSelf>.
    template<typename TSelf>
    struct IAdditiveIdentity { static TSelf AdditiveIdentity(); ///< Returns the additive identity (zero) of TSelf.
    };

    /// Stub interface exposing the multiplicative identity, mirroring .NET IMultiplicativeIdentity<TSelf>.
    template<typename TSelf>
    struct IMultiplicativeIdentity { static TSelf MultiplicativeIdentity(); ///< Returns the multiplicative identity (one) of TSelf.
    };

} // namespace System::Numerics
