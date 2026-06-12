// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
#include <gtest/gtest.h>
#include <cmath>
#include "System/Numerics/Vector2.hpp"
#include "System/Numerics/Vector3.hpp"
#include "System/Numerics/Vector4.hpp"
#include "System/Numerics/Matrix3x2.hpp"
#include "System/Numerics/Matrix4x4.hpp"
#include "System/Numerics/Quaternion.hpp"
#include "System/Numerics/Plane.hpp"

using namespace System::Numerics;

static bool near(float a, float b, float eps = 1e-5f) { return std::abs(a - b) < eps; }

// --- Vector2 ---
TEST(Vector2Tests, BasicArithmetic) {
    Vector2 a{1,2}, b{3,4};
    EXPECT_EQ((a+b), Vector2(4,6));
    EXPECT_EQ((a-b), Vector2(-2,-2));
    EXPECT_EQ((a*b), Vector2(3,8));
    EXPECT_EQ((a*2.0f), Vector2(2,4));
    EXPECT_EQ((a/2.0f), Vector2(0.5f,1.0f));
}
TEST(Vector2Tests, Dot) { EXPECT_FLOAT_EQ(Vector2::Dot({1,0},{0,1}), 0.0f); }
TEST(Vector2Tests, Length) { EXPECT_TRUE(near(Vector2(3,4).Length(), 5.0f)); }
TEST(Vector2Tests, Normalize) {
    auto n = Vector2::Normalize({3,4});
    EXPECT_TRUE(near(n.Length(), 1.0f));
}
TEST(Vector2Tests, Distance) { EXPECT_TRUE(near(Vector2::Distance({0,0},{3,4}), 5.0f)); }
TEST(Vector2Tests, Lerp) { EXPECT_EQ(Vector2::Lerp({0,0},{10,10},0.5f), Vector2(5,5)); }
TEST(Vector2Tests, Zero)  { EXPECT_EQ(Vector2::Zero(),  Vector2(0,0)); }
TEST(Vector2Tests, One)   { EXPECT_EQ(Vector2::One(),   Vector2(1,1)); }
TEST(Vector2Tests, UnitX) { EXPECT_EQ(Vector2::UnitX(), Vector2(1,0)); }

// --- Vector3 ---
TEST(Vector3Tests, Cross) {
    auto c = Vector3::Cross({1,0,0},{0,1,0});
    EXPECT_TRUE(near(c.X,0)); EXPECT_TRUE(near(c.Y,0)); EXPECT_TRUE(near(c.Z,1));
}
TEST(Vector3Tests, Normalize) {
    auto n = Vector3::Normalize({1,2,3});
    EXPECT_TRUE(near(n.Length(), 1.0f));
}
TEST(Vector3Tests, Lerp) { EXPECT_EQ(Vector3::Lerp({0,0,0},{2,4,6},0.5f), Vector3(1,2,3)); }

// --- Vector4 ---
TEST(Vector4Tests, BasicArithmetic) {
    Vector4 a{1,2,3,4}, b{5,6,7,8};
    EXPECT_EQ((a+b), Vector4(6,8,10,12));
}
TEST(Vector4Tests, Length) { EXPECT_TRUE(near(Vector4(1,1,1,1).Length(), 2.0f)); }

// --- Matrix3x2 ---
TEST(Matrix3x2Tests, Identity) {
    EXPECT_TRUE(Matrix3x2::Identity().getIsIdentityProperty());
}
TEST(Matrix3x2Tests, Multiply) {
    auto t = Matrix3x2::CreateTranslation(3,4);
    auto p = Vector2::Transform({1,1}, t);
    EXPECT_TRUE(near(p.X, 4.0f));
    EXPECT_TRUE(near(p.Y, 5.0f));
}
TEST(Matrix3x2Tests, Invert) {
    auto m = Matrix3x2::CreateTranslation(5,3);
    Matrix3x2 inv;
    EXPECT_TRUE(Matrix3x2::Invert(m, inv));
    auto combined = m * inv;
    EXPECT_TRUE(combined.getIsIdentityProperty());
}
TEST(Matrix3x2Tests, CreateRotation) {
    auto m = Matrix3x2::CreateRotation(0.0f);
    EXPECT_TRUE(m.getIsIdentityProperty());
}

// --- Matrix4x4 ---
TEST(Matrix4x4Tests, Identity) {
    EXPECT_TRUE(Matrix4x4::Identity().getIsIdentityProperty());
}
TEST(Matrix4x4Tests, Multiply) {
    auto a = Matrix4x4::Identity();
    auto b = Matrix4x4::CreateTranslation(1,2,3);
    auto c = a * b;
    EXPECT_TRUE(near(c.M41, 1.0f));
    EXPECT_TRUE(near(c.M42, 2.0f));
    EXPECT_TRUE(near(c.M43, 3.0f));
}
TEST(Matrix4x4Tests, Invert) {
    auto m = Matrix4x4::CreateTranslation(5,6,7);
    Matrix4x4 inv;
    EXPECT_TRUE(Matrix4x4::Invert(m, inv));
    auto r = m * inv;
    EXPECT_TRUE(r.getIsIdentityProperty());
}
TEST(Matrix4x4Tests, TransformVector3) {
    auto m = Matrix4x4::CreateTranslation(10,0,0);
    auto v = Vector3::Transform({0,0,0}, m);
    EXPECT_TRUE(near(v.X, 10.0f));
}
TEST(Matrix4x4Tests, CreateRotationX) {
    auto m = Matrix4x4::CreateRotationX(0.0f);
    EXPECT_TRUE(m.getIsIdentityProperty());
}
TEST(Matrix4x4Tests, Transpose) {
    auto m = Matrix4x4{1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16};
    auto t = Matrix4x4::Transpose(m);
    EXPECT_FLOAT_EQ(t.M12, 5.0f);
    EXPECT_FLOAT_EQ(t.M21, 2.0f);
}

// --- Quaternion ---
TEST(QuaternionTests, Identity) {
    EXPECT_TRUE(Quaternion::Identity().getIsIdentityProperty());
}
TEST(QuaternionTests, Normalize) {
    auto q = Quaternion::Normalize({1,2,3,4});
    EXPECT_TRUE(near(q.Length(), 1.0f));
}
TEST(QuaternionTests, Conjugate) {
    auto q = Quaternion{1,2,3,4};
    auto c = Quaternion::Conjugate(q);
    EXPECT_FLOAT_EQ(c.X, -1.0f);
    EXPECT_FLOAT_EQ(c.W,  4.0f);
}
TEST(QuaternionTests, CreateFromAxisAngle) {
    auto q = Quaternion::CreateFromAxisAngle({0,0,1}, 0.0f);
    EXPECT_TRUE(near(q.W, 1.0f));
}
TEST(QuaternionTests, RoundtripMatrix) {
    auto q = Quaternion::CreateFromYawPitchRoll(0.3f, 0.2f, 0.1f);
    auto m = Matrix4x4::CreateFromQuaternion(q);
    auto q2 = Quaternion::CreateFromRotationMatrix(m);
    EXPECT_TRUE(near(std::abs(Quaternion::Dot(q,q2)), 1.0f, 1e-4f));
}

// --- Plane ---
TEST(PlaneTests, DotCoordinate) {
    Plane p{{0,1,0}, -1.0f};
    EXPECT_FLOAT_EQ(Plane::DotCoordinate(p, {0,1,0}), 0.0f);
}
TEST(PlaneTests, Normalize) {
    Plane p{2,0,0,-4};
    auto n = Plane::Normalize(p);
    EXPECT_TRUE(near(n.Normal.Length(), 1.0f));
    EXPECT_TRUE(near(n.D, -2.0f));
}
TEST(PlaneTests, CreateFromVertices) {
    auto p = Plane::CreateFromVertices({0,0,0},{1,0,0},{0,1,0});
    EXPECT_TRUE(near(std::abs(p.Normal.Z), 1.0f));
}

// --- Colors ---
#include "System/Numerics/Colors/Colors.hpp"
using namespace System::Numerics::Colors;
TEST(ColorsTests, ArgbRoundtrip) {
    Argb<uint8_t> a{255,10,20,30};
    auto r = a.ToRgba();
    EXPECT_EQ(r.R, 10); EXPECT_EQ(r.G, 20); EXPECT_EQ(r.B, 30); EXPECT_EQ(r.A, 255);
    auto back = r.ToArgb();
    EXPECT_TRUE(back.Equals(a));
}
