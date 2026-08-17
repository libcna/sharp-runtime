// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include "System/Numerics/Vector3.hpp"
#include "System/Numerics/Vector4.hpp"
#include "System/Numerics/Matrix4x4.hpp"
#include "System/Numerics/Quaternion.hpp"

namespace System::Numerics {

/** <summary>Represents a plane in three-dimensional space defined by a normal vector and a distance.</summary> */
struct Plane {
    Vector3 Normal{};
    float D{0.0f};

    Plane() = default;
    Plane(float x, float y, float z, float d) : Normal(x,y,z), D(d) {}
    Plane(Vector3 normal, float d) : Normal(normal), D(d) {}
    Plane(Vector4 v) : Normal(v.X,v.Y,v.Z), D(v.W) {}

    /** @brief Returns true if this plane's Normal and D exactly equal @p o's. */
    bool Equals(const Plane& o) const { return Normal==o.Normal && D==o.D; }
    bool operator==(const Plane& o) const { return Equals(o); }
    bool operator!=(const Plane& o) const { return !Equals(o); }

    /** @brief Returns a string representation of this plane in the form "{Normal:... D:...}". */
    std::string ToString() const {
        std::ostringstream ss;
        ss.imbue(std::locale::classic());
        ss<<"{Normal:"<<Normal.ToString()<<" D:"<<D<<"}";
        return ss.str();
    }

    /** <summary>Creates a Plane from three points on the plane.</summary> */
    static Plane CreateFromVertices(Vector3 p1, Vector3 p2, Vector3 p3) {
        Vector3 n = Vector3::Normalize(Vector3::Cross(p2-p1, p3-p1));
        return {n, -Vector3::Dot(n,p1)};
    }

    /** <summary>Returns the dot product of a plane and a 4D vector.</summary> */
    static float Dot(const Plane& plane, Vector4 value) {
        return plane.Normal.X*value.X + plane.Normal.Y*value.Y
             + plane.Normal.Z*value.Z + plane.D*value.W;
    }

    /** <summary>Returns the dot product of a plane and a normal vector.</summary> */
    static float DotNormal(const Plane& plane, Vector3 value) {
        return Vector3::Dot(plane.Normal, value);
    }

    /** <summary>Returns the dot product of a plane and a coordinate vector.</summary> */
    static float DotCoordinate(const Plane& plane, Vector3 value) {
        return Vector3::Dot(plane.Normal, value) + plane.D;
    }

    /**
     * <summary>Normalizes the normal vector of a plane, scaling D by the same factor.</summary>
     *
     * @note **`Plane::Normalize` genuinely is not `Vector3::Normalize`, and SR-AUD-276 was right
     * to ask** (#2173/#2175). But the answer is not the one the plan expected. .NET's
     * `Plane.Normalize` has **no already-normalized epsilon fast path** — that belief, recorded
     * here before `/rv` was available, is wrong for .NET 11. What it has is DirectXMath's
     * infinity mask (`Plane.cs:127-138`):
     *
     * ```csharp
     * Vector128<float> lengthSquared = Vector128.Create(value.Normal.LengthSquared());
     * return Vector128.AndNot(
     *     (value.AsVector128() / Vector128.Sqrt(lengthSquared)),
     *     Vector128.Equals(lengthSquared, Vector128<float>.PositiveInfinity)
     * ).AsPlane();
     * ```
     *
     * So all four lanes — `Normal.X`, `Normal.Y`, `Normal.Z` and `D` — are divided by
     * `sqrt(Normal.LengthSquared())` unconditionally, and then **every lane is forced to zero if
     * and only if the squared length was `+Infinity`**. That single guard is the whole
     * difference from the vector types, and it is about *overflow*, not about smallness.
     *
     * Consequences, all now matching .NET and pinned by test:
     *   - a **zero** normal divides by zero: the three normal components become `NaN` (`0/0`) and
     *     `D` becomes `±Infinity`, or `NaN` if `D` is zero too;
     *   - a normal whose squared length **overflows** (components near `FLT_MAX`) returns the
     *     **all-zero plane**, which is the one case where `Plane` is deliberately gentler than
     *     `Vector3::Normalize`;
     *   - a **NaN** normal propagates NaN, which this port already did;
     *   - `{1e-11,0,0}` now normalizes instead of being returned unchanged. The old `< 1e-10f`
     *     guard was roughly twelve orders of magnitude wide and had no counterpart in .NET at
     *     all.
     *
     * The module used to hold three answers to one structural question — `> 0` in the vectors,
     * `< 1e-10f` here, and `> 1.192092896e-7f` on the square in `Quaternion::Inverse`. Two of
     * them are gone: the vectors and `Plane` now carry .NET's, and `Quaternion::Inverse`'s
     * threshold is the one that was always documented as matching a .NET constant.
     */
    static Plane Normalize(const Plane& plane) {
        const float lengthSquared = plane.Normal.LengthSquared();
        if (lengthSquared == std::numeric_limits<float>::infinity()) return Plane{0.0f, 0.0f, 0.0f, 0.0f};
        const float length = std::sqrt(lengthSquared);
        return {plane.Normal / length, plane.D / length};
    }

    /** <summary>Transforms a plane by a matrix.</summary> */
    static Plane Transform(const Plane& plane, const Matrix4x4& m) {
        Matrix4x4 inv;
        Matrix4x4::Invert(m, inv);
        Matrix4x4 t = Matrix4x4::Transpose(inv);
        Vector4 v = Vector4::Transform(Vector4{plane.Normal.X,plane.Normal.Y,plane.Normal.Z,plane.D}, t);
        return {v.X,v.Y,v.Z,v.W};
    }

    /** <summary>Transforms a plane by a quaternion rotation.</summary> */
    static Plane Transform(const Plane& plane, const Quaternion& q) {
        return Transform(plane, Matrix4x4::CreateFromQuaternion(q));
    }
};

} // namespace System::Numerics
