// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Stub interfaces from System.Numerics generic math (.NET 7+).
// C++ templates naturally cover the same use cases; these stubs exist for API name compatibility.
#pragma once

namespace System::Numerics {

    template<typename TSelf>
    struct INumberBase {
        static constexpr int Radix = 2;
        virtual ~INumberBase() = default;
    };

    template<typename TSelf>
    struct INumber : INumberBase<TSelf> {};

    template<typename TSelf>
    struct ISignedNumber : INumber<TSelf> {};

    template<typename TSelf>
    struct IUnsignedNumber : INumber<TSelf> {};

    template<typename TSelf>
    struct IFloatingPoint : INumber<TSelf> {};

    template<typename TSelf>
    struct IFloatingPointIeee754 : IFloatingPoint<TSelf> {};

    template<typename TSelf>
    struct IBinaryInteger : INumber<TSelf> {};

    template<typename TSelf>
    struct IBinaryNumber : INumberBase<TSelf> {};

    // Arithmetic operator interfaces (C++ satisfies these implicitly via operators)
    template<typename TLeft, typename TRight, typename TResult>
    struct IAdditionOperators {};

    template<typename TLeft, typename TRight, typename TResult>
    struct ISubtractionOperators {};

    template<typename TLeft, typename TRight, typename TResult>
    struct IMultiplyOperators {};

    template<typename TLeft, typename TRight, typename TResult>
    struct IDivisionOperators {};

    template<typename TLeft, typename TRight, typename TResult>
    struct IModulusOperators {};

    template<typename TSelf>
    struct IUnaryNegationOperators {};

    template<typename TSelf>
    struct IUnaryPlusOperators {};

    template<typename TLeft, typename TRight>
    struct IComparisonOperators {};

    template<typename TLeft, typename TRight, typename TResult>
    struct IBitwiseOperators {};

    template<typename TLeft, typename TRight, typename TResult>
    struct IShiftOperators {};

    template<typename TSelf>
    struct IIncrementOperators {};

    template<typename TSelf>
    struct IDecrementOperators {};

    template<typename TSelf>
    struct IMinMaxValue {
        static TSelf MinValue();
        static TSelf MaxValue();
    };

    template<typename TSelf>
    struct IAdditiveIdentity { static TSelf AdditiveIdentity(); };

    template<typename TSelf>
    struct IMultiplicativeIdentity { static TSelf MultiplicativeIdentity(); };

} // namespace System::Numerics
