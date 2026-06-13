// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Math.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace System
{
    double Math::Sin(double value)
    {
        return std::sin(value);
    }

    double Math::Cos(double value)
    {
        return std::cos(value);
    }

    double Math::Tan(double value)
    {
        return std::tan(value);
    }

    double Math::Sqrt(double value)
    {
        return std::sqrt(value);
    }

    double Math::Abs(double value)
    {
        return std::fabs(value);
    }

    intcs Math::Abs(intcs value)
    {
        return std::abs(value);
    }

    intcs Math::Min(intcs a, intcs b)
    {
        return std::min(a, b);
    }

    double Math::Min(double a, double b)
    {
        return std::min(a, b);
    }

    intcs Math::Max(intcs a, intcs b)
    {
        return std::max(a, b);
    }

    double Math::Max(double a, double b)
    {
        return std::max(a, b);
    }

    intcs Math::Clamp(intcs value, intcs min, intcs max)
    {
        if (value < min)
        {
            return min;
        }
        if (value > max)
        {
            return max;
        }
        return value;
    }

    double Math::Clamp(double value, double min, double max)
    {
        if (value < min)
        {
            return min;
        }
        if (value > max)
        {
            return max;
        }
        return value;
    }

    double Math::Round(double value)
    {
        return std::round(value);
    }

    double Math::Floor(double value)
    {
        return std::floor(value);
    }

    double Math::Ceiling(double value)
    {
        return std::ceil(value);
    }

    double Math::Pow(double x, double y)
    {
        return std::pow(x, y);
    }

    double Math::Log(double d)    { return std::log(d); }
    double Math::Log(double a, double newBase) { return std::log(a) / std::log(newBase); }
    double Math::Log2(double x)   { return std::log2(x); }
    double Math::Log10(double d)  { return std::log10(d); }
    double Math::Exp(double d)    { return std::exp(d); }

    double Math::Asin(double d)   { return std::asin(d); }
    double Math::Acos(double d)   { return std::acos(d); }
    double Math::Atan(double d)   { return std::atan(d); }
    double Math::Atan2(double y, double x) { return std::atan2(y, x); }

    double Math::Sinh(double value) { return std::sinh(value); }
    double Math::Cosh(double value) { return std::cosh(value); }
    double Math::Tanh(double value) { return std::tanh(value); }

    intcs Math::Sign(intcs value)
    {
        if (value < 0) return -1;
        if (value > 0) return  1;
        return 0;
    }

    intcs Math::Sign(double value)
    {
        if (value < 0.0) return -1;
        if (value > 0.0) return  1;
        return 0;
    }

    double Math::Truncate(double d) { return std::trunc(d); }

    double Math::IEEERemainder(double x, double y)
    {
        return std::remainder(x, y);
    }

    intcs Math::DivRem(intcs a, intcs b, intcs& result)
    {
        intcs q = a / b;
        result  = a % b;
        return q;
    }

    longcs Math::BigMul(intcs a, intcs b)
    {
        return static_cast<longcs>(a) * static_cast<longcs>(b);
    }

    double Math::ScaleB(double x, intcs n)
    {
        return std::scalbn(x, n);
    }
}