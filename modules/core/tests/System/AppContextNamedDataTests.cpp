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
#include <type_traits>
#include <any>
#include <typeinfo>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "System/AppContext.hpp"
#include "System/AppDomain.hpp"

using System::AppContext;

// ---------------------------------------------------------------------------
// #2255 — the two divergences are GONE, and both pins below are inverted.
//
// Both needed the store to carry a runtime TYPE, which a std::unordered_map<std::string, void*>
// cannot. Reading a void* back AS a std::string for two special keys was the alternative the
// review priced and rejected: undefined behaviour by construction, and unfalsifiable at the point
// of use. std::any makes the question answerable, so both behaviours are real rather than
// documented away.
// ---------------------------------------------------------------------------

TEST(AppContextNamedDataTests, Fix2255_BaseDirectoryHonoursTheAppContextBaseDirectoryKey) {
    // .NET resolves BaseDirectory from this key first (`AppContext.cs:28-32`):
    //     GetData("APP_CONTEXT_BASE_DIRECTORY") as string ?? GetBaseDirectoryCore()
    const std::string before = AppContext::getBaseDirectoryProperty();
    AppContext::SetData("APP_CONTEXT_BASE_DIRECTORY", std::string("/sharp-runtime-2256-override/"));
    EXPECT_EQ("/sharp-runtime-2256-override/", AppContext::getBaseDirectoryProperty());

    // `as string` -- so a NON-string entry falls through SILENTLY to the computed default rather
    // than throwing. .NET's own comment says the value "has to be a string and it is not allowed
    // to be any other type", and an `as` cast is how it enforces that. This row is the reason the
    // pointer form of any_cast is used rather than the throwing one.
    AppContext::SetData("APP_CONTEXT_BASE_DIRECTORY", 42);
    EXPECT_EQ(before, AppContext::getBaseDirectoryProperty());

    AppContext::SetData("APP_CONTEXT_BASE_DIRECTORY", std::any{});
    EXPECT_EQ(before, AppContext::getBaseDirectoryProperty());
}

TEST(AppContextNamedDataTests, Fix2255_TryGetSwitchFallsBackToAStringValuedDataEntry) {
    // `AppContext.cs:158-161`:
    //     if (GetData(switchName) is string value && bool.TryParse(value, out isEnabled))
    AppContext::SetData("sharp-runtime.2256.stringSwitch", std::string("true"));
    bool isEnabled = false;
    EXPECT_TRUE(AppContext::TryGetSwitch("sharp-runtime.2256.stringSwitch", isEnabled));
    EXPECT_TRUE(isEnabled);

    AppContext::SetData("sharp-runtime.2256.stringSwitch", std::string("  FALSE  "));
    isEnabled = true;
    EXPECT_TRUE(AppContext::TryGetSwitch("sharp-runtime.2256.stringSwitch", isEnabled));
    EXPECT_FALSE(isEnabled) << "bool.TryParse is case-insensitive and trims whitespace";

    // THE PARSE IS .NET's bool.TryParse AND NOT A LAXER ONE. "1", "yes" and "on" are NOT booleans
    // there, and accepting them here would be a quiet widening -- a switch .NET reports as unset
    // would report as ON.
    for (const char* notABool : {"1", "0", "yes", "on", "", "truthy"}) {
        SCOPED_TRACE(notABool);
        AppContext::SetData("sharp-runtime.2256.stringSwitch", std::string(notABool));
        isEnabled = true;
        EXPECT_FALSE(AppContext::TryGetSwitch("sharp-runtime.2256.stringSwitch", isEnabled));
        EXPECT_FALSE(isEnabled) << "set to false on failure, per the contract";
    }

    // A NON-string entry is not a switch either, and must not be reinterpreted as one.
    AppContext::SetData("sharp-runtime.2256.stringSwitch", true);
    isEnabled = true;
    EXPECT_FALSE(AppContext::TryGetSwitch("sharp-runtime.2256.stringSwitch", isEnabled));

    // An EXPLICIT switch still wins over the data entry, as .NET checks the switch map first.
    AppContext::SetData("sharp-runtime.2256.stringSwitch", std::string("false"));
    AppContext::SetSwitch("sharp-runtime.2256.stringSwitch", true);
    isEnabled = false;
    EXPECT_TRUE(AppContext::TryGetSwitch("sharp-runtime.2256.stringSwitch", isEnabled));
    EXPECT_TRUE(isEnabled) << "the explicit switch map is consulted first";

    AppContext::SetData("sharp-runtime.2256.stringSwitch", std::any{});
}

TEST(AppContextNamedDataTests, TheDataAndSwitchMapsAreIndependent) {
    AppContext::SetData("sharp-runtime.2256.independent", 7);
    AppContext::SetSwitch("sharp-runtime.2256.independent", true);

    // Same name, two maps, two unrelated answers.
    EXPECT_EQ(7, std::any_cast<int>(AppContext::GetData("sharp-runtime.2256.independent")));
    bool isEnabled = false;
    EXPECT_TRUE(AppContext::TryGetSwitch("sharp-runtime.2256.independent", isEnabled));
    EXPECT_TRUE(isEnabled);

    // And setting one does not disturb the other.
    AppContext::SetSwitch("sharp-runtime.2256.independent", false);
    EXPECT_EQ(7, std::any_cast<int>(AppContext::GetData("sharp-runtime.2256.independent")));
}

// ---------------------------------------------------------------------------
// The data store's ownership and replacement contract.
// ---------------------------------------------------------------------------

TEST(AppContextNamedDataTests, Fix2255_AStoredNullPointerIsDistinguishableFromAnAbsentKey) {
    // INVERTED BY #2255. With a void* store, "stored nullptr" and "never stored" were ONE state,
    // which is why there was no removal door and no way to say "present but null". A std::any
    // holding a null pointer HAS a value; an absent key does not.
    EXPECT_FALSE(AppContext::GetData("sharp-runtime.2256.neverStored").has_value());

    AppContext::SetData("sharp-runtime.2256.storedNull", static_cast<int*>(nullptr));
    const std::any stored = AppContext::GetData("sharp-runtime.2256.storedNull");
    EXPECT_TRUE(stored.has_value()) << "a stored null POINTER is a present value";
    EXPECT_TRUE(std::any_cast<int*>(stored) == nullptr);

    // ...and storing an EMPTY any is how a caller now says "no value", which is the closest thing
    // to a removal door and did not exist before.
    AppContext::SetData("sharp-runtime.2256.storedNull", std::any{});
    EXPECT_FALSE(AppContext::GetData("sharp-runtime.2256.storedNull").has_value());
}

TEST(AppContextNamedDataTests, SetDataReplacesAnExistingKey) {
    AppContext::SetData("sharp-runtime.2256.replace", 1);
    ASSERT_EQ(1, std::any_cast<int>(AppContext::GetData("sharp-runtime.2256.replace")));
    AppContext::SetData("sharp-runtime.2256.replace", 2);
    EXPECT_EQ(2, std::any_cast<int>(AppContext::GetData("sharp-runtime.2256.replace")));

    // #2255: replacing with a DIFFERENT TYPE is legal too, and the type moves with the value --
    // which the untyped store could not have represented at all.
    AppContext::SetData("sharp-runtime.2256.replace", std::string("three"));
    EXPECT_EQ("three", std::any_cast<std::string>(AppContext::GetData("sharp-runtime.2256.replace")));
}

TEST(AppContextNamedDataTests, Fix2255_TheStoreOwnsItsValuesAndCarriesTheirType) {
    // INVERTED BY #2255. This test used to pin that the store held a BORROWED pointer -- so a
    // caller who mutated the target afterwards saw the new value, and a caller who stored a
    // pointer to a temporary left a dangling entry GetData handed straight back. .NET's SetData
    // stores a BOXED OBJECT, which owns its value and can be asked its runtime type.
    int payload = 10;
    AppContext::SetData("sharp-runtime.2256.borrowed", payload);
    payload = 20;
    const std::any stored = AppContext::GetData("sharp-runtime.2256.borrowed");
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(10, std::any_cast<int>(stored)) << "the store OWNS a copy -- 20 would mean it borrowed";

    // The type is interrogable, which is the property both of SR-AUD-102's behaviours needed and
    // which a void* could not provide at all.
    EXPECT_TRUE(stored.type() == typeid(int));
    EXPECT_TRUE(std::any_cast<std::string>(&stored) == nullptr)
        << "asking the wrong type is now SAFE -- a void* could only be reinterpreted blindly";

    // A pointer can still be stored, for a caller that genuinely wants one -- it is simply no
    // longer the only thing the store can hold.
    AppContext::SetData("sharp-runtime.2256.pointer", &payload);
    EXPECT_EQ(&payload, std::any_cast<int*>(AppContext::GetData("sharp-runtime.2256.pointer")));
}

TEST(AppContextNamedDataTests, AppDomainSeesTheSameStore) {
    // #2249 made AppDomain::SetData/GetData forwarders. This pins the direction #2255's route A
    // would have to change on BOTH classes at once.
    int payload = 42;
    AppContext::SetData("sharp-runtime.2256.shared", payload);
    EXPECT_EQ(42, std::any_cast<int>(
                      System::AppDomain::CurrentDomain().GetData("sharp-runtime.2256.shared")));
}

// ---------------------------------------------------------------------------
// Reference lifetime and the reflection deviation.
// ---------------------------------------------------------------------------

TEST(AppContextNamedDataTests, Fix2255_BaseDirectoryReturnsByValueNow) {
    // INVERTED BY #2255, AND THIS TEST PREDICTED IT: its old comment said the stable reference is
    // "exactly what an APP_CONTEXT_BASE_DIRECTORY override would put at risk, and why #2255's
    // option (a) also asks about the return type". It does, and the answer is a VALUE -- the
    // override is materialised inside the accessor, so there is no stable storage to lend.
    static_assert(std::is_same_v<decltype(AppContext::getBaseDirectoryProperty()), std::string>,
                  "#2255: by value, because the override has no process-lifetime home");

    const std::string held = AppContext::getBaseDirectoryProperty();
    EXPECT_FALSE(held.empty());
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(held, AppContext::getBaseDirectoryProperty()) << "the VALUE is still stable";
    }

    // ...and it tracks the override, which is the whole reason the reference had to go.
    AppContext::SetData("APP_CONTEXT_BASE_DIRECTORY", std::string("/sharp-runtime-2255-value/"));
    EXPECT_EQ("/sharp-runtime-2255-value/", AppContext::getBaseDirectoryProperty());
    AppContext::SetData("APP_CONTEXT_BASE_DIRECTORY", std::any{});
    EXPECT_EQ(held, AppContext::getBaseDirectoryProperty());
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
                const std::any read = AppContext::GetData(dataKey);
                const int* const* stored = std::any_cast<int*>(&read);
                if (stored == nullptr || *stored != &payloads[static_cast<std::size_t>(t)]) {
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
        EXPECT_EQ(&payloads[static_cast<std::size_t>(t)],
                  std::any_cast<int*>(
                      AppContext::GetData("sharp-runtime.2256.concurrent.data." + std::to_string(t))));
    }
}
