// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include "System/Numerics/Vector2.hpp"
#include "System/Numerics/Vector3.hpp"

namespace System::Numerics {

struct Matrix4x4;
struct Quaternion;

/// <summary>A structure encapsulating four single-precision floating-point values.</summary>
struct Vector4 {
    float X{0.0f};
    float Y{0.0f};
    float Z{0.0f};
    float W{0.0f};

    Vector4() = default;
    explicit Vector4(float value) : X(value), Y(value), Z(value), W(value) {}
    Vector4(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}
    Vector4(Vector2 xy, float z, float w) : X(xy.X), Y(xy.Y), Z(z), W(w) {}
    Vector4(Vector3 xyz, float w)         : X(xyz.X), Y(xyz.Y), Z(xyz.Z), W(w) {}

    static Vector4 Zero()  { return {0,0,0,0}; }
    static Vector4 One()   { return {1,1,1,1}; }
    static Vector4 UnitX() { return {1,0,0,0}; }
    static Vector4 UnitY() { return {0,1,0,0}; }
    static Vector4 UnitZ() { return {0,0,1,0}; }
    static Vector4 UnitW() { return {0,0,0,1}; }

    [[nodiscard]] float Length()        const { return std::sqrt(LengthSquared()); }
    [[nodiscard]] float LengthSquared() const { return X*X+Y*Y+Z*Z+W*W; }

    [[nodiscard]] bool Equals(const Vector4& o) const { return X==o.X&&Y==o.Y&&Z==o.Z&&W==o.W; }

    std::string ToString() const {
        std::ostringstream ss; ss<<"<"<<X<<", "<<Y<<", "<<Z<<", "<<W<<">"; return ss.str();
    }

    float operator[](int i) const {
        if(i==0)return X; if(i==1)return Y; if(i==2)return Z; if(i==3)return W;
        throw std::out_of_range("index");
    }
    float& operator[](int i) {
        if(i==0)return X; if(i==1)return Y; if(i==2)return Z; if(i==3)return W;
        throw std::out_of_range("index");
    }

    Vector4 operator+(const Vector4& r) const { return {X+r.X,Y+r.Y,Z+r.Z,W+r.W}; }
    Vector4 operator-(const Vector4& r) const { return {X-r.X,Y-r.Y,Z-r.Z,W-r.W}; }
    Vector4 operator*(const Vector4& r) const { return {X*r.X,Y*r.Y,Z*r.Z,W*r.W}; }
    Vector4 operator*(float s)          const { return {X*s,Y*s,Z*s,W*s}; }
    Vector4 operator/(const Vector4& r) const { return {X/r.X,Y/r.Y,Z/r.Z,W/r.W}; }
    Vector4 operator/(float s)          const { return {X/s,Y/s,Z/s,W/s}; }
    Vector4 operator-()                 const { return {-X,-Y,-Z,-W}; }
    bool    operator==(const Vector4& r) const { return Equals(r); }
    bool    operator!=(const Vector4& r) const { return !Equals(r); }
    friend Vector4 operator*(float s, const Vector4& v) { return v * s; }

    static float   Dot(Vector4 a, Vector4 b)      { return a.X*b.X+a.Y*b.Y+a.Z*b.Z+a.W*b.W; }
    static float   Distance(Vector4 a, Vector4 b) { return (a-b).Length(); }
    static float   DistanceSquared(Vector4 a, Vector4 b) { return (a-b).LengthSquared(); }
    static Vector4 Normalize(Vector4 v) { float l=v.Length(); return l>0?v/l:v; }
    static Vector4 Abs(Vector4 v)    { return {std::abs(v.X),std::abs(v.Y),std::abs(v.Z),std::abs(v.W)}; }
    static Vector4 Min(Vector4 a, Vector4 b) {
        return {std::min(a.X,b.X),std::min(a.Y,b.Y),std::min(a.Z,b.Z),std::min(a.W,b.W)};
    }
    static Vector4 Max(Vector4 a, Vector4 b) {
        return {std::max(a.X,b.X),std::max(a.Y,b.Y),std::max(a.Z,b.Z),std::max(a.W,b.W)};
    }
    static Vector4 Clamp(Vector4 v, Vector4 mn, Vector4 mx) { return Min(Max(v,mn),mx); }
    static Vector4 Lerp(Vector4 a, Vector4 b, float t) { return a+(b-a)*t; }
    static Vector4 SquareRoot(Vector4 v) {
        return {std::sqrt(v.X),std::sqrt(v.Y),std::sqrt(v.Z),std::sqrt(v.W)};
    }
    static Vector4 Add(Vector4 a,Vector4 b)      { return a+b; }
    static Vector4 Subtract(Vector4 a,Vector4 b) { return a-b; }
    static Vector4 Multiply(Vector4 a,Vector4 b) { return a*b; }
    static Vector4 Multiply(Vector4 v,float s)   { return v*s; }
    static Vector4 Divide(Vector4 a,Vector4 b)   { return a/b; }
    static Vector4 Divide(Vector4 v,float s)     { return v/s; }
    static Vector4 Negate(Vector4 v)             { return -v; }

    static Vector4 Transform(Vector2 pos, const Matrix4x4& m);
    static Vector4 Transform(Vector3 pos, const Matrix4x4& m);
    static Vector4 Transform(Vector4 v,   const Matrix4x4& m);
    static Vector4 Transform(Vector4 v,   const Quaternion& q);
};

} // namespace System::Numerics
