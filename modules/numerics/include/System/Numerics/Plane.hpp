// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
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
     * @note **This does NOT behave like `Vector3::Normalize`, although SR-AUD-276 groups them**
     * (#2173/#2175). Measured differences, all reachable:
     *   - the guard is `Length() < 1e-10f`, roughly twelve orders of magnitude wider than the
     *     vectors' `Length() > 0`: `{1e-11,0,0}` is returned **unnormalized**, while
     *     `{1e-9,0,0}` normalizes to `(1,0,0)` with `D` scaled to `1e+09`;
     *   - `NaN < 1e-10f` is **false**, so a NaN normal falls through and **propagates NaN to all
     *     four fields** — the exact opposite of `Vector3::Normalize`, which swallows it;
     *   - a zero normal is returned unchanged, which is the only case the two agree on.
     *
     * The module therefore holds three answers to one structural question: `> 0` here in the
     * vectors, `< 1e-10f` here, and `> 1.192092896e-7f` on the **square** in
     * `Quaternion::Inverse` (the only one documented as matching a .NET threshold).
     *
     * .NET's `Plane.Normalize` is believed to carry an *already-normalized* fast path
     * (`|lengthSquared - 1| < epsilon`) that the vector types do not have, and then to divide
     * unconditionally. Measured here, an already-unit plane does come back **bit-identical**, so
     * no divergence from that fast path is observable — but the rest is a reading rather than a
     * measurement and `/rv` is absent. **#2175** owns it; the behaviour above is pinned by test.
     */
    static Plane Normalize(const Plane& plane) {
        float len = plane.Normal.Length();
        if (len < 1e-10f) return plane;
        return {plane.Normal / len, plane.D / len};
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
