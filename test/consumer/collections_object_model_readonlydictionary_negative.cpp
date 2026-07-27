// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1780 / SR-AUD-359: proves that
// assignment through System::Collections::ObjectModel::ReadOnlyDictionary
// <K,V>::Empty()'s result is a compile-time error, not a silent process-wide
// rebind of the shared empty singleton. This file is deliberately excluded
// from every normal build target (see test/consumer/CMakeLists.txt and
// scripts/check_readonlydict_empty_negative.sh) and must never compile
// successfully.
#include <memory>
#include <string>
#include <unordered_map>

#include "System/Collections/ObjectModel/ReadOnlyDictionary.hpp"

using System::Collections::ObjectModel::ReadOnlyDictionary;

int main() {
    auto& empty = ReadOnlyDictionary<std::string, int>::Empty();
    auto contaminated = std::make_shared<std::unordered_map<std::string, int>>();
    (*contaminated)["leaked"] = 1;
    ReadOnlyDictionary<std::string, int> nonEmpty(contaminated);
    empty = nonEmpty; // must fail: Empty() returns const&, so this discards qualifiers
    return 0;
}
