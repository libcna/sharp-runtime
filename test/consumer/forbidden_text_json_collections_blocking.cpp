// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// RETARGETED BY #2415, AND RENAMED SO THE NAME STILL SAYS WHAT IT ASSERTS.
//
// This fixture used to include `System/Collections/Generic/List.hpp` and assert that a selective
// `Text.Json` build could not reach it. **#1889 made that boundary legitimate**: `JsonArray` and
// `JsonObject` gained fail-fast enumeration, which needs
// `System::Collections::detail::MutationCounter`, and the module-boundary validator REJECTED the
// private declaration outright -- so `modules/text-json` took `Collections.Core` as a PUBLIC
// dependency. The fixture then asserted a boundary that had deliberately been moved, and it has
// been failing since 2026-08-19 behind a green test count.
//
// IT IS RETARGETED RATHER THAN DELETED, because CLAUDE.md names it as an invariant:
// "BlockingCollection<T> belongs to Collections.Blocking; do not add its Threading requirements
// back to Collections.Core or weaken the Text.Json isolation fixture." `List.hpp` was only ever a
// PROXY for "Collections"; the proxy moves to the type that sentence is actually about.
//
// AND THE NEW PROXY IS STRICTLY STRONGER than the old one. `Collections.Blocking` declares
// `PUBLIC_DEPENDENCIES Collections.Core Core.Base Threading`, so this include can only compile if
// `Text.Json` has acquired a `Threading` requirement -- which is precisely what the surrounding
// `assert_target_absent sharp_runtime_threading` exists to prevent, now pinned by compilation
// rather than by a target name alone.
#include "System/Text/Json/JsonDocument.hpp"
#include "System/Collections/Concurrent/BlockingCollection.hpp"

int main() {
    return 0;
}
