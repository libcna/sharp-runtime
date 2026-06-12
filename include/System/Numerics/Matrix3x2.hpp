// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <sstream>
#include <string>
#include "System/Numerics/Vector2.hpp"

namespace System::Numerics {

/// <summary>Represents a 3x2 matrix used for 2D transformations.</summary>
struct Matrix3x2 {
    float M11{}, M12{};
    float M21{}, M22{};
    float M31{}, M32{};

    Matrix3x2() = default;
    Matrix3x2(float m11,float m12,float m21,float m22,float m31,float m32)
        : M11(m11),M12(m12),M21(m21),M22(m22),M31(m31),M32(m32) {}

    static Matrix3x2 Identity() { return {1,0, 0,1, 0,0}; }

    [[nodiscard]] bool getIsIdentityProperty() const {
        return M11==1&&M12==0&&M21==0&&M22==1&&M31==0&&M32==0;
    }

    [[nodiscard]] Vector2 getTranslationProperty() const { return {M31,M32}; }
    void setTranslationProperty(Vector2 t) { M31=t.X; M32=t.Y; }

    [[nodiscard]] float GetDeterminant() const { return M11*M22 - M12*M21; }

    bool Equals(const Matrix3x2& o) const {
        return M11==o.M11&&M12==o.M12&&M21==o.M21&&M22==o.M22&&M31==o.M31&&M32==o.M32;
    }
    bool operator==(const Matrix3x2& o) const { return Equals(o); }
    bool operator!=(const Matrix3x2& o) const { return !Equals(o); }

    Matrix3x2 operator+(const Matrix3x2& r) const {
        return {M11+r.M11,M12+r.M12,M21+r.M21,M22+r.M22,M31+r.M31,M32+r.M32};
    }
    Matrix3x2 operator-(const Matrix3x2& r) const {
        return {M11-r.M11,M12-r.M12,M21-r.M21,M22-r.M22,M31-r.M31,M32-r.M32};
    }
    Matrix3x2 operator-() const { return {-M11,-M12,-M21,-M22,-M31,-M32}; }
    Matrix3x2 operator*(float s) const { return {M11*s,M12*s,M21*s,M22*s,M31*s,M32*s}; }
    Matrix3x2 operator*(const Matrix3x2& b) const {
        return {
            M11*b.M11+M12*b.M21,
            M11*b.M12+M12*b.M22,
            M21*b.M11+M22*b.M21,
            M21*b.M12+M22*b.M22,
            M31*b.M11+M32*b.M21+b.M31,
            M31*b.M12+M32*b.M22+b.M32
        };
    }

    static Matrix3x2 Add(Matrix3x2 a, Matrix3x2 b)      { return a+b; }
    static Matrix3x2 Subtract(Matrix3x2 a, Matrix3x2 b) { return a-b; }
    static Matrix3x2 Multiply(Matrix3x2 a, Matrix3x2 b) { return a*b; }
    static Matrix3x2 Multiply(Matrix3x2 a, float s)     { return a*s; }
    static Matrix3x2 Negate(Matrix3x2 m)                { return -m; }

    static bool Invert(const Matrix3x2& m, Matrix3x2& result) {
        float det = m.GetDeterminant();
        if (std::abs(det) < 1e-10f) { result = {}; return false; }
        float inv = 1.0f / det;
        result = {m.M22*inv, -m.M12*inv, -m.M21*inv, m.M11*inv,
                  (m.M21*m.M32-m.M22*m.M31)*inv, (m.M12*m.M31-m.M11*m.M32)*inv};
        return true;
    }

    static Matrix3x2 CreateTranslation(float x, float y) { return {1,0, 0,1, x,y}; }
    static Matrix3x2 CreateTranslation(Vector2 t)        { return CreateTranslation(t.X,t.Y); }
    static Matrix3x2 CreateScale(float x, float y)       { return {x,0, 0,y, 0,0}; }
    static Matrix3x2 CreateScale(Vector2 s)              { return CreateScale(s.X,s.Y); }
    static Matrix3x2 CreateScale(float s)                { return CreateScale(s,s); }

    static Matrix3x2 CreateRotation(float radians) {
        float c=std::cos(radians), s=std::sin(radians);
        return {c,s, -s,c, 0,0};
    }

    static Matrix3x2 CreateSkew(float radiansX, float radiansY) {
        return {1,std::tan(radiansY), std::tan(radiansX),1, 0,0};
    }

    std::string ToString() const {
        std::ostringstream ss;
        ss<<"{ {M11:"<<M11<<" M12:"<<M12<<"} {M21:"<<M21<<" M22:"<<M22<<"} {M31:"<<M31<<" M32:"<<M32<<"} }";
        return ss.str();
    }
};

// --- Deferred Vector2::Transform for Matrix3x2 ---
inline Vector2 Vector2::Transform(Vector2 p, const Matrix3x2& m) {
    return {p.X*m.M11+p.Y*m.M21+m.M31, p.X*m.M12+p.Y*m.M22+m.M32};
}
inline Vector2 Vector2::TransformNormal(Vector2 n, const Matrix3x2& m) {
    return {n.X*m.M11+n.Y*m.M21, n.X*m.M12+n.Y*m.M22};
}

} // namespace System::Numerics
