// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <stdexcept>
#include <thread>
#include <utility>
#include "System/ArgumentNullException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Runtime/ExceptionServices/ExceptionDispatchInfo.hpp"

using System::Runtime::ExceptionServices::ExceptionDispatchInfo;

namespace {
    /** Captures a live exception_ptr so each test starts from a valid instance. */
    std::exception_ptr makeCaptured(const char* text) {
        try {
            throw std::runtime_error(text);
        } catch (...) {
            return std::current_exception();
        }
    }
} // namespace

TEST(ExceptionDispatchInfoTests, CaptureAndThrow_PreservesException) {
    std::exception_ptr captured;
    try {
        throw std::runtime_error("original failure");
    } catch (...) {
        captured = std::current_exception();
    }

    auto info = ExceptionDispatchInfo::Capture(captured);
    EXPECT_THROW(info.Throw(), std::runtime_error);

    try {
        info.Throw();
        FAIL() << "expected throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "original failure");
    }
}

TEST(ExceptionDispatchInfoTests, GetSourceException_ReturnsCaptured) {
    std::exception_ptr captured;
    try {
        throw std::logic_error("boom");
    } catch (...) {
        captured = std::current_exception();
    }
    auto info = ExceptionDispatchInfo::Capture(captured);
    EXPECT_EQ(info.getSourceExceptionProperty(), captured);
}

TEST(ExceptionDispatchInfoTests, StaticThrow_RethrowsImmediately) {
    std::exception_ptr captured;
    try {
        throw std::runtime_error("static throw");
    } catch (...) {
        captured = std::current_exception();
    }
    EXPECT_THROW(ExceptionDispatchInfo::Throw(captured), std::runtime_error);
}

TEST(ExceptionDispatchInfoTests, CrossThreadRethrow) {
    std::exception_ptr captured;
    std::thread worker([&]() {
        try {
            throw std::runtime_error("from worker thread");
        } catch (...) {
            captured = std::current_exception();
        }
    });
    worker.join();

    auto info = ExceptionDispatchInfo::Capture(captured);
    EXPECT_THROW(info.Throw(), std::runtime_error);
}

// ---------------------------------------------------------------------------------------
// Ticket #1973 / SR-AUD-155 (cause R-E of docs/SystemRuntimeNamespaceReviewPlan.md).
//
// Every case below reproduced as SIGSEGV before the repair -- see
// build-probe/1972_probe1_before.log. The two "moved-from" cases are NOT named by
// SR-AUD-155; they are §4.1 of the review plan, and they are the reason the instance
// Throw() carries its own guard rather than relying on Capture()'s.
// ---------------------------------------------------------------------------------------

TEST(ExceptionDispatchInfoTests, Capture_NullSource_ThrowsArgumentNullException) {
    std::exception_ptr none;
    EXPECT_THROW(ExceptionDispatchInfo::Capture(none), System::ArgumentNullException);
}

TEST(ExceptionDispatchInfoTests, Capture_NullSource_NamesTheSourceParameter) {
    std::exception_ptr none;
    try {
        (void)ExceptionDispatchInfo::Capture(none);
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_NE(std::string(e.what()).find("source"), std::string::npos) << e.what();
    }
}

TEST(ExceptionDispatchInfoTests, StaticThrow_NullSource_ThrowsArgumentNullException) {
    std::exception_ptr none;
    EXPECT_THROW(ExceptionDispatchInfo::Throw(none), System::ArgumentNullException);
}

TEST(ExceptionDispatchInfoTests, NullSource_FailureIsCatchableAsSystemException) {
    // The point of the repair: the failure now crosses the boundary as an ordinary
    // System::Exception instead of ending the process where no handler can see it.
    std::exception_ptr none;
    bool caught = false;
    try {
        (void)ExceptionDispatchInfo::Capture(none);
    } catch (const System::Exception&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
}

TEST(ExceptionDispatchInfoTests, MovedFromInstance_Throw_ThrowsInvalidOperationException) {
    auto source = ExceptionDispatchInfo::Capture(makeCaptured("original"));
    auto sink = std::move(source);

    EXPECT_EQ(source.getSourceExceptionProperty(), nullptr);
    EXPECT_THROW(source.Throw(), System::InvalidOperationException);
}

TEST(ExceptionDispatchInfoTests, MoveAssignedFromInstance_Throw_ThrowsInvalidOperationException) {
    auto source = ExceptionDispatchInfo::Capture(makeCaptured("a"));
    auto sink = ExceptionDispatchInfo::Capture(makeCaptured("b"));
    sink = std::move(source);

    EXPECT_EQ(source.getSourceExceptionProperty(), nullptr);
    EXPECT_THROW(source.Throw(), System::InvalidOperationException);
}

TEST(ExceptionDispatchInfoTests, MoveTarget_StillRethrowsTheOriginalException) {
    // The guard must not cost the moved-TO object anything: emptying the source is only a
    // defect for the source.
    auto source = ExceptionDispatchInfo::Capture(makeCaptured("original"));
    auto sink = std::move(source);

    ASSERT_NE(sink.getSourceExceptionProperty(), nullptr);
    try {
        sink.Throw();
        FAIL() << "expected throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "original");
    }
}

TEST(ExceptionDispatchInfoTests, MoveAssignTarget_StillRethrowsTheMovedException) {
    auto source = ExceptionDispatchInfo::Capture(makeCaptured("a"));
    auto sink = ExceptionDispatchInfo::Capture(makeCaptured("b"));
    sink = std::move(source);

    try {
        sink.Throw();
        FAIL() << "expected throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "a");
    }
}

TEST(ExceptionDispatchInfoTests, CopiedInstance_BothCopiesStillRethrow) {
    // ExceptionDispatchInfo is copyable; a copy must not empty the original, so this is the
    // control case that distinguishes "moved from" from "used twice".
    auto original = ExceptionDispatchInfo::Capture(makeCaptured("shared"));
    auto copy = original;  // NOLINT(performance-unnecessary-copy-initialization) -- deliberate

    EXPECT_NE(original.getSourceExceptionProperty(), nullptr);
    EXPECT_THROW(original.Throw(), std::runtime_error);
    EXPECT_THROW(copy.Throw(), std::runtime_error);
}

TEST(ExceptionDispatchInfoTests, LiveInstance_ThrowIsRepeatable) {
    // Throwing must not consume the capture; the guard is a check, not a transfer.
    auto info = ExceptionDispatchInfo::Capture(makeCaptured("repeatable"));
    EXPECT_THROW(info.Throw(), std::runtime_error);
    EXPECT_THROW(info.Throw(), std::runtime_error);
    EXPECT_NE(info.getSourceExceptionProperty(), nullptr);
}
