// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <sstream>
#include <string>
#include "System/Numerics/Vector3.hpp"
#include "System/Numerics/Vector4.hpp"
#include "System/Numerics/Quaternion.hpp"

namespace System::Numerics {

/// <summary>Represents a 4x4 matrix.</summary>
struct Matrix4x4 {
    float M11{}, M12{}, M13{}, M14{};
    float M21{}, M22{}, M23{}, M24{};
    float M31{}, M32{}, M33{}, M34{};
    float M41{}, M42{}, M43{}, M44{};

    Matrix4x4() = default;
    Matrix4x4(float m11,float m12,float m13,float m14,
              float m21,float m22,float m23,float m24,
              float m31,float m32,float m33,float m34,
              float m41,float m42,float m43,float m44)
        : M11(m11),M12(m12),M13(m13),M14(m14)
        , M21(m21),M22(m22),M23(m23),M24(m24)
        , M31(m31),M32(m32),M33(m33),M34(m34)
        , M41(m41),M42(m42),M43(m43),M44(m44) {}

    static Matrix4x4 Identity() {
        return {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    }

    [[nodiscard]] bool getIsIdentityProperty() const {
        return M11==1&&M22==1&&M33==1&&M44==1
            && M12==0&&M13==0&&M14==0
            && M21==0&&M23==0&&M24==0
            && M31==0&&M32==0&&M34==0
            && M41==0&&M42==0&&M43==0;
    }

    [[nodiscard]] Vector3 getTranslationProperty() const { return {M41,M42,M43}; }
    void setTranslationProperty(Vector3 t) { M41=t.X; M42=t.Y; M43=t.Z; }

    bool Equals(const Matrix4x4& o) const {
        return M11==o.M11&&M12==o.M12&&M13==o.M13&&M14==o.M14
            && M21==o.M21&&M22==o.M22&&M23==o.M23&&M24==o.M24
            && M31==o.M31&&M32==o.M32&&M33==o.M33&&M34==o.M34
            && M41==o.M41&&M42==o.M42&&M43==o.M43&&M44==o.M44;
    }
    bool operator==(const Matrix4x4& o) const { return Equals(o); }
    bool operator!=(const Matrix4x4& o) const { return !Equals(o); }

    Matrix4x4 operator+(const Matrix4x4& r) const {
        return {M11+r.M11,M12+r.M12,M13+r.M13,M14+r.M14,
                M21+r.M21,M22+r.M22,M23+r.M23,M24+r.M24,
                M31+r.M31,M32+r.M32,M33+r.M33,M34+r.M34,
                M41+r.M41,M42+r.M42,M43+r.M43,M44+r.M44};
    }
    Matrix4x4 operator-(const Matrix4x4& r) const {
        return {M11-r.M11,M12-r.M12,M13-r.M13,M14-r.M14,
                M21-r.M21,M22-r.M22,M23-r.M23,M24-r.M24,
                M31-r.M31,M32-r.M32,M33-r.M33,M34-r.M34,
                M41-r.M41,M42-r.M42,M43-r.M43,M44-r.M44};
    }
    Matrix4x4 operator-() const {
        return {-M11,-M12,-M13,-M14,-M21,-M22,-M23,-M24,
                -M31,-M32,-M33,-M34,-M41,-M42,-M43,-M44};
    }
    Matrix4x4 operator*(float s) const {
        return {M11*s,M12*s,M13*s,M14*s, M21*s,M22*s,M23*s,M24*s,
                M31*s,M32*s,M33*s,M34*s, M41*s,M42*s,M43*s,M44*s};
    }
    Matrix4x4 operator*(const Matrix4x4& b) const {
        return {
            M11*b.M11+M12*b.M21+M13*b.M31+M14*b.M41,
            M11*b.M12+M12*b.M22+M13*b.M32+M14*b.M42,
            M11*b.M13+M12*b.M23+M13*b.M33+M14*b.M43,
            M11*b.M14+M12*b.M24+M13*b.M34+M14*b.M44,
            M21*b.M11+M22*b.M21+M23*b.M31+M24*b.M41,
            M21*b.M12+M22*b.M22+M23*b.M32+M24*b.M42,
            M21*b.M13+M22*b.M23+M23*b.M33+M24*b.M43,
            M21*b.M14+M22*b.M24+M23*b.M34+M24*b.M44,
            M31*b.M11+M32*b.M21+M33*b.M31+M34*b.M41,
            M31*b.M12+M32*b.M22+M33*b.M32+M34*b.M42,
            M31*b.M13+M32*b.M23+M33*b.M33+M34*b.M43,
            M31*b.M14+M32*b.M24+M33*b.M34+M34*b.M44,
            M41*b.M11+M42*b.M21+M43*b.M31+M44*b.M41,
            M41*b.M12+M42*b.M22+M43*b.M32+M44*b.M42,
            M41*b.M13+M42*b.M23+M43*b.M33+M44*b.M43,
            M41*b.M14+M42*b.M24+M43*b.M34+M44*b.M44
        };
    }

    static Matrix4x4 Add(Matrix4x4 a, Matrix4x4 b)      { return a+b; }
    static Matrix4x4 Subtract(Matrix4x4 a, Matrix4x4 b) { return a-b; }
    static Matrix4x4 Multiply(Matrix4x4 a, Matrix4x4 b) { return a*b; }
    static Matrix4x4 Multiply(Matrix4x4 a, float s)     { return a*s; }
    static Matrix4x4 Negate(Matrix4x4 m)                { return -m; }

    static Matrix4x4 Transpose(const Matrix4x4& m) {
        return {m.M11,m.M21,m.M31,m.M41, m.M12,m.M22,m.M32,m.M42,
                m.M13,m.M23,m.M33,m.M43, m.M14,m.M24,m.M34,m.M44};
    }

    [[nodiscard]] float GetDeterminant() const {
        float a = M11*(M22*(M33*M44-M34*M43)-M23*(M32*M44-M34*M42)+M24*(M32*M43-M33*M42));
        float b = M12*(M21*(M33*M44-M34*M43)-M23*(M31*M44-M34*M41)+M24*(M31*M43-M33*M41));
        float c = M13*(M21*(M32*M44-M34*M42)-M22*(M31*M44-M34*M41)+M24*(M31*M42-M32*M41));
        float d = M14*(M21*(M32*M43-M33*M42)-M22*(M31*M43-M33*M41)+M23*(M31*M42-M32*M41));
        return a - b + c - d;
    }

    static bool Invert(const Matrix4x4& m, Matrix4x4& result) {
        // Cofactor expansion
        float a=m.M11,b=m.M12,c=m.M13,d=m.M14;
        float e=m.M21,f=m.M22,g=m.M23,h=m.M24;
        float i=m.M31,j=m.M32,k=m.M33,l=m.M34;
        float mn=m.M41,n=m.M42,o=m.M43,p=m.M44;

        float kp_lo=k*p-l*o, jp_ln=j*p-l*n, jo_kn=j*o-k*n;
        float ip_lm=i*p-l*mn, io_km=i*o-k*mn, in_jm=i*n-j*mn;
        float gp_ho=g*p-h*o, fp_hn=f*p-h*n, fo_gn=f*o-g*n;
        float ep_hm=e*p-h*mn, eo_gm=e*o-g*mn, en_fm=e*n-f*mn;
        float gl_hk=g*l-h*k, fl_hj=f*l-h*j, fk_gj=f*k-g*j;
        float el_hi=e*l-h*i, ek_gi=e*k-g*i, ej_fi=e*j-f*i;

        float det = a*(f*(kp_lo)-g*(jp_ln)+h*(jo_kn))
                  - b*(e*(kp_lo)-g*(ip_lm)+h*(io_km))
                  + c*(e*(jp_ln)-f*(ip_lm)+h*(in_jm))
                  - d*(e*(jo_kn)-f*(io_km)+g*(in_jm));
        if (std::abs(det) < 1e-10f) { result = {}; return false; }
        float inv = 1.0f / det;
        result.M11 = (f*(kp_lo)-g*(jp_ln)+h*(jo_kn))*inv;
        result.M21 = -(e*(kp_lo)-g*(ip_lm)+h*(io_km))*inv;
        result.M31 = (e*(jp_ln)-f*(ip_lm)+h*(in_jm))*inv;
        result.M41 = -(e*(jo_kn)-f*(io_km)+g*(in_jm))*inv;
        result.M12 = -(b*(kp_lo)-c*(jp_ln)+d*(jo_kn))*inv;
        result.M22 = (a*(kp_lo)-c*(ip_lm)+d*(io_km))*inv;
        result.M32 = -(a*(jp_ln)-b*(ip_lm)+d*(in_jm))*inv;
        result.M42 = (a*(jo_kn)-b*(io_km)+c*(in_jm))*inv;
        result.M13 = (b*(gp_ho)-c*(fp_hn)+d*(fo_gn))*inv;
        result.M23 = -(a*(gp_ho)-c*(ep_hm)+d*(eo_gm))*inv;
        result.M33 = (a*(fp_hn)-b*(ep_hm)+d*(en_fm))*inv;
        result.M43 = -(a*(fo_gn)-b*(eo_gm)+c*(en_fm))*inv;
        result.M14 = -(b*(gl_hk)-c*(fl_hj)+d*(fk_gj))*inv;
        result.M24 = (a*(gl_hk)-c*(el_hi)+d*(ek_gi))*inv;
        result.M34 = -(a*(fl_hj)-b*(el_hi)+d*(ej_fi))*inv;
        result.M44 = (a*(fk_gj)-b*(ek_gi)+c*(ej_fi))*inv;
        return true;
    }

    // --- Factory methods ---
    static Matrix4x4 CreateTranslation(float x, float y, float z) {
        auto m = Identity(); m.M41=x; m.M42=y; m.M43=z; return m;
    }
    static Matrix4x4 CreateTranslation(Vector3 t) { return CreateTranslation(t.X,t.Y,t.Z); }

    static Matrix4x4 CreateScale(float x, float y, float z) {
        auto m = Identity(); m.M11=x; m.M22=y; m.M33=z; return m;
    }
    static Matrix4x4 CreateScale(Vector3 s) { return CreateScale(s.X,s.Y,s.Z); }
    static Matrix4x4 CreateScale(float s)   { return CreateScale(s,s,s); }

    static Matrix4x4 CreateRotationX(float radians) {
        float c=std::cos(radians), s=std::sin(radians);
        auto m = Identity(); m.M22=c; m.M23=s; m.M32=-s; m.M33=c; return m;
    }
    static Matrix4x4 CreateRotationY(float radians) {
        float c=std::cos(radians), s=std::sin(radians);
        auto m = Identity(); m.M11=c; m.M13=-s; m.M31=s; m.M33=c; return m;
    }
    static Matrix4x4 CreateRotationZ(float radians) {
        float c=std::cos(radians), s=std::sin(radians);
        auto m = Identity(); m.M11=c; m.M12=s; m.M21=-s; m.M22=c; return m;
    }

    static Matrix4x4 CreateFromQuaternion(const Quaternion& q) {
        float xx=q.X*q.X, yy=q.Y*q.Y, zz=q.Z*q.Z;
        float xy=q.X*q.Y, xz=q.X*q.Z, yz=q.Y*q.Z;
        float wx=q.W*q.X, wy=q.W*q.Y, wz=q.W*q.Z;
        return {
            1-2*(yy+zz), 2*(xy+wz),   2*(xz-wy),   0,
            2*(xy-wz),   1-2*(xx+zz), 2*(yz+wx),   0,
            2*(xz+wy),   2*(yz-wx),   1-2*(xx+yy), 0,
            0,           0,           0,           1
        };
    }

    static Matrix4x4 CreateFromYawPitchRoll(float yaw, float pitch, float roll) {
        return CreateFromQuaternion(Quaternion::CreateFromYawPitchRoll(yaw,pitch,roll));
    }

    static Matrix4x4 CreateLookAt(Vector3 eye, Vector3 target, Vector3 up) {
        Vector3 zAxis = Vector3::Normalize(eye - target);
        Vector3 xAxis = Vector3::Normalize(Vector3::Cross(up, zAxis));
        Vector3 yAxis = Vector3::Cross(zAxis, xAxis);
        return {
            xAxis.X,             yAxis.X,             zAxis.X,             0,
            xAxis.Y,             yAxis.Y,             zAxis.Y,             0,
            xAxis.Z,             yAxis.Z,             zAxis.Z,             0,
            -Vector3::Dot(xAxis,eye), -Vector3::Dot(yAxis,eye), -Vector3::Dot(zAxis,eye), 1
        };
    }

    static Matrix4x4 CreatePerspectiveFieldOfView(float fov, float aspect, float near, float far) {
        float ys = 1.0f/std::tan(fov*0.5f);
        float xs = ys/aspect;
        float zn = far/(near-far);
        return {xs,0,0,0, 0,ys,0,0, 0,0,zn,-1, 0,0,near*zn,0};
    }

    static Matrix4x4 CreateOrthographic(float w, float h, float near, float far) {
        return {2/w,0,0,0, 0,2/h,0,0, 0,0,1/(near-far),0, 0,0,near/(near-far),1};
    }

    std::string ToString() const {
        std::ostringstream ss;
        ss << "{ {M11:"<<M11<<" M12:"<<M12<<" M13:"<<M13<<" M14:"<<M14<<"} "
           << "{M21:"<<M21<<" M22:"<<M22<<" M23:"<<M23<<" M24:"<<M24<<"} "
           << "{M31:"<<M31<<" M32:"<<M32<<" M33:"<<M33<<" M34:"<<M34<<"} "
           << "{M41:"<<M41<<" M42:"<<M42<<" M43:"<<M43<<" M44:"<<M44<<"} }";
        return ss.str();
    }
};

// --- Deferred Transform implementations ---

inline Vector2 Vector2::Transform(Vector2 p, const Matrix4x4& m) {
    return {p.X*m.M11+p.Y*m.M21+m.M41, p.X*m.M12+p.Y*m.M22+m.M42};
}
inline Vector2 Vector2::TransformNormal(Vector2 n, const Matrix4x4& m) {
    return {n.X*m.M11+n.Y*m.M21, n.X*m.M12+n.Y*m.M22};
}
inline Vector3 Vector3::Transform(Vector3 p, const Matrix4x4& m) {
    return {p.X*m.M11+p.Y*m.M21+p.Z*m.M31+m.M41,
            p.X*m.M12+p.Y*m.M22+p.Z*m.M32+m.M42,
            p.X*m.M13+p.Y*m.M23+p.Z*m.M33+m.M43};
}
inline Vector3 Vector3::TransformNormal(Vector3 n, const Matrix4x4& m) {
    return {n.X*m.M11+n.Y*m.M21+n.Z*m.M31,
            n.X*m.M12+n.Y*m.M22+n.Z*m.M32,
            n.X*m.M13+n.Y*m.M23+n.Z*m.M33};
}
inline Vector3 Vector3::Transform(Vector3 v, const Quaternion& q) {
    Vector3 u{q.X,q.Y,q.Z};
    float s = q.W;
    return 2.0f*Vector3::Dot(u,v)*u + (s*s - Vector3::Dot(u,u))*v + 2.0f*s*Vector3::Cross(u,v);
}
inline Vector4 Vector4::Transform(Vector2 p, const Matrix4x4& m) {
    return {p.X*m.M11+p.Y*m.M21+m.M41,
            p.X*m.M12+p.Y*m.M22+m.M42,
            p.X*m.M13+p.Y*m.M23+m.M43,
            p.X*m.M14+p.Y*m.M24+m.M44};
}
inline Vector4 Vector4::Transform(Vector3 p, const Matrix4x4& m) {
    return {p.X*m.M11+p.Y*m.M21+p.Z*m.M31+m.M41,
            p.X*m.M12+p.Y*m.M22+p.Z*m.M32+m.M42,
            p.X*m.M13+p.Y*m.M23+p.Z*m.M33+m.M43,
            p.X*m.M14+p.Y*m.M24+p.Z*m.M34+m.M44};
}
inline Vector4 Vector4::Transform(Vector4 v, const Matrix4x4& m) {
    return {v.X*m.M11+v.Y*m.M21+v.Z*m.M31+v.W*m.M41,
            v.X*m.M12+v.Y*m.M22+v.Z*m.M32+v.W*m.M42,
            v.X*m.M13+v.Y*m.M23+v.Z*m.M33+v.W*m.M43,
            v.X*m.M14+v.Y*m.M24+v.Z*m.M34+v.W*m.M44};
}
inline Vector4 Vector4::Transform(Vector4 v, const Quaternion& q) {
    Vector3 u{q.X,q.Y,q.Z};
    Vector3 vxyz{v.X,v.Y,v.Z};
    Vector3 r = Vector3::Transform(vxyz,q);
    return {r.X,r.Y,r.Z,v.W};
}
inline Quaternion Quaternion::CreateFromRotationMatrix(const Matrix4x4& m) {
    float trace = m.M11+m.M22+m.M33;
    if (trace > 0) {
        float s = 0.5f/std::sqrt(trace+1.0f);
        return {(m.M23-m.M32)*s,(m.M31-m.M13)*s,(m.M12-m.M21)*s,0.25f/s};
    } else if (m.M11>m.M22 && m.M11>m.M33) {
        float s = 2.0f*std::sqrt(1.0f+m.M11-m.M22-m.M33);
        return {0.25f*s,(m.M12+m.M21)/s,(m.M31+m.M13)/s,(m.M23-m.M32)/s};
    } else if (m.M22>m.M33) {
        float s = 2.0f*std::sqrt(1.0f+m.M22-m.M11-m.M33);
        return {(m.M12+m.M21)/s,0.25f*s,(m.M23+m.M32)/s,(m.M31-m.M13)/s};
    } else {
        float s = 2.0f*std::sqrt(1.0f+m.M33-m.M11-m.M22);
        return {(m.M31+m.M13)/s,(m.M23+m.M32)/s,0.25f*s,(m.M12-m.M21)/s};
    }
}

} // namespace System::Numerics
