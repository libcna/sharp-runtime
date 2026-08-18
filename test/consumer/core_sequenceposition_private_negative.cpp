// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2332 (SR-AUD-069, approval-gated clause
// split out of #2331).
//
// #2332 made System::SequencePosition's two components private, matching .NET's
// private readonly fields. .NET documents that the parts of a position must not
// be interpreted by anything except the sequence that created it, and enforces
// that in the language; this port published both as mutable data members, so a
// caller could rewrite a position after the sequence handed it out -- to an
// unrelated segment, a dangling pointer, or an offset the owner never produced.
//
// Nothing in this repository ever touched them, so there was no first-party
// migration. What a CONSUMER loses is the three spellings that reach a public
// data member, plus the aggregate-ness the first two depend on. Each is
// compiled on its own below; the #else branches are the migrated spellings.
//
// Migration: build positions with the two-argument constructor and read them
// with GetObject() / GetInteger(). Those cover every legitimate use, which is
// why every other type in this repository already used them.
//
// Records: docs/Migration-SequencePositionPrivateComponents.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <type_traits>

#include "System/SequencePosition.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::SequencePosition;

int main() {
    int segment = 0;
    SequencePosition position(&segment, 5);

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(sequenceposition-direct-write): is private within this context
    //     | private
    position.integer_ = 99;
#else
    position = SequencePosition(&segment, 99);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(sequenceposition-direct-read): is private within this context
    //     | private
    void* raw = position.object_;
    (void)raw;
#else
    void* raw = position.GetObject();
    (void)raw;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(sequenceposition-structured-binding): cannot decompose
    //     | private
    //     | non-public
    auto [object, integer] = position;
    (void)object;
    (void)integer;
#else
    void* object = position.GetObject();
    auto  integer = position.GetInteger();
    (void)object;
    (void)integer;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(sequenceposition-still-aggregate): static assertion failed
    //     | static_assert
    // The shape that breaks a consumer SILENTLY rather than loudly: a trait query, or a template
    // constrained on aggregate-ness.
    static_assert(std::is_aggregate_v<SequencePosition>,
                  "SequencePosition is expected to be an aggregate");
#else
    static_assert(!std::is_aggregate_v<SequencePosition>,
                  "#2332: the components are private, so the type is no longer an aggregate");
#endif

    // UNCHANGED, and asserted so the fixture proves what did NOT break. Brace initialisation with
    // two arguments still compiles -- through the constructor rather than as an aggregate -- and
    // the type stays copyable and comparable.
    const SequencePosition braced{&segment, 7};
    const SequencePosition defaulted{};
    const SequencePosition copied = braced;
    static_assert(std::is_copy_constructible_v<SequencePosition>, "still copyable");
    static_assert(std::is_trivially_copyable_v<SequencePosition>, "still trivially copyable");
    return (braced.GetInteger() == 7 && copied == braced && defaulted.GetInteger() == 0) ? 0 : 1;
}
