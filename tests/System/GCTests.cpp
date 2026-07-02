// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/GC.hpp"

using System::GC;
using System::GCCollectionMode;
using System::GCNotificationStatus;
using System::GCKind;
using System::GCMemoryInfo;
using System::TimeSpan;

// ---------------------------------------------------------------------------
// GCCollectionMode enum
// ---------------------------------------------------------------------------

TEST(GCTests, GCCollectionMode_DefaultIsZero) {
    EXPECT_EQ(static_cast<int>(GCCollectionMode::Default), 0);
}

TEST(GCTests, GCCollectionMode_ForcedIsOne) {
    EXPECT_EQ(static_cast<int>(GCCollectionMode::Forced), 1);
}

TEST(GCTests, GCCollectionMode_OptimizedIsTwo) {
    EXPECT_EQ(static_cast<int>(GCCollectionMode::Optimized), 2);
}

TEST(GCTests, GCCollectionMode_AggressiveIsThree) {
    EXPECT_EQ(static_cast<int>(GCCollectionMode::Aggressive), 3);
}

// ---------------------------------------------------------------------------
// GCNotificationStatus enum
// ---------------------------------------------------------------------------

TEST(GCTests, GCNotificationStatus_SucceededIsZero) {
    EXPECT_EQ(static_cast<int>(GCNotificationStatus::Succeeded), 0);
}

TEST(GCTests, GCNotificationStatus_FailedIsOne) {
    EXPECT_EQ(static_cast<int>(GCNotificationStatus::Failed), 1);
}

TEST(GCTests, GCNotificationStatus_CanceledIsTwo) {
    EXPECT_EQ(static_cast<int>(GCNotificationStatus::Canceled), 2);
}

TEST(GCTests, GCNotificationStatus_TimeoutIsThree) {
    EXPECT_EQ(static_cast<int>(GCNotificationStatus::Timeout), 3);
}

TEST(GCTests, GCNotificationStatus_NotApplicableIsFour) {
    EXPECT_EQ(static_cast<int>(GCNotificationStatus::NotApplicable), 4);
}

// ---------------------------------------------------------------------------
// GCKind enum
// ---------------------------------------------------------------------------

TEST(GCTests, GCKind_AnyIsZero)          { EXPECT_EQ(static_cast<int>(GCKind::Any), 0); }
TEST(GCTests, GCKind_EphemeralIsOne)     { EXPECT_EQ(static_cast<int>(GCKind::Ephemeral), 1); }
TEST(GCTests, GCKind_FullBlockingIsTwo)  { EXPECT_EQ(static_cast<int>(GCKind::FullBlocking), 2); }
TEST(GCTests, GCKind_BackgroundIsThree) { EXPECT_EQ(static_cast<int>(GCKind::Background), 3); }

// ---------------------------------------------------------------------------
// Collect overloads — all no-op, must not throw
// ---------------------------------------------------------------------------

TEST(GCTests, Collect_NoArgs_DoesNotThrow_New) {
    EXPECT_NO_THROW(GC::Collect());
}

TEST(GCTests, Collect_WithGeneration_DoesNotThrow) {
    EXPECT_NO_THROW(GC::Collect(1));
}

TEST(GCTests, Collect_WithMode_DoesNotThrow) {
    EXPECT_NO_THROW(GC::Collect(2, GCCollectionMode::Forced));
}

TEST(GCTests, Collect_WithModeAndBlocking_DoesNotThrow) {
    EXPECT_NO_THROW(GC::Collect(2, GCCollectionMode::Optimized, true));
}

TEST(GCTests, Collect_WithModeBlockingCompacting_DoesNotThrow) {
    EXPECT_NO_THROW(GC::Collect(2, GCCollectionMode::Aggressive, true, false));
}

// ---------------------------------------------------------------------------
// Memory info
// ---------------------------------------------------------------------------

TEST(GCTests, GetTotalMemory_ReturnsZero_New) {
    EXPECT_EQ(GC::GetTotalMemory(false), 0LL);
}

TEST(GCTests, GetTotalMemory_ForceCollection_ReturnsZero) {
    EXPECT_EQ(GC::GetTotalMemory(true), 0LL);
}

TEST(GCTests, GetTotalAllocatedBytes_ReturnsZero) {
    EXPECT_EQ(GC::GetTotalAllocatedBytes(), 0LL);
}

TEST(GCTests, GetTotalAllocatedBytes_Precise_ReturnsZero) {
    EXPECT_EQ(GC::GetTotalAllocatedBytes(true), 0LL);
}

TEST(GCTests, GetAllocatedBytesForCurrentThread_ReturnsZero) {
    EXPECT_EQ(GC::GetAllocatedBytesForCurrentThread(), 0LL);
}

TEST(GCTests, GetTotalPauseDuration_ReturnsZero) {
    EXPECT_EQ(GC::GetTotalPauseDuration(), TimeSpan::Zero);
}

// ---------------------------------------------------------------------------
// GCMemoryInfo
// ---------------------------------------------------------------------------

TEST(GCTests, GetGCMemoryInfo_NoArgs_DoesNotThrow) {
    EXPECT_NO_THROW(GC::GetGCMemoryInfo());
}

TEST(GCTests, GetGCMemoryInfo_WithKind_DoesNotThrow) {
    EXPECT_NO_THROW(GC::GetGCMemoryInfo(GCKind::FullBlocking));
}

TEST(GCTests, GCMemoryInfo_TotalAvailableMemoryBytes_IsZero) {
    GCMemoryInfo info = GC::GetGCMemoryInfo();
    EXPECT_EQ(info.getTotalAvailableMemoryBytesProperty(), 0LL);
}

TEST(GCTests, GCMemoryInfo_HeapSizeBytes_IsZero) {
    GCMemoryInfo info = GC::GetGCMemoryInfo();
    EXPECT_EQ(info.getHeapSizeBytesProperty(), 0LL);
}

TEST(GCTests, GCMemoryInfo_PauseDuration_IsZero) {
    GCMemoryInfo info = GC::GetGCMemoryInfo();
    EXPECT_EQ(info.getPauseDurationProperty(), TimeSpan::Zero);
}

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

TEST(GCTests, MaxGeneration_ReturnsTwo_New) {
    EXPECT_EQ(GC::MaxGeneration(), 2);
}

TEST(GCTests, GetMaxGenerationProperty_ReturnsTwo) {
    EXPECT_EQ(GC::getMaxGenerationProperty(), 2);
}

TEST(GCTests, GetGeneration_ReturnsZero_New) {
    int x = 42;
    EXPECT_EQ(GC::GetGeneration(&x), 0);
}

TEST(GCTests, CollectionCount_ReturnsZero) {
    EXPECT_EQ(GC::CollectionCount(0), 0);
    EXPECT_EQ(GC::CollectionCount(2), 0);
}

// ---------------------------------------------------------------------------
// Finalizer control
// ---------------------------------------------------------------------------

TEST(GCTests, SuppressFinalize_DoesNotThrow) {
    int x = 0;
    EXPECT_NO_THROW(GC::SuppressFinalize(&x));
}

TEST(GCTests, ReRegisterForFinalize_DoesNotThrow) {
    int x = 0;
    EXPECT_NO_THROW(GC::ReRegisterForFinalize(&x));
}

TEST(GCTests, WaitForPendingFinalizers_DoesNotThrow_New) {
    EXPECT_NO_THROW(GC::WaitForPendingFinalizers());
}

TEST(GCTests, KeepAlive_DoesNotThrow_New) {
    int v = 99;
    EXPECT_NO_THROW(GC::KeepAlive(v));
}

// ---------------------------------------------------------------------------
// Memory pressure
// ---------------------------------------------------------------------------

TEST(GCTests, AddMemoryPressure_DoesNotThrow) {
    EXPECT_NO_THROW(GC::AddMemoryPressure(1024LL));
}

TEST(GCTests, RemoveMemoryPressure_DoesNotThrow) {
    EXPECT_NO_THROW(GC::RemoveMemoryPressure(1024LL));
}

TEST(GCTests, RefreshMemoryLimit_DoesNotThrow) {
    EXPECT_NO_THROW(GC::RefreshMemoryLimit());
}

// ---------------------------------------------------------------------------
// No-GC region
// ---------------------------------------------------------------------------

TEST(GCTests, TryStartNoGCRegion_ReturnsFalse) {
    EXPECT_FALSE(GC::TryStartNoGCRegion(1024LL * 1024));
}

TEST(GCTests, TryStartNoGCRegion_WithDisallowFullBlockingGC_ReturnsFalse) {
    EXPECT_FALSE(GC::TryStartNoGCRegion(1024LL * 1024, true));
}

TEST(GCTests, TryStartNoGCRegion_WithLohSize_ReturnsFalse) {
    EXPECT_FALSE(GC::TryStartNoGCRegion(1024LL * 1024, 512LL * 1024));
}

TEST(GCTests, TryStartNoGCRegion_WithLohSizeAndDisallowFullBlockingGC_ReturnsFalse) {
    EXPECT_FALSE(GC::TryStartNoGCRegion(1024LL * 1024, 512LL * 1024, true));
}

TEST(GCTests, EndNoGCRegion_DoesNotThrow) {
    EXPECT_NO_THROW(GC::EndNoGCRegion());
}

TEST(GCTests, RegisterNoGCRegionCallback_DoesNotThrow) {
    bool called = false;
    EXPECT_NO_THROW(GC::RegisterNoGCRegionCallback(4096LL, [&]{ called = true; }));
    EXPECT_FALSE(called);
}

// ---------------------------------------------------------------------------
// Full GC notifications
// ---------------------------------------------------------------------------

TEST(GCTests, RegisterForFullGCNotification_DoesNotThrow) {
    EXPECT_NO_THROW(GC::RegisterForFullGCNotification(90, 90));
}

TEST(GCTests, CancelFullGCNotification_DoesNotThrow) {
    EXPECT_NO_THROW(GC::CancelFullGCNotification());
}

TEST(GCTests, WaitForFullGCApproach_ReturnsSucceeded) {
    EXPECT_EQ(GC::WaitForFullGCApproach(0), GCNotificationStatus::Succeeded);
}

TEST(GCTests, WaitForFullGCApproach_DefaultTimeout_ReturnsSucceeded) {
    EXPECT_EQ(GC::WaitForFullGCApproach(), GCNotificationStatus::Succeeded);
}

TEST(GCTests, WaitForFullGCApproach_TimeSpanTimeout_ReturnsSucceeded) {
    EXPECT_EQ(GC::WaitForFullGCApproach(TimeSpan::FromSeconds(1)), GCNotificationStatus::Succeeded);
}

TEST(GCTests, WaitForFullGCComplete_ReturnsSucceeded) {
    EXPECT_EQ(GC::WaitForFullGCComplete(0), GCNotificationStatus::Succeeded);
}

TEST(GCTests, WaitForFullGCComplete_DefaultTimeout_ReturnsSucceeded) {
    EXPECT_EQ(GC::WaitForFullGCComplete(), GCNotificationStatus::Succeeded);
}

TEST(GCTests, WaitForFullGCComplete_TimeSpanTimeout_ReturnsSucceeded) {
    EXPECT_EQ(GC::WaitForFullGCComplete(TimeSpan::FromSeconds(1)), GCNotificationStatus::Succeeded);
}

// ---------------------------------------------------------------------------
// Legacy helper
// ---------------------------------------------------------------------------

TEST(GCTests, GetGCMemoryInfo_TotalAvailableMemoryBytes_IsZero) {
    EXPECT_EQ(GC::GetGCMemoryInfo_TotalAvailableMemoryBytes(), 0LL);
}
