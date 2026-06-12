// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <sstream>
#include <string>
#include "System/Numerics/Vector3.hpp"

namespace System::Numerics {

struct Matrix4x4;

/// <summary>Represents a vector that is used to encode three-dimensional physical rotations.</summary>
struct Quaternion {
    float X{0.0f};
    float Y{0.0f};
    float Z{0.0f};
    float W{1.0f};

    Quaternion() = default;
    Quaternion(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}
    Quaternion(Vector3 v, float w) : X(v.X), Y(v.Y), Z(v.Z), W(w) {}

    static Quaternion Identity() { return {0,0,0,1}; }
    static Quaternion Zero()     { return {0,0,0,0}; }

    [[nodiscard]] bool getIsIdentityProperty() const { return X==0&&Y==0&&Z==0&&W==1; }
    [[nodiscard]] float Length()        const { return std::sqrt(LengthSquared()); }
    [[nodiscard]] float LengthSquared() const { return X*X+Y*Y+Z*Z+W*W; }

    [[nodiscard]] bool Equals(const Quaternion& o) const {
        return X==o.X&&Y==o.Y&&Z==o.Z&&W==o.W;
    }

    std::string ToString() const {
        std::ostringstream ss; ss<<"{X:"<<X<<" Y:"<<Y<<" Z:"<<Z<<" W:"<<W<<"}"; return ss.str();
    }

    Quaternion operator+(const Quaternion& r) const { return {X+r.X,Y+r.Y,Z+r.Z,W+r.W}; }
    Quaternion operator-(const Quaternion& r) const { return {X-r.X,Y-r.Y,Z-r.Z,W-r.W}; }
    Quaternion operator-()                   const { return {-X,-Y,-Z,-W}; }
    Quaternion operator*(float s)            const { return {X*s,Y*s,Z*s,W*s}; }
    Quaternion operator/(float s)            const { return {X/s,Y/s,Z/s,W/s}; }
    bool operator==(const Quaternion& r)    const { return Equals(r); }
    bool operator!=(const Quaternion& r)    const { return !Equals(r); }

    Quaternion operator*(const Quaternion& q) const {
        return {
            W*q.X + X*q.W + Y*q.Z - Z*q.Y,
            W*q.Y - X*q.Z + Y*q.W + Z*q.X,
            W*q.Z + X*q.Y - Y*q.X + Z*q.W,
            W*q.W - X*q.X - Y*q.Y - Z*q.Z
        };
    }

    static float       Dot(Quaternion a, Quaternion b) { return a.X*b.X+a.Y*b.Y+a.Z*b.Z+a.W*b.W; }
    static Quaternion  Normalize(Quaternion q) { float l=q.Length(); return l>0?q/l:q; }
    static Quaternion  Conjugate(Quaternion q) { return {-q.X,-q.Y,-q.Z,q.W}; }
    static Quaternion  Inverse(Quaternion q)   { float n=q.LengthSquared(); return n>0?Conjugate(q)/n:q; }
    static Quaternion  Negate(Quaternion q)    { return -q; }
    static Quaternion  Add(Quaternion a,Quaternion b)      { return a+b; }
    static Quaternion  Subtract(Quaternion a,Quaternion b) { return a-b; }
    static Quaternion  Multiply(Quaternion a,Quaternion b) { return a*b; }
    static Quaternion  Multiply(Quaternion q, float s)     { return q*s; }
    static Quaternion  Divide(Quaternion a, Quaternion b)  { return a*Inverse(b); }

    static Quaternion CreateFromAxisAngle(Vector3 axis, float angle) {
        float half=angle*0.5f, s=std::sin(half), c=std::cos(half);
        return {axis.X*s, axis.Y*s, axis.Z*s, c};
    }

    static Quaternion CreateFromYawPitchRoll(float yaw, float pitch, float roll) {
        float cy=std::cos(yaw*0.5f), sy=std::sin(yaw*0.5f);
        float cp=std::cos(pitch*0.5f), sp=std::sin(pitch*0.5f);
        float cr=std::cos(roll*0.5f), sr=std::sin(roll*0.5f);
        return {
            cy*sp*cr + sy*cp*sr,
            sy*cp*cr - cy*sp*sr,
            cy*cp*sr - sy*sp*cr,
            cy*cp*cr + sy*sp*sr
        };
    }

    static Quaternion Slerp(Quaternion a, Quaternion b, float t) {
        float cosAngle = Dot(a, b);
        if (cosAngle < 0) { b = -b; cosAngle = -cosAngle; }
        float k0, k1;
        if (cosAngle > 0.9999f) {
            k0 = 1.0f - t; k1 = t;
        } else {
            float angle = std::acos(cosAngle);
            float inv = 1.0f / std::sin(angle);
            k0 = std::sin((1.0f-t)*angle)*inv;
            k1 = std::sin(t*angle)*inv;
        }
        return {a.X*k0+b.X*k1, a.Y*k0+b.Y*k1, a.Z*k0+b.Z*k1, a.W*k0+b.W*k1};
    }

    static Quaternion Lerp(Quaternion a, Quaternion b, float t) {
        if (Dot(a,b) < 0) b = -b;
        return Normalize({a.X+t*(b.X-a.X), a.Y+t*(b.Y-a.Y), a.Z+t*(b.Z-a.Z), a.W+t*(b.W-a.W)});
    }

    static Quaternion CreateFromRotationMatrix(const Matrix4x4& m);
};

} // namespace System::Numerics
