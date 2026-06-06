// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Math.hpp"

#include <algorithm>
#include <cmath>

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
}