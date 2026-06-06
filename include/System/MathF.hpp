// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <limits>

namespace System {

    // Single-precision counterpart of System.Math.
    class MathF {
    public:
        MathF() = delete;

        static constexpr float E  = 2.71828182845904523536f;
        static constexpr float PI = 3.14159265358979323846f;
        static constexpr float Tau = 6.28318530717958647692f;

        static float Abs(float x)                  { return std::abs(x); }
        static float Ceiling(float x)              { return std::ceil(x); }
        static float Floor(float x)                { return std::floor(x); }
        static float Round(float x)                { return std::round(x); }
        static float Truncate(float x)             { return std::trunc(x); }
        static float Sqrt(float x)                 { return std::sqrt(x); }
        static float Cbrt(float x)                 { return std::cbrt(x); }
        static float Exp(float x)                  { return std::exp(x); }
        static float Log(float x)                  { return std::log(x); }
        static float Log(float x, float y)         { return std::log(x) / std::log(y); }
        static float Log2(float x)                 { return std::log2(x); }
        static float Log10(float x)                { return std::log10(x); }
        static float Pow(float x, float y)         { return std::pow(x, y); }
        static float Sin(float x)                  { return std::sin(x); }
        static float Cos(float x)                  { return std::cos(x); }
        static float Tan(float x)                  { return std::tan(x); }
        static float Asin(float x)                 { return std::asin(x); }
        static float Acos(float x)                 { return std::acos(x); }
        static float Atan(float x)                 { return std::atan(x); }
        static float Atan2(float y, float x)       { return std::atan2(y, x); }
        static float Sinh(float x)                 { return std::sinh(x); }
        static float Cosh(float x)                 { return std::cosh(x); }
        static float Tanh(float x)                 { return std::tanh(x); }
        static float Max(float x, float y)         { return std::fmax(x, y); }
        static float Min(float x, float y)         { return std::fmin(x, y); }
        static float Clamp(float v, float min, float max) {
            return v < min ? min : (v > max ? max : v);
        }
        static float Sign(float x)                 { return x < 0 ? -1.0f : (x > 0 ? 1.0f : 0.0f); }
        static bool IsNaN(float x)                 { return std::isnan(x); }
        static bool IsInfinity(float x)            { return std::isinf(x); }
        static bool IsPositiveInfinity(float x)    { return x == std::numeric_limits<float>::infinity(); }
        static bool IsNegativeInfinity(float x)    { return x == -std::numeric_limits<float>::infinity(); }
        static float IEEERemainder(float x, float y) { return std::remainder(x, y); }
    };

} // namespace System
