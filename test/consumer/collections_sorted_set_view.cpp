// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Standalone public-header consumer fixture for the live SortedSet<T> bounded-view contract
// (ticket #1783, SR-AUD-361). It compiles against only the public Collections.Core surface, so
// it fails if the shared-state representation starts to require a private header, another
// component, or a non-public helper. It follows the collections_linked_list.cpp precedent from
// ticket #1769.
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Generic/SortedSet.hpp"

using System::Collections::Generic::SortedSet;

// A non-const call still returns SortedSet<T> BY VALUE -- the return type is unchanged by
// ticket #1783; only the const qualifier and the object's semantics changed. The companion
// negative fixture collections_sorted_set_view_negative.cpp proves the const call is rejected.
static_assert(
    std::is_same_v<
        decltype(std::declval<SortedSet<int>&>().GetViewBetween(std::declval<const int&>(),
                                                               std::declval<const int&>())),
        SortedSet<int>>,
    "GetViewBetween must return SortedSet<T> by value");

namespace {

// A view is a bidirectional write-through window onto the set it came from.
bool viewPropagatesInBothDirections() {
    SortedSet<int> set{1, 2, 3, 4, 6, 7, 8, 9, 10};
    SortedSet<int> view = set.GetViewBetween(3, 7);

    if (!view.getIsViewProperty() || set.getIsViewProperty()) return false;
    if (view.getCountProperty() != 4) return false;

    if (!set.Add(5)) return false;                    // parent -> view
    if (!view.Contains(5) || view.getCountProperty() != 5) return false;

    if (!view.Remove(4)) return false;                // view -> parent
    if (set.Contains(4)) return false;

    return view.getMinProperty() == 3 && view.getMaxProperty() == 7;
}

// Bounds are inclusive and enforced for the whole life of the view.
bool viewEnforcesItsBounds() {
    SortedSet<int> set{1, 2, 3, 4, 5, 6, 7, 8};
    SortedSet<int> view = set.GetViewBetween(3, 6);

    if (!view.IsWithinRange(3) || !view.IsWithinRange(6)) return false;
    if (view.IsWithinRange(2) || view.IsWithinRange(7)) return false;
    if (view.Contains(2) || view.Contains(7)) return false;
    if (view.Remove(8)) return false;                 // out of range: false, no throw

    bool threw = false;
    try {
        view.Add(99);
    } catch (const System::ArgumentOutOfRangeException&) {
        threw = true;
    }
    if (!threw || set.Contains(99)) return false;

    view.Clear();                                     // removes exactly [3, 6]
    return set.ToVector() == std::vector<int>{1, 2, 7, 8};
}

// A view keeps the shared state alive, so it stays valid after its origin is destroyed.
bool viewOutlivesItsOrigin() {
    SortedSet<std::string> view;
    {
        SortedSet<std::string> set{std::string("alpha"), std::string("bravo"),
                                   std::string("charlie"), std::string("delta")};
        view = set.GetViewBetween(std::string("bravo"), std::string("charlie"));
    }
    if (view.getCountProperty() != 2) return false;
    if (!view.Remove(std::string("bravo"))) return false;

    std::vector<std::string> seen;
    for (const std::string& value : view) seen.push_back(value);
    return seen == std::vector<std::string>{"charlie"};
}

// A nested view may only narrow, and is flattened onto the same root state.
bool nestedViewNarrowsOnly() {
    SortedSet<int> set{1, 2, 3, 4, 5, 6, 7, 8, 9};
    SortedSet<int> outer = set.GetViewBetween(3, 7);
    SortedSet<int> inner = outer.GetViewBetween(4, 6);

    if (inner.getCountProperty() != 3) return false;
    if (!inner.Remove(5) || set.Contains(5)) return false;

    bool threw = false;
    try {
        (void)outer.GetViewBetween(1, 6);
    } catch (const System::ArgumentOutOfRangeException&) {
        threw = true;
    }
    return threw;
}

// ToSortedSet() is the supported way to materialize an independent range.
bool explicitSnapshotIsDetached() {
    SortedSet<int> set{1, 2, 3, 4, 5};
    SortedSet<int> snapshot = set.GetViewBetween(2, 4).ToSortedSet();

    if (snapshot.getIsViewProperty()) return false;
    if (snapshot.ToVector() != std::vector<int>{2, 3, 4}) return false;

    if (!snapshot.Add(99)) return false;              // no bounds on a detached set
    if (set.Contains(99)) return false;
    if (!set.Remove(3) || !snapshot.Contains(3)) return false;

    // Copying an owning set stays an independent deep clone; copying a view stays a handle.
    SortedSet<int> clone = set;
    SortedSet<int> view = set.GetViewBetween(1, 2);
    SortedSet<int> viewHandle = view;
    if (!clone.Add(1000) || set.Contains(1000)) return false;
    if (!viewHandle.Remove(1) || set.Contains(1)) return false;
    return true;
}

} // namespace

int main() {
    return viewPropagatesInBothDirections()
        && viewEnforcesItsBounds()
        && viewOutlivesItsOrigin()
        && nestedViewNarrowsOnly()
        && explicitSnapshotIsDetached() ? 0 : 1;
}
