// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2256, the compatible half of the #2255 review of finding SR-AUD-102.
//
// SR-AUD-102 says AppContext's named data cannot configure BaseDirectory or compatibility
// switches, because the store holds untyped `void*` and nothing can ask whether an entry is a
// string. Both premises reproduce exactly as filed, and BOTH repairs are approval-bound:
// implementing either needs the store to carry a runtime type, which changes four public member
// signatures across AppContext and AppDomain (the latter has forwarded here since #2249), and the
// BaseDirectory half additionally needs getBaseDirectoryProperty() to return by value, because a
// reference into caller-owned store storage would have no liveness boundary. That decision is
// ticket #2255; docs/CoreAppContextNamedDataDesign.md prices the three routes.
//
// This fixture therefore changes NO behaviour. It pins the contract that exists today -- the five
// gaps the audit report lists under "other missing assertions" -- so that a future approved repair
// has a baseline to move FROM, and so that the two divergences are observable rather than merely
// asserted in a doc-comment. In particular the two DIVERGENCE tests below are expected to be
// REWRITTEN, not merely to keep passing, if #2255 is approved.
//
// It deliberately does not add string-switch parsing or a BaseDirectory override, and it cannot
// reach #2250: nothing here changes what AppContext::TryGetSwitch returns, so nothing changes what
// a future approved AppDomain::IsCompatibilitySwitchSet forwarding would observe.

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "System/AppContext.hpp"
#include "System/AppDomain.hpp"

using System::AppContext;

// ---------------------------------------------------------------------------
// The two divergences, pinned rather than described.
// ---------------------------------------------------------------------------

TEST(AppContextNamedDataTests, Divergence_BaseDirectoryIgnoresTheAppContextBaseDirectoryKey) {
    // .NET resolves BaseDirectory from this key first. This port computes it unconditionally.
    // Storing a std::string under the key must change nothing -- and must not be read back
    // through the void*, which would be undefined behaviour for any other stored type.
    const std::string before = AppContext::getBaseDirectoryProperty();
    std::string override = "/sharp-runtime-2256-override/";
    AppContext::SetData("APP_CONTEXT_BASE_DIRECTORY", &override);
    EXPECT_EQ(AppContext::getBaseDirectoryProperty(), before);
    // The entry itself round-trips like any other -- it is simply never consulted.
    EXPECT_EQ(AppContext::GetData("APP_CONTEXT_BASE_DIRECTORY"), &override);
    AppContext::SetData("APP_CONTEXT_BASE_DIRECTORY", nullptr);
}

TEST(AppContextNamedDataTests, Divergence_TryGetSwitchIgnoresAStringValuedDataEntry) {
    // .NET falls back to the data store and parses a string value there as the switch's boolean.
    // This port keeps the two maps independent.
    std::string enabledText = "true";
    AppContext::SetData("sharp-runtime.2256.stringSwitch", &enabledText);
    bool isEnabled = true;
    EXPECT_FALSE(AppContext::TryGetSwitch("sharp-runtime.2256.stringSwitch", isEnabled));
    EXPECT_FALSE(isEnabled);  // set to false on failure, per the documented contract
    AppContext::SetData("sharp-runtime.2256.stringSwitch", nullptr);
}

TEST(AppContextNamedDataTests, TheDataAndSwitchMapsAreIndependent) {
    int payload = 7;
    AppContext::SetData("sharp-runtime.2256.independent", &payload);
    AppContext::SetSwitch("sharp-runtime.2256.independent", true);

    // Same name, two maps, two unrelated answers.
    EXPECT_EQ(AppContext::GetData("sharp-runtime.2256.independent"), &payload);
    bool isEnabled = false;
    EXPECT_TRUE(AppContext::TryGetSwitch("sharp-runtime.2256.independent", isEnabled));
    EXPECT_TRUE(isEnabled);

    // And setting one does not disturb the other.
    AppContext::SetSwitch("sharp-runtime.2256.independent", false);
    EXPECT_EQ(AppContext::GetData("sharp-runtime.2256.independent"), &payload);
}

// ---------------------------------------------------------------------------
// The data store's ownership and replacement contract.
// ---------------------------------------------------------------------------

TEST(AppContextNamedDataTests, StoredNullIsIndistinguishableFromAnAbsentKey) {
    // Documented consequence of the void* adaptation, and the reason there is no removal door:
    // a caller cannot tell "stored nullptr" from "never stored".
    EXPECT_EQ(AppContext::GetData("sharp-runtime.2256.neverStored"), nullptr);
    AppContext::SetData("sharp-runtime.2256.storedNull", nullptr);
    EXPECT_EQ(AppContext::GetData("sharp-runtime.2256.storedNull"), nullptr);
}

TEST(AppContextNamedDataTests, SetDataReplacesAnExistingKey) {
    int first = 1;
    int second = 2;
    AppContext::SetData("sharp-runtime.2256.replace", &first);
    ASSERT_EQ(AppContext::GetData("sharp-runtime.2256.replace"), &first);
    AppContext::SetData("sharp-runtime.2256.replace", &second);
    EXPECT_EQ(AppContext::GetData("sharp-runtime.2256.replace"), &second);
}

TEST(AppContextNamedDataTests, SetDataStoresABorrowedPointerAndCopiesNothing) {
    // The stored pointer must be the caller's own address, not a copy of the pointee: a caller
    // who mutates the target afterwards must see the new value through GetData. This is what
    // "borrowed" means, and it is the property that makes the lifetime warning necessary.
    int payload = 10;
    AppContext::SetData("sharp-runtime.2256.borrowed", &payload);
    payload = 20;
    void* stored = AppContext::GetData("sharp-runtime.2256.borrowed");
    ASSERT_EQ(stored, &payload);
    EXPECT_EQ(*static_cast<int*>(stored), 20);
}

TEST(AppContextNamedDataTests, AppDomainSeesTheSameStore) {
    // #2249 made AppDomain::SetData/GetData forwarders. This pins the direction #2255's route A
    // would have to change on BOTH classes at once.
    int payload = 42;
    AppContext::SetData("sharp-runtime.2256.shared", &payload);
    EXPECT_EQ(System::AppDomain::CurrentDomain().GetData("sharp-runtime.2256.shared"), &payload);
}

// ---------------------------------------------------------------------------
// Reference lifetime and the reflection deviation.
// ---------------------------------------------------------------------------

TEST(AppContextNamedDataTests, BaseDirectoryReferenceOutlivesItsCallAndIsStable) {
    // Today the reference binds to process-lifetime storage owned by AppDomain, so holding it is
    // safe. That is exactly what an APP_CONTEXT_BASE_DIRECTORY override would put at risk, and
    // why #2255's option (a) also asks about the return type.
    const std::string& held = AppContext::getBaseDirectoryProperty();
    const std::string copy = held;
    for (int i = 0; i < 100; ++i) {
        (void)AppContext::getBaseDirectoryProperty();
    }
    EXPECT_EQ(held, copy);
    EXPECT_EQ(&held, &AppContext::getBaseDirectoryProperty());
    EXPECT_FALSE(held.empty());
}

TEST(AppContextNamedDataTests, TargetFrameworkNameIsTheDeclaredReflectionDeviation) {
    // Empty, always, by decision -- not an unavailable-entry-assembly accident. Pinned so that a
    // future change has to say so out loud.
    EXPECT_TRUE(AppContext::getTargetFrameworkNameProperty().empty());
    EXPECT_EQ(AppContext::getTargetFrameworkNameProperty(),
              AppContext::getTargetFrameworkNameProperty());
}

// ---------------------------------------------------------------------------
// Concurrency: both maps are guarded by the same process-wide mutex.
// ---------------------------------------------------------------------------

TEST(AppContextNamedDataTests, ConcurrentDataAndSwitchUseIsSerialised) {
    constexpr int kThreads = 4;
    constexpr int kIterations = 250;
    std::vector<int> payloads(kThreads, 0);
    std::atomic<int> mismatches{0};

    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([t, &payloads, &mismatches] {
            const std::string dataKey = "sharp-runtime.2256.concurrent.data." + std::to_string(t);
            const std::string switchKey =
                "sharp-runtime.2256.concurrent.switch." + std::to_string(t);
            for (int i = 0; i < kIterations; ++i) {
                AppContext::SetData(dataKey, &payloads[static_cast<std::size_t>(t)]);
                AppContext::SetSwitch(switchKey, (i % 2) == 0);
                if (AppContext::GetData(dataKey) != &payloads[static_cast<std::size_t>(t)]) {
                    ++mismatches;
                }
                bool isEnabled = false;
                if (!AppContext::TryGetSwitch(switchKey, isEnabled)) {
                    ++mismatches;
                }
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    EXPECT_EQ(mismatches.load(), 0);
    for (int t = 0; t < kThreads; ++t) {
        EXPECT_EQ(AppContext::GetData("sharp-runtime.2256.concurrent.data." + std::to_string(t)),
                  &payloads[static_cast<std::size_t>(t)]);
    }
}
