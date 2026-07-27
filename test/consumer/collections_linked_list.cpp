// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Standalone public-header consumer fixture for the LinkedListNode<T> lifetime
// contract (ticket #1769, SR-AUD-357). It compiles against only the public
// Collections.Core surface, so it fails if the node representation starts to
// require a private header, another component, or a non-public helper.
#include <string>
#include <vector>

#include "System/Collections/Generic/LinkedList.hpp"

using System::Collections::Generic::LinkedList;
using System::Collections::Generic::LinkedListNode;

namespace {

// A detached node keeps its value after the owning list is destroyed.
bool detachedNodeSurvivesItsOwner() {
    LinkedListNode<std::string> retained;
    {
        LinkedList<std::string> owner;
        owner.AddLast(std::string("first"));
        retained = owner.getFirstProperty();
        if (retained.getListProperty() != &owner) return false;
    }
    return static_cast<bool>(retained)
        && retained.getListProperty() == nullptr
        && retained.getValueProperty() == "first"
        && !static_cast<bool>(retained.getNextProperty())
        && !static_cast<bool>(retained.getPreviousProperty());
}

// An independently constructed node can be attached, removed, and reattached.
bool detachedNodeRoundTrips() {
    LinkedList<int> list;
    LinkedListNode<int> node(7);
    if (node.getListProperty() != nullptr) return false;

    list.AddLast(node);
    if (node.getListProperty() != &list) return false;

    list.Remove(node);
    if (node.getListProperty() != nullptr || node.getValueProperty() != 7) return false;

    list.AddFirst(node);
    return list.getFirstProperty() == node && list.getCountProperty() == 1;
}

// Public STL iteration still works over the migrated iterator type.
bool iterationRoundTrips() {
    LinkedList<int> list;
    list.AddLast(1);
    LinkedListNode<int> second = list.AddLast(2);
    list.AddAfter(second, LinkedListNode<int>(3));

    std::vector<int> collected;
    for (int value : list) collected.push_back(value);

    const LinkedList<int>& constList = list;
    for (LinkedList<int>::const_iterator it = constList.begin(); it != constList.end(); ++it) {
        collected.push_back(*it);
    }

    LinkedList<int>::iterator last = list.end();
    --last;
    return collected.size() == 6u && collected[0] == 1 && collected[5] == 3 && *last == 3;
}

} // namespace

int main() {
    return detachedNodeSurvivesItsOwner()
        && detachedNodeRoundTrips()
        && iterationRoundTrips() ? 0 : 1;
}
