// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
#include "System/Collections/Concurrent/BlockingCollection.hpp"

int main() {
    System::Collections::Concurrent::BlockingCollection<int> collection;
    collection.Add(42);
    return collection.Take() == 42 ? 0 : 1;
}
