// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2027 (SR-AUD-275, cause D-G of docs/SystemDiagnosticsNamespaceReviewPlan.md).
//
// Debug::providerStorage() was a function-local static std::shared_ptr<DebugProvider> written
// by SetProvider and read by every Write, with no lock and no atomic; Debug's and Trace's
// indent SIZE were plain process-global scalars (indent LEVEL was already correctly
// thread_local in both). Measured with TSan against the header itself -- Debug and Trace are
// header-only, so the production bodies are necessarily instrumented -- the before-state
// reported 20 data races AND 12 heap-use-after-free, the latter because a reader could be
// dereferencing a provider that a concurrent SetProvider had already destroyed
// (build-probe/2027_probe1_tsan_before.log; after: build-probe/2027_probe1_tsan_after.log, 0).
//
// A unit test cannot assert "no data race" -- that is what the TSan probe is for. What these
// tests pin is the OBSERVABLE contract the repair rests on: reads take an owning snapshot, so
// a provider in use cannot be destroyed under the caller; the callback reports the value the
// call actually set; and none of the single-threaded semantics moved.

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "System/ArgumentNullException.hpp"
#include "System/Diagnostics/Debug.hpp"
#include "System/Diagnostics/DebugProvider.hpp"
#include "System/Diagnostics/Trace.hpp"

using System::Diagnostics::Debug;
using System::Diagnostics::DebugProvider;
using System::Diagnostics::Trace;

namespace {

class RecordingProvider : public DebugProvider {
public:
    std::atomic<int> writes{0};
    std::atomic<SharpRuntime::intcs> lastIndentSize{-1};
    std::atomic<SharpRuntime::intcs> lastIndentLevel{-1};

    void Write(const std::string&) override { writes.fetch_add(1, std::memory_order_relaxed); }
    void WriteLine(const std::string&) override { writes.fetch_add(1, std::memory_order_relaxed); }
    void OnIndentSizeChanged(SharpRuntime::intcs size) override {
        lastIndentSize.store(size, std::memory_order_relaxed);
    }
    void OnIndentLevelChanged(SharpRuntime::intcs level) override {
        lastIndentLevel.store(level, std::memory_order_relaxed);
    }
};

// Restores whatever provider and indent sizes were installed, so these tests cannot leak
// state into the rest of the executable.
class DiagnosticsStateGuard {
public:
    DiagnosticsStateGuard()
        : provider_(Debug::GetProvider()),
          debugIndentSize_(Debug::getIndentSizeProperty()),
          traceIndentSize_(Trace::getIndentSizeProperty()) {}
    ~DiagnosticsStateGuard() {
        Debug::SetProvider(provider_);
        Debug::setIndentSizeProperty(debugIndentSize_);
        Trace::setIndentSizeProperty(traceIndentSize_);
    }
    DiagnosticsStateGuard(const DiagnosticsStateGuard&) = delete;
    DiagnosticsStateGuard& operator=(const DiagnosticsStateGuard&) = delete;

private:
    std::shared_ptr<DebugProvider> provider_;
    SharpRuntime::intcs debugIndentSize_;
    int traceIndentSize_;
};

} // namespace

// ---------------------------------------------------------------------------
// The ownership guarantee the repair rests on.
// ---------------------------------------------------------------------------

// GetProvider must hand back an OWNING reference, not a view of the global. This is precisely
// what turned the heap-use-after-free into a safe read.
TEST(DebugTraceConcurrencyTests, GetProviderKeepsTheProviderAliveAcrossAReplacement) {
    DiagnosticsStateGuard guard;

    auto installed = std::make_shared<RecordingProvider>();
    Debug::SetProvider(installed);

    std::weak_ptr<DebugProvider> observer = installed;
    std::shared_ptr<DebugProvider> snapshot = Debug::GetProvider();
    installed.reset();

    // Replacing the global drops the storage's reference; the snapshot must still own one.
    Debug::SetProvider(std::make_shared<RecordingProvider>());

    EXPECT_FALSE(observer.expired()) << "the provider was destroyed while a caller held it";
    ASSERT_NE(snapshot, nullptr);
    EXPECT_NO_THROW(snapshot->Write("still alive"));
}

// SetProvider returns the outgoing provider by value, so it survives the swap even when the
// storage held the only other reference.
TEST(DebugTraceConcurrencyTests, SetProviderReturnsAnOwningReferenceToThePreviousProvider) {
    DiagnosticsStateGuard guard;

    Debug::SetProvider(std::make_shared<RecordingProvider>());
    std::shared_ptr<DebugProvider> previous =
        Debug::SetProvider(std::make_shared<RecordingProvider>());

    ASSERT_NE(previous, nullptr);
    EXPECT_NO_THROW(previous->Write("previous is still usable"));
}

// ---------------------------------------------------------------------------
// Concurrency: the shapes TSan proved race-free must also behave sanely.
// ---------------------------------------------------------------------------

TEST(DebugTraceConcurrencyTests, ConcurrentSetProviderAndWriteCompleteWithoutCorruption) {
    DiagnosticsStateGuard guard;
    Debug::SetProvider(std::make_shared<RecordingProvider>());

    std::atomic<bool> stop{false};
    std::thread writer([&] {
        for (int i = 0; i < 2000; ++i) Debug::SetProvider(std::make_shared<RecordingProvider>());
        stop.store(true, std::memory_order_relaxed);
    });
    std::thread reader([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            Debug::Write("x");
            Debug::WriteLine("y");
            auto snapshot = Debug::GetProvider();
            EXPECT_NE(snapshot, nullptr);
        }
    });

    writer.join();
    reader.join();

    EXPECT_NE(Debug::GetProvider(), nullptr);
}

TEST(DebugTraceConcurrencyTests, ConcurrentDebugIndentSizeWritesLeaveAValueThatWasWritten) {
    DiagnosticsStateGuard guard;
    Debug::SetProvider(std::make_shared<RecordingProvider>());

    std::vector<std::thread> threads;
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([] {
            for (int i = 0; i < 5000; ++i) {
                Debug::setIndentSizeProperty((i % 8) + 1);
                const SharpRuntime::intcs observed = Debug::getIndentSizeProperty();
                EXPECT_GE(observed, 1);
                EXPECT_LE(observed, 8);
            }
        });
    }
    for (auto& thread : threads) thread.join();

    EXPECT_GE(Debug::getIndentSizeProperty(), 1);
    EXPECT_LE(Debug::getIndentSizeProperty(), 8);
}

TEST(DebugTraceConcurrencyTests, ConcurrentTraceIndentSizeWritesLeaveAValueThatWasWritten) {
    DiagnosticsStateGuard guard;

    std::vector<std::thread> threads;
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([] {
            for (int i = 0; i < 5000; ++i) {
                Trace::setIndentSizeProperty((i % 8) + 1);
                const int observed = Trace::getIndentSizeProperty();
                EXPECT_GE(observed, 1);
                EXPECT_LE(observed, 8);
            }
        });
    }
    for (auto& thread : threads) thread.join();

    EXPECT_GE(Trace::getIndentSizeProperty(), 1);
    EXPECT_LE(Trace::getIndentSizeProperty(), 8);
}

// The indent LEVEL is per-thread in both classes and must stay that way: one thread's level
// must be invisible to another.
TEST(DebugTraceConcurrencyTests, IndentLevelRemainsPerThread) {
    DiagnosticsStateGuard guard;
    Debug::SetProvider(std::make_shared<RecordingProvider>());

    Debug::setIndentLevelProperty(7);
    Trace::setIndentLevelProperty(9);

    SharpRuntime::intcs debugLevelInOtherThread = -1;
    int traceLevelInOtherThread = -1;
    std::thread other([&] {
        debugLevelInOtherThread = Debug::getIndentLevelProperty();
        traceLevelInOtherThread = Trace::getIndentLevelProperty();
    });
    other.join();

    EXPECT_EQ(debugLevelInOtherThread, 0);
    EXPECT_EQ(traceLevelInOtherThread, 0);
    EXPECT_EQ(Debug::getIndentLevelProperty(), 7);
    EXPECT_EQ(Trace::getIndentLevelProperty(), 9);

    Debug::setIndentLevelProperty(0);
    Trace::setIndentLevelProperty(0);
}

// ---------------------------------------------------------------------------
// Single-threaded semantics must be unchanged.
// ---------------------------------------------------------------------------

TEST(DebugTraceConcurrencyTests, SetProviderNullStillThrows) {
    EXPECT_THROW(Debug::SetProvider(nullptr), System::ArgumentNullException);
}

TEST(DebugTraceConcurrencyTests, IndentSizeCallbackReportsTheClampedValueThatWasSet) {
    DiagnosticsStateGuard guard;

    auto provider = std::make_shared<RecordingProvider>();
    Debug::SetProvider(provider);

    Debug::setIndentSizeProperty(6);
    EXPECT_EQ(Debug::getIndentSizeProperty(), 6);
    EXPECT_EQ(provider->lastIndentSize.load(), 6);

    // Negative values clamp to 0, and the callback is told 0 rather than the raw argument.
    Debug::setIndentSizeProperty(-3);
    EXPECT_EQ(Debug::getIndentSizeProperty(), 0);
    EXPECT_EQ(provider->lastIndentSize.load(), 0);
}

TEST(DebugTraceConcurrencyTests, TraceIndentSizeClampingIsUnchanged) {
    DiagnosticsStateGuard guard;

    Trace::setIndentSizeProperty(6);
    EXPECT_EQ(Trace::getIndentSizeProperty(), 6);
    Trace::setIndentSizeProperty(-3);
    EXPECT_EQ(Trace::getIndentSizeProperty(), 0);
}

TEST(DebugTraceConcurrencyTests, IndentLevelCallbackStillFires) {
    DiagnosticsStateGuard guard;

    auto provider = std::make_shared<RecordingProvider>();
    Debug::SetProvider(provider);

    Debug::setIndentLevelProperty(3);
    EXPECT_EQ(provider->lastIndentLevel.load(), 3);
    Debug::setIndentLevelProperty(-1);
    EXPECT_EQ(provider->lastIndentLevel.load(), 0);
    EXPECT_EQ(Debug::getIndentLevelProperty(), 0);
}

TEST(DebugTraceConcurrencyTests, WriteStillRoutesThroughTheInstalledProvider) {
    DiagnosticsStateGuard guard;

    auto provider = std::make_shared<RecordingProvider>();
    Debug::SetProvider(provider);

    Debug::Write("a");
    Debug::WriteLine("b");

#ifdef NDEBUG
    // Write/WriteLine are compiled out in release builds, so nothing may be recorded.
    EXPECT_EQ(provider->writes.load(), 0);
#else
    EXPECT_EQ(provider->writes.load(), 2);
#endif
}
