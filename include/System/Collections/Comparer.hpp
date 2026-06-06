// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstring>
#include "System/Collections/IComparer.hpp"

namespace System::Collections {

    // Non-generic Comparer using strcmp for strings, numeric comparison for pointers treated as intptr.
    class Comparer : public IComparer {
    public:
        [[nodiscard]] int Compare(const void* x, const void* y) const override {
            if (x == y) return 0;
            if (!x) return -1;
            if (!y) return  1;
            // Compare as pointer values (approximate — callers should use typed comparers).
            return x < y ? -1 : 1;
        }

        static const Comparer& Default() {
            static Comparer instance;
            return instance;
        }
    };

} // namespace System::Collections
