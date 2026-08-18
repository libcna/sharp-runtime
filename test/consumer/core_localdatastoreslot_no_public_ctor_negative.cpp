// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2298 (SR-AUD-129), route B.
//
// #2298 made System::LocalDataStoreSlot's constructor private and gave
// System::Threading::Thread the six .NET data-slot doors, under
// docs/StandingApprovals.md SA-9, which authorised that new public surface
// explicitly.
//
// Before it, the type had a PUBLIC constructor and a getData/setData pair, and
// this repository had no Thread slot API at all -- so the whole surface was
// project-owned, wearing a .NET name whose constructor is `internal`
// (LocalDataStoreSlot.cs:11). Worse, the single std::any it held was ONE VALUE
// SHARED BY EVERY THREAD, so a write from any thread replaced what every other
// thread read, and two threads touching one slot with at least one write was a
// data race.
//
// Migration: allocate through Thread::AllocateDataSlot() or its named siblings,
// and read and write through Thread::GetData / Thread::SetData.
//
// Records: docs/Migration-LocalDataStoreSlotThreadDoors.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <cstddef>
#include <type_traits>

#include "System/LocalDataStoreSlot.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::LocalDataStoreSlot;

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(localdatastoreslot-default-construction): is private within this context
    //     | private
    //     | no matching function
    LocalDataStoreSlot slot;
    (void)slot;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(localdatastoreslot-getdata-member): has no member named
    //     | no member named
    //     | is private within this context
    LocalDataStoreSlot* slot = nullptr;
    (void)slot->getData();
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(localdatastoreslot-still-default-constructible): static assertion failed
    //     | static_assert
    // The shape that breaks a consumer SILENTLY: a trait query.
    static_assert(std::is_default_constructible_v<LocalDataStoreSlot>,
                  "LocalDataStoreSlot is expected to be default-constructible");
#else
    static_assert(!std::is_default_constructible_v<LocalDataStoreSlot>,
                  "#2298: Thread is the only door, as .NET's internal constructor arranges");
    static_assert(!std::is_constructible_v<LocalDataStoreSlot, std::size_t>,
                  "#2298: not even the id-taking constructor is reachable");
#endif

    // UNCHANGED, and asserted so the fixture proves what a slot still IS: a copyable handle,
    // because .NET's is a reference passed around freely. A non-copyable slot would break every
    // caller that stores one.
    static_assert(std::is_copy_constructible_v<LocalDataStoreSlot>, "a slot is a handle");
    static_assert(std::is_copy_assignable_v<LocalDataStoreSlot>, "a slot is a handle");
    return 0;
}
