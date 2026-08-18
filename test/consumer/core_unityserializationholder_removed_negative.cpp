// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2281 (SR-AUD-137).
//
// #2281 REMOVED System::UnitySerializationHolder, under
// docs/StandingApprovals.md SA-9.
//
// A PREMISE CORRECTION FIRST, because both the ticket and the old header stated
// the opposite: .NET's UnitySerializationHolder is NOT internal. It is
// `public sealed class UnitySerializationHolder : ISerializable,
// IObjectReference` (UnitySerializationHolder.cs:15), carrying an explicit
// comment -- "Needs to be public to support binary serialization
// compatibility". What IS internal there is only the NullUnity constant.
//
// The reason for removal is therefore different from, and better than, the one
// the ticket gave. .NET's own summary says the type "only exists for
// compatibility with .NET Framework"; it is `[Obsolete(LegacyFormatterMessage)]`
// and every one of its public members takes SerializationInfo and
// StreamingContext. CLAUDE.md records serialization infrastructure as a
// PERMANENT project deviation and BinaryFormatter is not implemented -- so the
// mechanism this type exists to serve does not exist here and never will. What
// the port published in its place was neither .NET's shape nor internal: a
// public NullUnity, a raw (intcs, string) constructor and getters for both
// fields, letting an ordinary caller fabricate a holder state .NET only ever
// builds from a serialization stream.
//
// Records: docs/Migration-UnitySerializationHolderRemoval.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base
//
// NOTE ON WHAT THIS FIXTURE DOES *NOT* INCLUDE. The obvious survivor to assert here would be
// DBNull::Value(), the only thing the holder ever produced. It is deliberately absent: including
// System/DBNull.hpp pulls in Decimal.hpp through IConvertible.hpp, and Decimal's `unsigned
// __int128` is rejected under this checker's `-Wpedantic -Werror`, so the fixture's BASELINE
// would not compile -- and a fixture whose baseline is broken cannot attribute any site's
// rejection to its own source. DBNull is covered by the repository suites instead.
#include <type_traits>

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
// NEGATIVE(unityserializationholder-header-gone): No such file or directory
//     | file not found
#include "System/UnitySerializationHolder.hpp"
#endif

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(unityserializationholder-name-gone): is not a member of
    //     | has not been declared
    //     | no member named
    System::UnitySerializationHolder holder(2);
    (void)holder;
#endif

    return 0;
}
