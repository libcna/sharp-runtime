// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2297 (SR-AUD-128).
//
// #2297 made System::MarshalByRefObject's constructor protected, matching
// .NET's `protected MarshalByRefObject()` -- C# rejects the equivalent with
// CS0144 -- and added GetLifetimeService(), which .NET keeps precisely so that
// a caller receives PlatformNotSupportedException rather than a compile error
// at an unrelated place.
//
// What it deliberately did NOT add is InitializeLifetimeService(). .NET's is
// VIRTUAL, and this class already has a vtable (the destructor), so adding it
// inserts a SLOT -- a vtable change here and in both derived classes, AppDomain
// and ContextBoundObject. docs/StandingApprovals.md SA-3 excludes vtable
// changes explicitly. Ticket #2374 carries that approval request.
//
// Records: docs/Migration-MarshalByRefObjectProtectedConstructor.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <type_traits>

#include "System/MarshalByRefObject.hpp"
#include "System/PlatformNotSupportedException.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::MarshalByRefObject;

namespace {
/// The migrated shape: derive, which is what a remotable type always had to do.
class MyRemotable : public MarshalByRefObject {};
}  // namespace

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(marshalbyrefobject-direct-instantiation): is protected within this context
    //     | protected
    MarshalByRefObject direct;
    (void)direct;
#else
    MyRemotable direct;
    (void)direct;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(marshalbyrefobject-heap-instantiation): is protected within this context
    //     | protected
    MarshalByRefObject* heap = new MarshalByRefObject();
    delete heap;
#else
    MarshalByRefObject* heap = new MyRemotable();
    delete heap;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(marshalbyrefobject-copy-slice): is protected within this context
    //     | protected
    //     | use of deleted function
    MyRemotable derived;
    MarshalByRefObject sliced = derived;
    (void)sliced;
#else
    MyRemotable derived;
    const MarshalByRefObject& notSliced = derived;
    (void)notSliced;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(marshalbyrefobject-still-default-constructible): static assertion failed
    //     | static_assert
    // The shape that breaks a consumer SILENTLY: a trait query.
    static_assert(std::is_default_constructible_v<MarshalByRefObject>,
                  "MarshalByRefObject is expected to be default-constructible");
#else
    static_assert(!std::is_default_constructible_v<MarshalByRefObject>,
                  "#2297: the constructor is protected, as .NET's is");
    static_assert(std::is_default_constructible_v<MyRemotable>,
                  "#2297: a derived type is still constructible -- that is the point");
#endif

    // UNCHANGED / ADDED, and asserted so the fixture proves what the ticket DID provide:
    // GetLifetimeService() exists and is reachable from a derived type. It is not virtual in
    // .NET, so adding it cost no vtable slot.
    try {
        (void)derived.GetLifetimeService();
        return 1;   // must not be reached
    } catch (const System::PlatformNotSupportedException&) {
        return 0;
    }
}
