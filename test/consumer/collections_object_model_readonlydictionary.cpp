// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Standalone public-header consumer fixture for
// System::Collections::ObjectModel::ReadOnlyDictionary<K,V>::Empty() (ticket
// #1780, SR-AUD-359). It compiles against only the public Collections.Core
// surface and exercises Empty() the way an ordinary read-only consumer would:
// through auto&, never through an explicit non-const reference and never
// assigning through the result.
#include <cassert>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "System/Collections/ObjectModel/ReadOnlyDictionary.hpp"

using System::Collections::ObjectModel::ReadOnlyDictionary;

namespace {

using StringIntReadOnlyDictionary = ReadOnlyDictionary<std::string, int>;

static_assert(
    std::is_same_v<decltype(StringIntReadOnlyDictionary::Empty()),
                    const StringIntReadOnlyDictionary&>,
    "Empty() must return a const reference");

bool emptyIsStableAndEmpty() {
    auto& empty1 = StringIntReadOnlyDictionary::Empty();
    auto& empty2 = StringIntReadOnlyDictionary::Empty();
    return empty1.getCountProperty() == 0 && &empty1 == &empty2;
}

bool ordinaryReadOnlyUsageIsUnaffected() {
    auto m = std::make_shared<std::unordered_map<std::string, int>>();
    (*m)["a"] = 1;
    (*m)["b"] = 2;
    StringIntReadOnlyDictionary rd(m);

    if (rd.getCountProperty() != 2) return false;
    if (!rd.ContainsKey("a")) return false;
    if (rd["b"] != 2) return false;

    int found = 0;
    if (!rd.TryGetValue("a", found) || found != 1) return false;

    // Copy-construction from Empty() creates an independent, caller-owned
    // instance and never touches the singleton.
    StringIntReadOnlyDictionary copyOfEmpty = StringIntReadOnlyDictionary::Empty();
    return copyOfEmpty.getCountProperty() == 0
        && StringIntReadOnlyDictionary::Empty().getCountProperty() == 0;
}

} // namespace

int main() {
    const bool ok = emptyIsStableAndEmpty() && ordinaryReadOnlyUsageIsUnaffected();
    return ok ? 0 : 1;
}
