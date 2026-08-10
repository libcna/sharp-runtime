// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2173 (SR-AUD-276, compatible subpart).
//
// EVERY TEST IN THIS FILE PINS BEHAVIOUR THAT IS DELIBERATELY NOT REPAIRED. None of them asserts
// that the current answer is correct; they assert what it *is*, so that #2175 -- which owns the
// question of whether Vector2/3/4 and Plane should divide unconditionally and yield NaN, the way
// .NET is believed to -- cannot be answered silently, and so that the three mutually inconsistent
// thresholds this module holds are visible in the test output rather than only in a document.
//
// Why #2175 is not implemented here: SR-AUD-276 carries no managed probe. Its .NET claim is the
// auditor's reading of the .NET source, corroborated by this module's own Quaternion.hpp
// doc-comment -- good corroboration, still a reading -- and it does not cover Plane::Normalize,
// whose .NET counterpart is believed to carry an already-normalized fast path the vector types
// lack. Changing a previously-accepted input's numeric answer from finite to NaN, with no
// diagnostic, in geometry code, is the class of change CLAUDE.md records as approved Groups A-D of
// docs/RemainingApprovalDecisions.md. See docs/SystemNumericsNamespaceReviewPlan.md section 6.2.
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
// The vector guard is "length not > 0", which catches three separate classes
// ---------------------------------------------------------------------------

TEST(PIN_NormalizeContractTests, Vector3_ZeroVectorIsReturnedUnchanged) {
    const float z = g_zero;
    const Vector3 r = Vector3::Normalize({z, z, z});
    EXPECT_FLOAT_EQ(r.X, 0.0f);
    EXPECT_FLOAT_EQ(r.Y, 0.0f);
    EXPECT_FLOAT_EQ(r.Z, 0.0f);
    EXPECT_FALSE(std::isnan(r.X)) << "PIN: .NET is believed to give NaN here; #2175 owns it";
}

TEST(PIN_NormalizeContractTests, Vector3_NegativeZeroIsPreservedNotFlattened) {
    // The audit records "finite zero"; measured, the SIGN survives.
    const float z = g_zero;
    const Vector3 r = Vector3::Normalize({-z, -z, -z});
    EXPECT_TRUE(std::signbit(r.X));
    EXPECT_TRUE(std::signbit(r.Y));
    EXPECT_TRUE(std::signbit(r.Z));
}

TEST(PIN_NormalizeContractTests, Vector3_ANaNComponentIsSwallowedNotPropagated) {
    // NaN > 0 is false, so the guard fires and the vector comes back untouched: the NaN does NOT
    // reach Y and Z. This is the premise correction -- the finding describes a *zero* guard.
    const float z = g_zero;
    const Vector3 r = Vector3::Normalize({kNaN, z, z});
    EXPECT_TRUE(std::isnan(r.X));
    EXPECT_FALSE(std::isnan(r.Y));
    EXPECT_FALSE(std::isnan(r.Z));
    EXPECT_FLOAT_EQ(r.Y, 0.0f);
}

TEST(PIN_NormalizeContractTests, Vector3_AnAllInfiniteVectorIsTreatedAsDegenerate) {
    // Length() is NaN (inf/inf), so the guard fires and an all-infinite vector is returned as is.
    const float o = g_one;
    const Vector3 r = Vector3::Normalize({kInf * o, kNaN * o, -kInf * o});
    EXPECT_TRUE(std::isinf(r.X));
    EXPECT_TRUE(std::isnan(r.Y));
    EXPECT_TRUE(std::isinf(r.Z));
    EXPECT_TRUE(std::signbit(r.Z));
}

TEST(PIN_NormalizeContractTests, Vector3_AnUnderflowingButNormalizableVectorIsReturnedUnnormalized) {
    // LengthSquared underflows to +0 for components below roughly 1e-22, so a real direction --
    // not degenerate geometry at all -- comes back unnormalized. This is reachable with ordinary
    // data and is the widest of the three classes the guard catches.
    const float t = 1e-25f * g_one;
    const Vector3 v{t, t, t};
    ASSERT_FLOAT_EQ(v.LengthSquared(), 0.0f) << "precondition: the square underflows";
    const Vector3 r = Vector3::Normalize(v);
    EXPECT_FLOAT_EQ(r.X, t);
    EXPECT_NE(r.Length(), 1.0f) << "PIN: returned unnormalized";
}

TEST(PIN_NormalizeContractTests, Vector3_OverflowNormalizesToZero_SharedWithDotNet) {
    // NOT caused by the guard, and .NET is believed to do the same: LengthSquared overflows to
    // +inf, so the division underflows to zero. Recorded so a future repair of the guard is not
    // credited with fixing this too.
    const float m = std::numeric_limits<float>::max() * g_one;
    const Vector3 r = Vector3::Normalize({m, m, m});
    EXPECT_FLOAT_EQ(r.X, 0.0f);
    EXPECT_FLOAT_EQ(r.Y, 0.0f);
    EXPECT_FLOAT_EQ(r.Z, 0.0f);
}

TEST(PIN_NormalizeContractTests, Vector2AndVector4_BehaveIdenticallyToVector3) {
    const float z = g_zero;
    const Vector2 v2 = Vector2::Normalize({z, z});
    const Vector2 v2n = Vector2::Normalize({kNaN, z});
    EXPECT_FLOAT_EQ(v2.X, 0.0f);
    EXPECT_TRUE(std::isnan(v2n.X));
    EXPECT_FALSE(std::isnan(v2n.Y));

    const Vector4 v4 = Vector4::Normalize({z, z, z, z});
    const Vector4 v4n = Vector4::Normalize({kNaN, z, z, z});
    EXPECT_FLOAT_EQ(v4.X, 0.0f);
    EXPECT_TRUE(std::isnan(v4n.X));
    EXPECT_FALSE(std::isnan(v4n.W));
}

TEST(PIN_NormalizeContractTests, OrdinaryVectorsStillNormalizeExactly) {
    // Invariance: nothing about the ordinary path may move.
    const float o = g_one;
    const Vector3 r = Vector3::Normalize({3.0f * o, 4.0f * o, 0.0f});
    EXPECT_FLOAT_EQ(r.X, 0.6f);
    EXPECT_FLOAT_EQ(r.Y, 0.8f);
    EXPECT_FLOAT_EQ(r.Z, 0.0f);
    const Vector3 u = Vector3::Normalize({o, 0.0f, 0.0f});
    EXPECT_FLOAT_EQ(u.X, 1.0f);
}

// ---------------------------------------------------------------------------
// Plane::Normalize is a DIFFERENT guard, and the finding groups the two wrongly
// ---------------------------------------------------------------------------

TEST(PIN_NormalizeContractTests, Plane_ThresholdIsTwelveOrdersOfMagnitudeWiderThanTheVectors) {
    const float o = g_one, z = g_zero;
    // Below 1e-10: returned unnormalized, even though the direction is perfectly representable.
    const Plane below = Plane::Normalize(Plane{1e-11f * o, z, z, o});
    EXPECT_FLOAT_EQ(below.Normal.X, 1e-11f);
    EXPECT_FLOAT_EQ(below.D, 1.0f);
    // Above 1e-10: normalized, and D is scaled by the same factor -- to 1e+09.
    const Plane above = Plane::Normalize(Plane{1e-9f * o, z, z, o});
    EXPECT_FLOAT_EQ(above.Normal.X, 1.0f);
    EXPECT_FLOAT_EQ(above.D, 1e9f);
}

TEST(PIN_NormalizeContractTests, Plane_PropagatesNaNWhereVector3SwallowsIt) {
    // NaN < 1e-10f is false, so Plane falls through and divides -- the exact opposite of
    // Vector3::Normalize. One finding, two contradictory behaviours.
    const float o = g_one, z = g_zero;
    const Plane r = Plane::Normalize(Plane{kNaN, z, z, o});
    EXPECT_TRUE(std::isnan(r.Normal.X));
    EXPECT_TRUE(std::isnan(r.Normal.Y));
    EXPECT_TRUE(std::isnan(r.Normal.Z));
    EXPECT_TRUE(std::isnan(r.D));

    const Vector3 v = Vector3::Normalize({kNaN, z, z});
    EXPECT_FALSE(std::isnan(v.Y)) << "the contrast is the point";
}

TEST(PIN_NormalizeContractTests, Plane_ZeroNormalIsTheOnlyCaseTheTwoAgreeOn) {
    const float z = g_zero;
    const Plane r = Plane::Normalize(Plane{z, z, z, 5.0f * g_one});
    EXPECT_FLOAT_EQ(r.Normal.X, 0.0f);
    EXPECT_FLOAT_EQ(r.D, 5.0f) << "PIN: D is not scaled because the guard fired";
}

TEST(PIN_NormalizeContractTests, Plane_AlreadyUnitPlaneComesBackBitIdentical) {
    // .NET is believed to short-circuit an already-normalized plane; measured, this port's
    // division reproduces it exactly anyway, so no divergence from that fast path is observable.
    const float o = g_one, z = g_zero;
    const Plane in{o, z, z, 3.0f * o};
    const Plane out = Plane::Normalize(in);
    EXPECT_FLOAT_EQ(out.Normal.X, in.Normal.X);
    EXPECT_FLOAT_EQ(out.Normal.Y, in.Normal.Y);
    EXPECT_FLOAT_EQ(out.Normal.Z, in.Normal.Z);
    EXPECT_FLOAT_EQ(out.D, in.D);
}

// ---------------------------------------------------------------------------
// The two dependents, and the module's own contrasting control
// ---------------------------------------------------------------------------

TEST(PIN_NormalizeContractTests, CreateFromVerticesOnDegenerateInputYieldsAnUnorientedPlane) {
    const float z = g_zero, o = g_one;
    const Plane identical = Plane::CreateFromVertices({z, z, z}, {z, z, z}, {z, z, z});
    EXPECT_FLOAT_EQ(identical.Normal.X, 0.0f);
    EXPECT_FLOAT_EQ(identical.Normal.Y, 0.0f);
    EXPECT_FLOAT_EQ(identical.Normal.Z, 0.0f);
    EXPECT_FALSE(std::isnan(identical.D));

    const Plane collinear = Plane::CreateFromVertices({z, z, z}, {o, z, z}, {2.0f * o, z, z});
    EXPECT_FLOAT_EQ(collinear.Normal.X, 0.0f);
    EXPECT_FLOAT_EQ(collinear.Normal.Y, 0.0f);
    EXPECT_FLOAT_EQ(collinear.Normal.Z, 0.0f);
}

TEST(PIN_NormalizeContractTests, CreateLookAtWithEyeEqualToTargetYieldsASingularMatrix) {
    const float z = g_zero, o = g_one;
    const Matrix4x4 m = Matrix4x4::CreateLookAt({z, z, z}, {z, z, z}, {z, o, z});
    EXPECT_FLOAT_EQ(m.M11, 0.0f);
    EXPECT_FLOAT_EQ(m.M22, 0.0f);
    EXPECT_FLOAT_EQ(m.M33, 0.0f);
    EXPECT_FLOAT_EQ(m.M44, 1.0f);
    EXPECT_FALSE(std::isnan(m.M11)) << "PIN: silently singular rather than NaN; #2175 owns it";

    // up parallel to the forward axis: the cross product is zero, same outcome.
    const Matrix4x4 par = Matrix4x4::CreateLookAt({z, z, 5.0f * o}, {z, z, z}, {z, z, o});
    EXPECT_FLOAT_EQ(par.M11, 0.0f);
    EXPECT_FLOAT_EQ(par.M21, 0.0f);
}

TEST(PIN_NormalizeContractTests, QuaternionNormalizeIsTheContrastingControl) {
    // Already unconditional, and its doc-comment cites .NET's Quaternion.cs for it. This is the
    // strongest in-repository evidence for what #2175 would do to the other four -- and the reason
    // the finding is real: one member of the family follows .NET and four do not.
    const Quaternion q = Quaternion::Normalize({0.0f, 0.0f, 0.0f, 0.0f});
    EXPECT_TRUE(std::isnan(q.X));
    EXPECT_TRUE(std::isnan(q.Y));
    EXPECT_TRUE(std::isnan(q.Z));
    EXPECT_TRUE(std::isnan(q.W));
}

TEST(PIN_NormalizeContractTests, TheModuleHoldsThreeThresholdsForOneQuestion) {
    // Recorded as a single assertion so the inconsistency is visible in test output.
    const float o = g_one, z = g_zero;
    // 1. vectors: > 0 on the length.
    EXPECT_FLOAT_EQ(Vector3::Normalize({1e-11f * o, z, z}).X, 1.0f);
    // 2. plane: < 1e-10f on the length -- the same magnitude is NOT normalized.
    EXPECT_FLOAT_EQ(Plane::Normalize(Plane{1e-11f * o, z, z, o}).Normal.X, 1e-11f);
    // 3. Quaternion::Inverse: > 1.192092896e-7f on the SQUARE, the only one documented as
    //    matching a .NET threshold.
    const Quaternion tiny{1e-5f * o, z, z, z};
    ASSERT_LT(tiny.LengthSquared(), 1.192092896e-7f);
    const Quaternion inv = Quaternion::Inverse(tiny);
    EXPECT_FLOAT_EQ(inv.X, 0.0f);
    EXPECT_FLOAT_EQ(inv.W, 0.0f);
}
