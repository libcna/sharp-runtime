// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1969 (cause TC-B/3).
//
// #1969 made System::Threading::Channels::BoundedChannelOptions::FullMode a private member
// behind getFullModeProperty()/setFullModeProperty(), matching .NET's validating property
// (ChannelOptions.cs:79-97). It was a bare public mutable data member, and the obstacle was
// the field's SHAPE rather than any missing logic: a data member has nowhere to put a check.
//
// The consequence was not cosmetic. With capacity 1 and static_cast<BoundedChannelFullMode>(99),
// the writer took the drop path (the mode is not Wait) and then matched no arm of the drop
// switch, so nothing was dropped and the item was appended anyway -- Count reached 2 on a
// channel bounded at 1. A caller, or a deserialized value, could defeat the bounded-memory
// contract outright.
//
// Migration: `opts.FullMode = m` becomes `opts.setFullModeProperty(m)`, and `opts.FullMode`
// becomes `opts.getFullModeProperty()`. An undeclared enumerator now throws
// ArgumentOutOfRangeException with parameter name "value" -- .NET's nameof(value), not the
// property name.
//
// Records: docs/Migration-ChannelFullModeValidation.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Threading.Channels
#include "System/Threading/Channels/ChannelOptions.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Threading::Channels::BoundedChannelFullMode;
using System::Threading::Channels::BoundedChannelOptions;

int main() {
    BoundedChannelOptions options(2);

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(channels-fullmode-direct-write): is private within this context
    //     | private
    options.FullMode = BoundedChannelFullMode::DropOldest;
#else
    options.setFullModeProperty(BoundedChannelFullMode::DropOldest);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(channels-fullmode-direct-read): is private within this context
    //     | private
    BoundedChannelFullMode mode = options.FullMode;
    (void)mode;
#else
    BoundedChannelFullMode mode = options.getFullModeProperty();
    (void)mode;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(channels-fullmode-undeclared-write): is private within this context
    //     | private
    // THE SITE THIS TICKET EXISTS FOR: the spelling that used to compile AND run, storing a
    // value no switch arm handles and defeating the channel's bound. It is now rejected at
    // compile time in this form, and at run time as ArgumentOutOfRangeException in the
    // migrated form below.
    options.FullMode = static_cast<BoundedChannelFullMode>(99);
#else
    // The migrated spelling throws rather than storing -- which a compile fixture cannot
    // assert, so ChannelFullModeValidationTests.Fix1969_AnUndeclaredValueIsRejected does.
#endif

    // UNCHANGED, and asserted so the fixture proves what did NOT break: Capacity already had
    // its accessor pair and its .NET validation, and the three base flags are deliberately
    // still public data members because .NET's are unvalidated auto-properties.
    options.setCapacityProperty(4);
    options.SingleWriter = true;
    options.SingleReader = true;
    options.AllowSynchronousContinuations = true;
    return (options.getCapacityProperty() == 4 && options.SingleWriter) ? 0 : 1;
}
