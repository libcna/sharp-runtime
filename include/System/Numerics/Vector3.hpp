// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include "System/Numerics/Vector2.hpp"

namespace System::Numerics {

struct Matrix4x4;
struct Quaternion;

/// <summary>A structure encapsulating three single-precision floating-point values.</summary>
struct Vector3 {
    float X{0.0f};
    float Y{0.0f};
    float Z{0.0f};

    Vector3() = default;
    explicit Vector3(float value) : X(value), Y(value), Z(value) {}
    Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}
    Vector3(Vector2 xy, float z) : X(xy.X), Y(xy.Y), Z(z) {}

    static Vector3 Zero()    { return {0.0f,0.0f,0.0f}; }
    static Vector3 One()     { return {1.0f,1.0f,1.0f}; }
    static Vector3 UnitX()   { return {1.0f,0.0f,0.0f}; }
    static Vector3 UnitY()   { return {0.0f,1.0f,0.0f}; }
    static Vector3 UnitZ()   { return {0.0f,0.0f,1.0f}; }

    [[nodiscard]] float Length()        const { return std::sqrt(LengthSquared()); }
    [[nodiscard]] float LengthSquared() const { return X*X + Y*Y + Z*Z; }

    [[nodiscard]] bool Equals(const Vector3& o) const { return X==o.X && Y==o.Y && Z==o.Z; }

    std::string ToString() const {
        std::ostringstream ss; ss << "<" << X << ", " << Y << ", " << Z << ">"; return ss.str();
    }

    float operator[](int i) const {
        if (i==0) return X; if (i==1) return Y; if (i==2) return Z;
        throw std::out_of_range("index");
    }
    float& operator[](int i) {
        if (i==0) return X; if (i==1) return Y; if (i==2) return Z;
        throw std::out_of_range("index");
    }

    Vector3 operator+(const Vector3& r) const { return {X+r.X,Y+r.Y,Z+r.Z}; }
    Vector3 operator-(const Vector3& r) const { return {X-r.X,Y-r.Y,Z-r.Z}; }
    Vector3 operator*(const Vector3& r) const { return {X*r.X,Y*r.Y,Z*r.Z}; }
    Vector3 operator*(float s)          const { return {X*s,  Y*s,  Z*s};   }
    Vector3 operator/(const Vector3& r) const { return {X/r.X,Y/r.Y,Z/r.Z}; }
    Vector3 operator/(float s)          const { return {X/s,  Y/s,  Z/s};   }
    Vector3 operator-()                 const { return {-X,-Y,-Z}; }
    bool    operator==(const Vector3& r) const { return Equals(r); }
    bool    operator!=(const Vector3& r) const { return !Equals(r); }
    friend Vector3 operator*(float s, const Vector3& v) { return v * s; }

    static float   Dot(Vector3 a, Vector3 b)         { return a.X*b.X + a.Y*b.Y + a.Z*b.Z; }
    static Vector3 Cross(Vector3 a, Vector3 b)       { return {a.Y*b.Z-a.Z*b.Y, a.Z*b.X-a.X*b.Z, a.X*b.Y-a.Y*b.X}; }
    static float   Distance(Vector3 a, Vector3 b)    { return (a-b).Length(); }
    static float   DistanceSquared(Vector3 a, Vector3 b) { return (a-b).LengthSquared(); }
    static Vector3 Normalize(Vector3 v)    { float l=v.Length(); return l>0?v/l:v; }
    static Vector3 Reflect(Vector3 v, Vector3 n) { return v - 2.0f*Dot(v,n)*n; }
    static Vector3 Abs(Vector3 v)          { return {std::abs(v.X),std::abs(v.Y),std::abs(v.Z)}; }
    static Vector3 Min(Vector3 a, Vector3 b) { return {std::min(a.X,b.X),std::min(a.Y,b.Y),std::min(a.Z,b.Z)}; }
    static Vector3 Max(Vector3 a, Vector3 b) { return {std::max(a.X,b.X),std::max(a.Y,b.Y),std::max(a.Z,b.Z)}; }
    static Vector3 Clamp(Vector3 v, Vector3 mn, Vector3 mx) { return Min(Max(v,mn),mx); }
    static Vector3 Lerp(Vector3 a, Vector3 b, float t) { return a+(b-a)*t; }
    static Vector3 SquareRoot(Vector3 v) { return {std::sqrt(v.X),std::sqrt(v.Y),std::sqrt(v.Z)}; }
    static Vector3 Add(Vector3 a,Vector3 b)      { return a+b; }
    static Vector3 Subtract(Vector3 a,Vector3 b) { return a-b; }
    static Vector3 Multiply(Vector3 a,Vector3 b) { return a*b; }
    static Vector3 Multiply(Vector3 v,float s)   { return v*s; }
    static Vector3 Divide(Vector3 a,Vector3 b)   { return a/b; }
    static Vector3 Divide(Vector3 v,float s)     { return v/s; }
    static Vector3 Negate(Vector3 v)             { return -v; }

    static Vector3 Transform(Vector3 pos, const Matrix4x4& m);
    static Vector3 TransformNormal(Vector3 n, const Matrix4x4& m);
    static Vector3 Transform(Vector3 v, const Quaternion& q);
};

} // namespace System::Numerics
