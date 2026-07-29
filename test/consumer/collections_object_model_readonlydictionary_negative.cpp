// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1780 / SR-AUD-359: proves that
// assignment through System::Collections::ObjectModel::ReadOnlyDictionary
// <K,V>::Empty()'s result is a compile-time error, not a silent process-wide
// rebind of the shared empty singleton.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by
// the compiler; with no site selected the file compiles cleanly. Ticket #1801's
// tracked checker, scripts/check_negative_consumer_fixtures.py, compiles each
// site on its own and asserts the rejection; the record is
// docs/NegativeConsumerFixtureValidation.md. Until #1801 this file named a
// `scripts/check_readonlydict_empty_negative.sh` that never existed in the
// repository.
//
// NEGATIVE-FIXTURE: component=Collections.ObjectModel
#include <memory>
#include <string>
#include <unordered_map>

#include "System/Collections/ObjectModel/ReadOnlyDictionary.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Collections::ObjectModel::ReadOnlyDictionary;

int main() {
    auto& empty = ReadOnlyDictionary<std::string, int>::Empty();
    auto contaminated = std::make_shared<std::unordered_map<std::string, int>>();
    (*contaminated)["leaked"] = 1;
    ReadOnlyDictionary<std::string, int> nonEmpty(contaminated);

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(empty-singleton-rebind): as 'this' argument discards qualifiers
    //     | assignment of read-only reference
    //     | discards qualifiers
    empty = nonEmpty;
#endif

    (void)empty;
    (void)nonEmpty;
    return 0;
}
