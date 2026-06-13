// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Numerics {

// Forward declarations
struct Matrix3x2;
struct Matrix4x4;

/// <summary>A structure encapsulating two single-precision floating-point values.</summary>
struct Vector2 {
    float X{0.0f};
    float Y{0.0f};

    Vector2() = default;
    explicit Vector2(float value) : X(value), Y(value) {}
    Vector2(float x, float y) : X(x), Y(y) {}

    // --- Static constants ---
    static Vector2 Zero()     { return {0.0f, 0.0f}; }
    static Vector2 One()      { return {1.0f, 1.0f}; }
    static Vector2 UnitX()    { return {1.0f, 0.0f}; }
    static Vector2 UnitY()    { return {0.0f, 1.0f}; }

    // --- Instance methods ---
    [[nodiscard]] float Length()        const { return std::sqrt(LengthSquared()); }
    [[nodiscard]] float LengthSquared() const { return X * X + Y * Y; }

    void CopyTo(float* arr) const { arr[0] = X; arr[1] = Y; }

    [[nodiscard]] bool Equals(const Vector2& other) const { return X == other.X && Y == other.Y; }

    std::string ToString() const {
        std::ostringstream ss;
        ss.imbue(std::locale::classic());
        ss << "<" << X << ", " << Y << ">";
        return ss.str();
    }

    float operator[](int index) const {
        if (index == 0) return X;
        if (index == 1) return Y;
        throw std::out_of_range("index");
    }
    float& operator[](int index) {
        if (index == 0) return X;
        if (index == 1) return Y;
        throw std::out_of_range("index");
    }

    // --- Operators ---
    Vector2 operator+(const Vector2& r) const { return {X + r.X, Y + r.Y}; }
    Vector2 operator-(const Vector2& r) const { return {X - r.X, Y - r.Y}; }
    Vector2 operator*(const Vector2& r) const { return {X * r.X, Y * r.Y}; }
    Vector2 operator*(float s)          const { return {X * s,   Y * s};   }
    Vector2 operator/(const Vector2& r) const { return {X / r.X, Y / r.Y}; }
    Vector2 operator/(float s)          const { return {X / s,   Y / s};   }
    Vector2 operator-()                 const { return {-X, -Y}; }
    bool    operator==(const Vector2& r) const { return Equals(r); }
    bool    operator!=(const Vector2& r) const { return !Equals(r); }
    friend Vector2 operator*(float s, const Vector2& v) { return v * s; }

    // --- Static math ---
    static Vector2 Abs(Vector2 v)              { return {std::abs(v.X), std::abs(v.Y)}; }
    static Vector2 Add(Vector2 a, Vector2 b)   { return a + b; }
    static Vector2 Subtract(Vector2 a, Vector2 b) { return a - b; }
    static Vector2 Multiply(Vector2 a, Vector2 b) { return a * b; }
    static Vector2 Multiply(Vector2 v, float s)   { return v * s; }
    static Vector2 Multiply(float s, Vector2 v)   { return v * s; }
    static Vector2 Divide(Vector2 a, Vector2 b)   { return a / b; }
    static Vector2 Divide(Vector2 v, float s)      { return v / s; }
    static Vector2 Negate(Vector2 v)               { return -v; }

    static float   Dot(Vector2 a, Vector2 b)        { return a.X * b.X + a.Y * b.Y; }
    static float   Cross(Vector2 a, Vector2 b)      { return a.X * b.Y - a.Y * b.X; }
    static float   Distance(Vector2 a, Vector2 b)   { return (a - b).Length(); }
    static float   DistanceSquared(Vector2 a, Vector2 b) { return (a - b).LengthSquared(); }

    static Vector2 Normalize(Vector2 v) {
        float len = v.Length();
        return len > 0 ? v / len : v;
    }
    static Vector2 Reflect(Vector2 v, Vector2 normal) {
        return v - 2.0f * Dot(v, normal) * normal;
    }
    static Vector2 Min(Vector2 a, Vector2 b) { return {std::min(a.X,b.X), std::min(a.Y,b.Y)}; }
    static Vector2 Max(Vector2 a, Vector2 b) { return {std::max(a.X,b.X), std::max(a.Y,b.Y)}; }
    static Vector2 Clamp(Vector2 v, Vector2 mn, Vector2 mx) { return Min(Max(v,mn),mx); }
    static Vector2 Lerp(Vector2 a, Vector2 b, float t) { return a + (b - a) * t; }
    static Vector2 SquareRoot(Vector2 v) { return {std::sqrt(v.X), std::sqrt(v.Y)}; }

    static Vector2 Transform(Vector2 pos, const Matrix3x2& m);
    static Vector2 Transform(Vector2 pos, const Matrix4x4& m);
    static Vector2 TransformNormal(Vector2 n, const Matrix3x2& m);
    static Vector2 TransformNormal(Vector2 n, const Matrix4x4& m);
};

} // namespace System::Numerics
