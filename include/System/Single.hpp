// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace System {

class Single {
public:
    static constexpr float MaxValue          =  std::numeric_limits<float>::max();
    static constexpr float MinValue          = -std::numeric_limits<float>::max();
    static constexpr float Epsilon           =  std::numeric_limits<float>::min();
    static constexpr float NaN               =  std::numeric_limits<float>::quiet_NaN();
    static constexpr float PositiveInfinity  =  std::numeric_limits<float>::infinity();
    static constexpr float NegativeInfinity  = -std::numeric_limits<float>::infinity();
    static constexpr float NegativeZero      = -0.0f;

    static bool IsNaN(float f)               { return std::isnan(f); }
    static bool IsInfinity(float f)          { return std::isinf(f); }
    static bool IsPositiveInfinity(float f)  { return std::isinf(f) && f > 0.0f; }
    static bool IsNegativeInfinity(float f)  { return std::isinf(f) && f < 0.0f; }
    static bool IsFinite(float f)            { return std::isfinite(f); }
    static bool IsNormal(float f)            { return std::isnormal(f); }
    static bool IsSubnormal(float f)         { return std::fpclassify(f) == FP_SUBNORMAL; }

    static float Parse(const std::string& s) {
        if (s == "NaN")       return std::numeric_limits<float>::quiet_NaN();
        if (s == "Infinity")  return  std::numeric_limits<float>::infinity();
        if (s == "-Infinity") return -std::numeric_limits<float>::infinity();
        float result{};
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
        if (ec != std::errc{} || ptr != s.data() + s.size())
            throw std::invalid_argument("Input string was not in a correct format.");
        return result;
    }

    static bool TryParse(const std::string& s, float& result) {
        if (s == "NaN")       { result = std::numeric_limits<float>::quiet_NaN(); return true; }
        if (s == "Infinity")  { result =  std::numeric_limits<float>::infinity(); return true; }
        if (s == "-Infinity") { result = -std::numeric_limits<float>::infinity(); return true; }
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
        if (ec != std::errc{} || ptr != s.data() + s.size()) { result = 0.0f; return false; }
        return true;
    }

    static std::string ToString(float value) {
        if (std::isnan(value)) return "NaN";
        if (std::isinf(value)) return value > 0 ? "Infinity" : "-Infinity";
        std::array<char, 64> buf;
        auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
        if (ec == std::errc{}) return std::string(buf.data(), ptr);
        return std::to_string(value);
    }
};

} // namespace System
