// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <type_traits>
#include <functional>
#include <string>
#include <vector>
#include "System/EventHandler.hpp"
#include "System/EventArgs.hpp"

using System::EventHandler;
using System::EventArgs;

TEST(EventHandlerTests, DefaultCtor_Empty) {
    EventHandler<EventArgs> h;
    EXPECT_TRUE(h.Empty());
    EXPECT_EQ(h.Size(), 0u);
}

TEST(EventHandlerTests, AddOne_SizeIsOne) {
    EventHandler<EventArgs> h;
    h += [](System::Object*, const EventArgs&) {};
    EXPECT_EQ(h.Size(), 1u);
}

TEST(EventHandlerTests, Raise_SingleHandler_IsCalled) {
    EventHandler<EventArgs> h;
    int called = 0;
    h += [&called](System::Object*, const EventArgs&) { ++called; };
    h.Raise(nullptr, EventArgs::Empty);
    EXPECT_EQ(called, 1);
}

TEST(EventHandlerTests, Raise_MultipleHandlers_AllCalled) {
    EventHandler<EventArgs> h;
    int a = 0, b = 0;
    h += [&a](System::Object*, const EventArgs&) { ++a; };
    h += [&b](System::Object*, const EventArgs&) { ++b; };
    h.Raise(nullptr, EventArgs::Empty);
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 1);
}

TEST(EventHandlerTests, Raise_HandlersCalledInOrder) {
    EventHandler<EventArgs> h;
    std::vector<int> order;
    h += [&order](System::Object*, const EventArgs&) { order.push_back(1); };
    h += [&order](System::Object*, const EventArgs&) { order.push_back(2); };
    h += [&order](System::Object*, const EventArgs&) { order.push_back(3); };
    h.Raise(nullptr, EventArgs::Empty);
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST(EventHandlerTests, Invoke_AliasForRaise) {
    EventHandler<EventArgs> h;
    int called = 0;
    h += [&called](System::Object*, const EventArgs&) { ++called; };
    h.Invoke(nullptr, EventArgs::Empty);
    EXPECT_EQ(called, 1);
}

TEST(EventHandlerTests, Remove_ByToken_HandlerNotCalled) {
    EventHandler<EventArgs> h;
    int called = 0;
    auto token = h.Add([&called](System::Object*, const EventArgs&) { ++called; });
    h.Remove(token);
    h.Raise(nullptr, EventArgs::Empty);
    EXPECT_EQ(called, 0);
    EXPECT_TRUE(h.Empty());
}

TEST(EventHandlerTests, Remove_UnknownToken_NoEffect) {
    EventHandler<EventArgs> h;
    int called = 0;
    h += [&called](System::Object*, const EventArgs&) { ++called; };
    h.Remove(9999); // no-op
    h.Raise(nullptr, EventArgs::Empty);
    EXPECT_EQ(called, 1);
}

TEST(EventHandlerTests, Clear_RemovesAllHandlers) {
    EventHandler<EventArgs> h;
    h += [](System::Object*, const EventArgs&) {};
    h += [](System::Object*, const EventArgs&) {};
    h.Clear();
    EXPECT_TRUE(h.Empty());
    EXPECT_EQ(h.Size(), 0u);
}

TEST(EventHandlerTests, Raise_WithCustomEventArgs) {
    struct CountArgs : public EventArgs { int count = 7; };
    EventHandler<CountArgs> h;
    int received = 0;
    h += [&received](System::Object*, const CountArgs& e) { received = e.count; };
    CountArgs args;
    h.Raise(nullptr, args);
    EXPECT_EQ(received, 7);
}

TEST(EventHandlerTests, Raise_Empty_DoesNotThrow) {
    EventHandler<EventArgs> h;
    EXPECT_NO_THROW(h.Raise(nullptr, EventArgs::Empty));
}

// Raise() must invoke a snapshot of the handler list, matching C# multicast delegate semantics:
// a handler that removes itself (or another handler) mid-raise must not corrupt the current
// invocation. Without a snapshot, this used to dereference an already-destroyed std::function
// (undefined behavior, observed as an escaping std::bad_function_call).
TEST(EventHandlerTests, Raise_HandlerRemovesItselfDuringRaise_DoesNotThrowAndStillFiresOthers) {
    EventHandler<EventArgs> h;
    int firedA = 0, firedB = 0;
    EventHandler<EventArgs>::Token tokenA = 0;
    tokenA = h.Add([&](System::Object*, const EventArgs&) {
        ++firedA;
        h.Remove(tokenA);
    });
    h += [&firedB](System::Object*, const EventArgs&) { ++firedB; };

    EXPECT_NO_THROW(h.Raise(nullptr, EventArgs::Empty));
    EXPECT_EQ(firedA, 1);
    EXPECT_EQ(firedB, 1); // still fires in this same Raise() -- the snapshot already included it

    // The removal takes effect for the *next* Raise(), matching C# semantics.
    h.Raise(nullptr, EventArgs::Empty);
    EXPECT_EQ(firedA, 1); // not called again -- it removed itself before the first Raise() finished
    EXPECT_EQ(firedB, 2);
}

// A handler removing a *different*, not-yet-invoked handler mid-raise must not skip or crash on
// that handler either -- the snapshot already captured it before the removal happened.
TEST(EventHandlerTests, Raise_HandlerRemovesAnotherHandlerDuringRaise_StillFiresBoth) {
    EventHandler<EventArgs> h;
    int firedA = 0, firedB = 0;
    EventHandler<EventArgs>::Token tokenB = 0;
    h += [&](System::Object*, const EventArgs&) {
        ++firedA;
        h.Remove(tokenB);
    };
    tokenB = h.Add([&firedB](System::Object*, const EventArgs&) { ++firedB; });

    EXPECT_NO_THROW(h.Raise(nullptr, EventArgs::Empty));
    EXPECT_EQ(firedA, 1);
    EXPECT_EQ(firedB, 1);
    EXPECT_EQ(h.Size(), 1u); // A is still subscribed; only B was removed
}

// SetReplayHook(): models real XNA events like NetworkSession.GamerJoined, which replay already-
// happened state to a handler the instant it subscribes, not just for future occurrences.

TEST(EventHandlerTests, NoReplayHookSet_AddBehavesExactlyAsBefore) {
    EventHandler<EventArgs> h;
    int called = 0;
    h += [&called](System::Object*, const EventArgs&) { ++called; };
    EXPECT_EQ(called, 0); // Add() alone never invokes a handler when no hook is set
    EXPECT_EQ(h.Size(), 1u);
}

TEST(EventHandlerTests, ReplayHook_CalledOnceImmediatelyForEachNewSubscriber) {
    EventHandler<EventArgs> h;
    int replayCount = 0;
    h.SetReplayHook([&](const EventHandler<EventArgs>::HandlerType& handler) {
        ++replayCount;
        handler(nullptr, EventArgs::Empty);
    });

    int firedA = 0;
    h += [&firedA](System::Object*, const EventArgs&) { ++firedA; };
    EXPECT_EQ(replayCount, 1);
    EXPECT_EQ(firedA, 1);

    int firedB = 0;
    h += [&firedB](System::Object*, const EventArgs&) { ++firedB; };
    EXPECT_EQ(replayCount, 2);
    EXPECT_EQ(firedB, 1);
    EXPECT_EQ(firedA, 1); // unaffected by the second subscription's own replay
}

TEST(EventHandlerTests, ReplayHook_DoesNotAffectSizeOrSubsequentRaise) {
    EventHandler<EventArgs> h;
    h.SetReplayHook([](const EventHandler<EventArgs>::HandlerType& handler) {
        handler(nullptr, EventArgs::Empty);
    });

    int called = 0;
    h += [&called](System::Object*, const EventArgs&) { ++called; };
    EXPECT_EQ(called, 1); // from the replay
    EXPECT_EQ(h.Size(), 1u); // the hook call itself is not a second subscriber

    h.Raise(nullptr, EventArgs::Empty);
    EXPECT_EQ(called, 2); // normal Raise() still reaches it exactly once
}

TEST(EventHandlerTests, ReplayHook_ClearedBySettingEmptyFunction) {
    EventHandler<EventArgs> h;
    int replayCount = 0;
    h.SetReplayHook([&](const EventHandler<EventArgs>::HandlerType&) { ++replayCount; });
    h.SetReplayHook(nullptr); // clears it

    h += [](System::Object*, const EventArgs&) {};
    EXPECT_EQ(replayCount, 0);
}

// --- Empty subscription is a no-op (#1868 / SR-AUD-121 / CCF-011) ---
//
// Add() used to store every handler, including an empty std::function, so Size() counted a
// subscriber that could never run and Raise() reached the native std::bad_function_call.
// With a replay hook set the failure happened inside Add() itself, because the hook calls
// the handler directly. C# `SomethingHappened += null` is Delegate.Combine(d, null) == d.
// See docs/EmptyCallableBoundaryPlan.md.

TEST(EventHandlerTests, EmptyHandler_AddDoesNotThrowAndStoresNothing) {
    EventHandler<EventArgs> h;
    EXPECT_NO_THROW((void)h.Add(EventHandler<EventArgs>::HandlerType{}));
    EXPECT_TRUE(h.Empty());
    EXPECT_EQ(h.Size(), 0u);
}

TEST(EventHandlerTests, EmptyHandler_PlusEqualsDoesNotThrowAndStoresNothing) {
    EventHandler<EventArgs> h;
    EXPECT_NO_THROW(h += EventHandler<EventArgs>::HandlerType{});
    EXPECT_TRUE(h.Empty());
    EXPECT_EQ(h.Size(), 0u);
}

TEST(EventHandlerTests, EmptyHandler_RaiseDoesNotThrow) {
    EventHandler<EventArgs> h;
    h += EventHandler<EventArgs>::HandlerType{};
    EXPECT_NO_THROW(h.Raise(nullptr, EventArgs::Empty));
    EXPECT_NO_THROW(h.Invoke(nullptr, EventArgs::Empty));
}

TEST(EventHandlerTests, EmptyHandler_ReplayHookIsNotInvoked) {
    EventHandler<EventArgs> h;
    int replayCount = 0;
    h.SetReplayHook([&](const EventHandler<EventArgs>::HandlerType& handler) {
        ++replayCount;
        handler(nullptr, EventArgs::Empty);   // would be std::bad_function_call if reached
    });
    EXPECT_NO_THROW((void)h.Add(EventHandler<EventArgs>::HandlerType{}));
    EXPECT_EQ(replayCount, 0);
    EXPECT_EQ(h.Size(), 0u);
}

TEST(EventHandlerTests, EmptyHandler_DoesNotDisturbRealSubscribers) {
    EventHandler<EventArgs> h;
    std::vector<int> order;
    h += [&](System::Object*, const EventArgs&) { order.push_back(1); };
    h += EventHandler<EventArgs>::HandlerType{};
    h += [&](System::Object*, const EventArgs&) { order.push_back(2); };
    EXPECT_EQ(h.Size(), 2u);
    h.Raise(nullptr, EventArgs::Empty);
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
}

TEST(EventHandlerTests, EmptyHandler_TokenIsUniqueAndSafeToRemove) {
    EventHandler<EventArgs> h;
    const auto ignored = h.Add(EventHandler<EventArgs>::HandlerType{});
    int called = 0;
    const auto real = h.Add([&](System::Object*, const EventArgs&) { ++called; });
    EXPECT_NE(ignored, real);          // reusing the token would unsubscribe the real one
    EXPECT_NO_THROW(h.Remove(ignored));
    EXPECT_EQ(h.Size(), 1u);
    h.Raise(nullptr, EventArgs::Empty);
    EXPECT_EQ(called, 1);
    h.Remove(real);
    EXPECT_EQ(h.Size(), 0u);
}

TEST(EventHandlerTests, EmptyHandler_ManyTimes_StillSilent) {
    EventHandler<EventArgs> h;
    for (int i = 0; i < 100; ++i) h += EventHandler<EventArgs>::HandlerType{};
    EXPECT_EQ(h.Size(), 0u);
    int called = 0;
    h += [&](System::Object*, const EventArgs&) { ++called; };
    EXPECT_NO_THROW(h.Raise(nullptr, EventArgs::Empty));
    EXPECT_EQ(called, 1);
}

TEST(EventHandlerTests, EmptyHandler_AddedDuringRaise_IsStillANoOp) {
    // Raise() iterates a snapshot; a handler that subscribes an empty handler mid-raise
    // must neither be invoked in this raise nor break the next one.
    EventHandler<EventArgs> h;
    int called = 0;
    h += [&](System::Object*, const EventArgs&) {
        ++called;
        h += EventHandler<EventArgs>::HandlerType{};
    };
    EXPECT_NO_THROW(h.Raise(nullptr, EventArgs::Empty));
    EXPECT_EQ(called, 1);
    EXPECT_EQ(h.Size(), 1u);
    EXPECT_NO_THROW(h.Raise(nullptr, EventArgs::Empty));
    EXPECT_EQ(called, 2);
}

// ---------------------------------------------------------------------------
// #2324 / SR-AUD-122 — event args are read-only to subscribers, DELIBERATELY
// ---------------------------------------------------------------------------
//
// NOT A REPAIR — A MEASURED DECISION, pinned so it cannot later be mistaken for an oversight.
//
// .NET's `delegate void EventHandler<TEventArgs>(object? sender, TEventArgs e)` hands a
// subscriber a mutable reference, which is what the acknowledgement and cancellation patterns
// (CancelEventArgs.Cancel, HandledEventArgs.Handled) are built on. This port's HandlerType takes
// `const TEventArgs&`, so a subscriber cannot write back.
//
// SA-8 DOES NOT DECIDE THIS ONE. It covers the case where this port is MORE PERMISSIVE than
// .NET -- "a mutable or public representation where .NET's is private, readonly or absent". Here
// the port is more RESTRICTIVE, which SA-8 says nothing about.
//
// THE REPAIR WAS BUILT AND MEASURED BEFORE BEING DECLINED (2026-08-18). Dropping the const across
// all nine *EventHandler aliases costs:
//
//   * 29 first-party call sites, all of the shape `h.Raise(nullptr, EventArgs::Empty)`;
//   * `EventArgs::Empty`, which is `static const EventArgs` and would have to become non-const --
//     a process-wide mutable global. (.NET's `static readonly EventArgs Empty` is the same shape
//     and is harmless only because EventArgs has no fields);
//   * a deliberate tripwire belonging to ANOTHER ticket:
//     XLinqChangeNotificationTests asserts "XObjectChangeEventHandler is no longer a bare
//     std::function alias. SR-AUD-336's removal blocker was derived from that fact -- re-derive
//     #2199 before proceeding."
//
// And it buys, today, NOTHING: measured, the only two EventArgs types in this repository with a
// settable property -- ConsoleCancelEventArgs and CancelEventArgs -- are not delivered through
// ANY handler alias. The pattern the finding invokes has no instance here.
//
// THE TRIGGER FOR REVISITING is therefore precise: the first event that delivers a settable args
// object. At that point #2199 must be re-derived first, and EventArgs::Empty must move with the
// aliases.

TEST(EventHandlerContractTests, Decl2324_SubscribersReceiveEventArgsByConstReference) {
    static_assert(
        std::is_same_v<EventHandler<EventArgs>::HandlerType,
                       std::function<void(System::Object*, const EventArgs&)>>,
        "#2324: the const is deliberate; see the comment above for the measured cost of removing it");

    // The observable half: a raiser may pass a const args object, which is what
    // EventArgs::Empty is, and which a mutable-reference handler could not accept.
    EventHandler<EventArgs> h;
    int called = 0;
    h += [&called](System::Object*, const EventArgs&) { ++called; };
    h.Raise(nullptr, EventArgs::Empty);
    EXPECT_EQ(1, called);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(EventArgs::Empty)>>,
                  "#2324: EventArgs::Empty is const, and would have to stop being so");
}

TEST(EventHandlerContractTests, Decl2324_TheCancellableArgsTypesReachNoHandlerAlias) {
    // The measurement that decided it, kept executable rather than left in a commit message: the
    // two settable-property EventArgs types exist, and nothing delivers them. If that ever
    // changes, this test is where the change should be noticed.
    //
    // It is deliberately a STRUCTURAL assertion about EventArgs, not a search: a test cannot
    // grep. What it CAN pin is that the base every args type derives from is delivered by const
    // reference, so adding a settable member to one changes nothing until an alias moves too.
    static_assert(std::is_same_v<EventHandler<EventArgs>::HandlerType,
                                 std::function<void(System::Object*, const EventArgs&)>>,
                  "#2324");
    SUCCEED() << "see the comment block above for the nine-alias measurement";
}
