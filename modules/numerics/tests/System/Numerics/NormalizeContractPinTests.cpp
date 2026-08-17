// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tickets #2173 (SR-AUD-276, compatible subpart) and #2175 (the remainder, LANDED 2026-08-17).
//
// This file used to pin behaviour that was deliberately NOT repaired: every test asserted what
// the answer *was*, not that it was right, so that #2175 could not be answered silently. #2175
// has now been answered, from the reference rather than from a reading, and the tests below hold
// the answers instead of holding the gate shut.
//
// WHAT THE REFERENCE SAID, and how much of the old note it overturned:
//
//   * Vector2/3/4 and Quaternion all divide UNCONDITIONALLY --
//     `public static Vector3 Normalize(Vector3 value) => value / value.Length();`
//     (Vector2.cs:822, Vector3.cs:852, Vector4.cs:902, Quaternion.cs:380). The auditor's reading
//     was right, and this port's `l > 0 ? v / l : v` was wider than "the length is zero" in three
//     separate ways.
//   * Plane::Normalize is genuinely different, so SR-AUD-276 was right to ask -- but NOT for the
//     reason recorded. .NET has NO already-normalized epsilon fast path (Plane.cs:127-138). It
//     divides all four lanes unconditionally and then zeroes every lane iff the squared length
//     was +Infinity, which is DirectXMath's OVERFLOW mask. The old `< 1e-10f` guard had no
//     counterpart in .NET at all.
//   * The two dependents follow .NET too: Plane.CreateFromVertices (Plane.cs:84) and
//     Matrix4x4.Impl.CreateLookToLeftHanded (Matrix4x4.Impl.cs:365-366) both call the
//     unconditional Vector3.Normalize, so degenerate input is NaN there as well.
//
// See docs/SystemNumericsNamespaceReviewPlan.md section 6.2 and
// docs/Migration-NumericsNormalizeDividesUnconditionally.md.
#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "System/Numerics/Matrix4x4.hpp"
#include "System/Numerics/Plane.hpp"
#include "System/Numerics/Quaternion.hpp"
#include "System/Numerics/Vector2.hpp"
#include "System/Numerics/Vector3.hpp"
#include "System/Numerics/Vector4.hpp"

using System::Numerics::Matrix4x4;
using System::Numerics::Plane;
using System::Numerics::Quaternion;
using System::Numerics::Vector2;
using System::Numerics::Vector3;
using System::Numerics::Vector4;

namespace {
constexpr float kInf = std::numeric_limits<float>::infinity();
const float kNaN = std::numeric_limits<float>::quiet_NaN();

// Runtime-fed so a compiler cannot fold the guard away and answer a different question than the
// shipped code does.
volatile float g_zero = 0.0f;
volatile float g_one  = 1.0f;
} // namespace

// ---------------------------------------------------------------------------
// The vector guard is gone. All three classes it used to catch now divide.
// ---------------------------------------------------------------------------

TEST(NormalizeContractTests, Fix2175_TheZeroVectorNormalizesToNaN) {
    // The headline change, and the one the ticket weighed for so long: a previously-finite
    // answer becomes NaN with no diagnostic. That IS the contract -- normalizing a direction
    // that has no direction is a caller error, and NaN makes it visible where returning the
    // zero vector silently produced something that is not a unit vector.
    const float z = g_zero;
    const Vector3 r = Vector3::Normalize({z, z, z});
    EXPECT_TRUE(std::isnan(r.X));
    EXPECT_TRUE(std::isnan(r.Y));
    EXPECT_TRUE(std::isnan(r.Z));

    // The sign of a negative zero cannot survive 0/0, so the old "signed zeros preserved" pin
    // is gone with the guard rather than merely weakened.
    const Vector3 neg = Vector3::Normalize({-z, -z, -z});
    EXPECT_TRUE(std::isnan(neg.X));
}

TEST(NormalizeContractTests, Fix2175_ANaNComponentIsPropagatedNotSwallowed) {
    // `NaN > 0` is false, so the old guard fired and returned the vector untouched -- the NaN
    // never reached Y and Z. Now Length() is NaN and every component is divided by it.
    const float z = g_zero;
    const Vector3 r = Vector3::Normalize({kNaN, z, z});
    EXPECT_TRUE(std::isnan(r.X));
    EXPECT_TRUE(std::isnan(r.Y)) << "the NaN must reach the other components";
    EXPECT_TRUE(std::isnan(r.Z));
}

TEST(NormalizeContractTests, Fix2175_AnAllInfiniteVectorIsNaNThroughout) {
    const float o = g_one;
    const Vector3 r = Vector3::Normalize({kInf * o, kNaN * o, -kInf * o});
    EXPECT_TRUE(std::isnan(r.X));
    EXPECT_TRUE(std::isnan(r.Y));
    EXPECT_TRUE(std::isnan(r.Z));
}

TEST(NormalizeContractTests, Fix2175_AnUnderflowingButNormalizableVectorNowNormalizes) {
    // The widest of the three classes the old guard caught, and the one reachable with ordinary
    // data: LengthSquared underflows to +0 for components below roughly 1e-22, so a real
    // direction was returned unnormalized.
    //
    // Note the answer is +Infinity, not NaN, and the distinction is worth pinning: Length() is
    // sqrt(+0) = +0, and a NONZERO component divided by +0 is an infinity. Only the components
    // that are themselves zero give NaN. So an underflowing vector and a zero vector -- which
    // the old guard treated identically -- now differ, exactly as they do in .NET.
    const float t = 1e-25f * g_one;
    const Vector3 v{t, t, t};
    ASSERT_FLOAT_EQ(v.LengthSquared(), 0.0f) << "precondition: the square underflows";
    const Vector3 r = Vector3::Normalize(v);
    EXPECT_TRUE(std::isinf(r.X)) << "t/+0 is an infinity, not a NaN";
    EXPECT_FALSE(std::signbit(r.X));
    EXPECT_TRUE(std::isinf(r.Z));

    // A sign-mixed underflowing vector keeps its per-component sign.
    const Vector3 mixed = Vector3::Normalize({t, -t, 0.0f});
    EXPECT_TRUE(std::isinf(mixed.X));
    EXPECT_TRUE(std::isinf(mixed.Y));
    EXPECT_TRUE(std::signbit(mixed.Y));
    EXPECT_TRUE(std::isnan(mixed.Z)) << "the zero component is 0/0";
}

TEST(NormalizeContractTests, OverflowStillNormalizesToZero_SharedWithDotNet) {
    // NOT caused by the old guard, and unchanged: LengthSquared overflows to +inf, so the
    // division underflows to zero. Recorded so #2175 is not credited with changing this too.
    const float m = std::numeric_limits<float>::max() * g_one;
    const Vector3 r = Vector3::Normalize({m, m, m});
    EXPECT_FLOAT_EQ(r.X, 0.0f);
    EXPECT_FLOAT_EQ(r.Y, 0.0f);
    EXPECT_FLOAT_EQ(r.Z, 0.0f);
}

TEST(NormalizeContractTests, Fix2175_Vector2AndVector4BehaveIdenticallyToVector3) {
    const float z = g_zero;
    const Vector2 v2 = Vector2::Normalize({z, z});
    const Vector2 v2n = Vector2::Normalize({kNaN, z});
    EXPECT_TRUE(std::isnan(v2.X));
    EXPECT_TRUE(std::isnan(v2n.X));
    EXPECT_TRUE(std::isnan(v2n.Y));

    const Vector4 v4 = Vector4::Normalize({z, z, z, z});
    const Vector4 v4n = Vector4::Normalize({kNaN, z, z, z});
    EXPECT_TRUE(std::isnan(v4.X));
    EXPECT_TRUE(std::isnan(v4n.X));
    EXPECT_TRUE(std::isnan(v4n.W));
}

TEST(NormalizeContractTests, OrdinaryVectorsStillNormalizeExactly) {
    // Invariance: nothing about the ordinary path may move. This is what makes the change a
    // narrowing of degenerate input rather than a rewrite of the arithmetic.
    const float o = g_one;
    const Vector3 r = Vector3::Normalize({3.0f * o, 4.0f * o, 0.0f});
    EXPECT_FLOAT_EQ(r.X, 0.6f);
    EXPECT_FLOAT_EQ(r.Y, 0.8f);
    EXPECT_FLOAT_EQ(r.Z, 0.0f);
    const Vector3 u = Vector3::Normalize({o, 0.0f, 0.0f});
    EXPECT_FLOAT_EQ(u.X, 1.0f);
    const Vector2 t = Vector2::Normalize({3.0f * o, 4.0f * o});
    EXPECT_FLOAT_EQ(t.X, 0.6f);
    const Vector4 q = Vector4::Normalize({o, o, o, o});
    EXPECT_FLOAT_EQ(q.X, 0.5f);
}

// ---------------------------------------------------------------------------
// Plane::Normalize keeps ONE guard, and it is .NET's -- an overflow mask, not
// an epsilon and not an already-normalized fast path.
// ---------------------------------------------------------------------------

TEST(NormalizeContractTests, Fix2175_PlaneKeepsExactlyOneGuardAndItIsTheInfinityMask) {
    // Plane.cs:134-137 -- AndNot(value / Sqrt(lengthSquared), Equals(lengthSquared, +Infinity)).
    // A squared length that overflows returns the ALL-ZERO plane, including D. This is the only
    // case where Plane is gentler than Vector3::Normalize, and it is .NET's own choice.
    const float m = std::numeric_limits<float>::max() * g_one;
    const Plane r = Plane::Normalize(Plane{m, m, m, 7.0f * g_one});
    ASSERT_EQ(Vector3(m, m, m).LengthSquared(), kInf) << "precondition: the square overflows";
    EXPECT_FLOAT_EQ(r.Normal.X, 0.0f);
    EXPECT_FLOAT_EQ(r.Normal.Y, 0.0f);
    EXPECT_FLOAT_EQ(r.Normal.Z, 0.0f);
    EXPECT_FLOAT_EQ(r.D, 0.0f) << "D is masked to zero too, not merely left alone";

    // THE CASE THE MASK EXISTS FOR, and the only one where it is observable. With all-finite
    // components the division by +Infinity already gives zero, so removing the mask changes
    // nothing there. An INFINITE component divides to inf/inf = NaN, and the mask is what turns
    // that into the zero plane instead. Same for an infinite D.
    const Plane infComponent = Plane::Normalize(Plane{kInf * g_one, g_zero, g_zero, 7.0f * g_one});
    EXPECT_FALSE(std::isnan(infComponent.Normal.X)) << "without the mask this is inf/inf = NaN";
    EXPECT_FLOAT_EQ(infComponent.Normal.X, 0.0f);
    EXPECT_FLOAT_EQ(infComponent.D, 0.0f);

    const Plane infD = Plane::Normalize(Plane{m, m, m, kInf * g_one});
    EXPECT_FALSE(std::isnan(infD.D));
    EXPECT_FLOAT_EQ(infD.D, 0.0f);
}

TEST(NormalizeContractTests, Fix2175_ThePlaneEpsilonIsGoneSoASmallNormalNormalizes) {
    // The old guard was `Length() < 1e-10f`, roughly twelve orders of magnitude wide and with no
    // counterpart in .NET. `{1e-11,0,0}` was returned unnormalized; it now normalizes, and D is
    // scaled by the same factor.
    const float o = g_one, z = g_zero;
    const Plane small = Plane::Normalize(Plane{1e-11f * o, z, z, o});
    EXPECT_FLOAT_EQ(small.Normal.X, 1.0f);
    EXPECT_FLOAT_EQ(small.D, 1e11f);

    // The value just above the old threshold is unchanged, so the repair is a widening at the
    // bottom rather than a shift of the whole scale.
    const Plane above = Plane::Normalize(Plane{1e-9f * o, z, z, o});
    EXPECT_FLOAT_EQ(above.Normal.X, 1.0f);
    EXPECT_FLOAT_EQ(above.D, 1e9f);
}

TEST(NormalizeContractTests, Fix2175_APlaneWithAZeroNormalDividesByZero) {
    // Previously the plane came back unchanged, D unscaled. Now the normal is 0/0 = NaN and D is
    // 5/0 = +Infinity, because only an INFINITE squared length is masked -- a zero one is not.
    const float z = g_zero;
    const Plane r = Plane::Normalize(Plane{z, z, z, 5.0f * g_one});
    EXPECT_TRUE(std::isnan(r.Normal.X));
    EXPECT_TRUE(std::isnan(r.Normal.Y));
    EXPECT_TRUE(std::isnan(r.Normal.Z));
    EXPECT_TRUE(std::isinf(r.D));
    EXPECT_FALSE(std::signbit(r.D));

    // With D zero as well, every field is NaN.
    const Plane allZero = Plane::Normalize(Plane{z, z, z, z});
    EXPECT_TRUE(std::isnan(allZero.D));
}

TEST(NormalizeContractTests, PlaneStillPropagatesNaN) {
    // Unchanged: `NaN < 1e-10f` was false, so the old code already fell through and divided.
    // Now there is no threshold to fall through, and the answer is the same.
    const float o = g_one, z = g_zero;
    const Plane r = Plane::Normalize(Plane{kNaN, z, z, o});
    EXPECT_TRUE(std::isnan(r.Normal.X));
    EXPECT_TRUE(std::isnan(r.Normal.Y));
    EXPECT_TRUE(std::isnan(r.D));
}

TEST(NormalizeContractTests, PlaneAlreadyUnitPlaneComesBackBitIdentical) {
    // .NET was believed to short-circuit an already-normalized plane. It does not, and it does
    // not need to: the division reproduces the input exactly. This is why the removed fast path
    // was never observable, and it stays asserted so the claim is not merely retired.
    const float o = g_one, z = g_zero;
    const Plane in{o, z, z, 3.0f * o};
    const Plane out = Plane::Normalize(in);
    EXPECT_FLOAT_EQ(out.Normal.X, in.Normal.X);
    EXPECT_FLOAT_EQ(out.Normal.Y, in.Normal.Y);
    EXPECT_FLOAT_EQ(out.Normal.Z, in.Normal.Z);
    EXPECT_FLOAT_EQ(out.D, in.D);
}

// ---------------------------------------------------------------------------
// The two dependents inherit the change, and .NET's do too
// ---------------------------------------------------------------------------

TEST(NormalizeContractTests, Fix2175_CreateFromVerticesOnDegenerateInputIsNowNaN) {
    // Plane.cs:84 calls the unconditional Vector3.Normalize, so .NET produces NaN here as well.
    // Previously this port returned an unoriented plane with a finite D, which looked usable and
    // was not.
    const float z = g_zero, o = g_one;
    const Plane identical = Plane::CreateFromVertices({z, z, z}, {z, z, z}, {z, z, z});
    EXPECT_TRUE(std::isnan(identical.Normal.X));
    EXPECT_TRUE(std::isnan(identical.D)) << "D is -Dot(NaN normal, p1)";

    const Plane collinear = Plane::CreateFromVertices({z, z, z}, {o, z, z}, {2.0f * o, z, z});
    EXPECT_TRUE(std::isnan(collinear.Normal.X));
    EXPECT_TRUE(std::isnan(collinear.Normal.Y));
    EXPECT_TRUE(std::isnan(collinear.Normal.Z));

    // A well-formed triangle is untouched.
    const Plane good = Plane::CreateFromVertices({z, z, z}, {o, z, z}, {z, o, z});
    EXPECT_FLOAT_EQ(good.Normal.Z, 1.0f);
    EXPECT_FLOAT_EQ(good.D, 0.0f);
}

TEST(NormalizeContractTests, Fix2175_CreateLookAtWithEyeEqualToTargetIsNowNaN) {
    // Matrix4x4.Impl.cs:365-366 normalizes the camera direction and the cross product with the
    // up vector, both unconditionally. A silently SINGULAR view matrix -- which is what this
    // port produced -- is the worst of the three possible answers, because it renders nothing
    // and reports nothing.
    const float z = g_zero, o = g_one;
    const Matrix4x4 m = Matrix4x4::CreateLookAt({z, z, z}, {z, z, z}, {z, o, z});
    EXPECT_TRUE(std::isnan(m.M11));
    EXPECT_TRUE(std::isnan(m.M22));
    EXPECT_TRUE(std::isnan(m.M33));

    // up parallel to the forward axis: the cross product is zero, same outcome.
    const Matrix4x4 par = Matrix4x4::CreateLookAt({z, z, 5.0f * o}, {z, z, z}, {z, z, o});
    EXPECT_TRUE(std::isnan(par.M11));

    // An ordinary camera is untouched.
    const Matrix4x4 ok = Matrix4x4::CreateLookAt({z, z, 5.0f * o}, {z, z, z}, {z, o, z});
    EXPECT_FALSE(std::isnan(ok.M11));
    EXPECT_FLOAT_EQ(ok.M44, 1.0f);
}

TEST(NormalizeContractTests, QuaternionNormalizeWasAlwaysRightAndIsUnchanged) {
    // Quaternion.cs:380 -- already unconditional before #2175, and the strongest in-repository
    // evidence for what the other four had to become. One member of the family followed .NET and
    // four did not; now all five do.
    const Quaternion q = Quaternion::Normalize({0.0f, 0.0f, 0.0f, 0.0f});
    EXPECT_TRUE(std::isnan(q.X));
    EXPECT_TRUE(std::isnan(q.Y));
    EXPECT_TRUE(std::isnan(q.Z));
    EXPECT_TRUE(std::isnan(q.W));
}

TEST(NormalizeContractTests, Fix2175_TheModuleNoLongerHoldsThreeThresholdsForOneQuestion) {
    // The old pin recorded three mutually inconsistent answers to one structural question. Two
    // are gone. The survivor is Quaternion::Inverse's, which was always documented as matching a
    // .NET constant -- it answers a DIFFERENT question (when is a quaternion non-invertible) and
    // is deliberately untouched.
    const float o = g_one, z = g_zero;

    // 1 and 2 now agree: the same tiny magnitude normalizes through both doors.
    EXPECT_FLOAT_EQ(Vector3::Normalize({1e-11f * o, z, z}).X, 1.0f);
    EXPECT_FLOAT_EQ(Plane::Normalize(Plane{1e-11f * o, z, z, o}).Normal.X, 1.0f);

    // 3. Quaternion::Inverse: > 1.192092896e-7f on the SQUARE, unchanged.
    const Quaternion tiny{1e-5f * o, z, z, z};
    ASSERT_LT(tiny.LengthSquared(), 1.192092896e-7f);
    const Quaternion inv = Quaternion::Inverse(tiny);
    EXPECT_FLOAT_EQ(inv.X, 0.0f);
    EXPECT_FLOAT_EQ(inv.W, 0.0f);
}
