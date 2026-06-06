// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Index.hpp"

namespace System {

    // Represents a range that has start and end indexes.
    // Corresponds to C# System.Range.
    class Range {
        Index start_;
        Index end_;

    public:
        Range() : start_(Index::Start()), end_(Index::End()) {}
        Range(Index start, Index end) : start_(start), end_(end) {}

        [[nodiscard]] const Index& getStartProperty() const noexcept { return start_; }
        [[nodiscard]] const Index& getEndProperty()   const noexcept { return end_; }

        struct OffsetAndLength {
            int Offset;
            int Length;
        };

        [[nodiscard]] OffsetAndLength GetOffsetAndLength(int length) const {
            int start = start_.GetOffset(length);
            int end   = end_.GetOffset(length);
            if (end < start) throw std::out_of_range("Range: end index is before start index.");
            return { start, end - start };
        }

        static Range All()               { return Range(Index::Start(), Index::End()); }
        static Range StartAt(Index start){ return Range(start, Index::End()); }
        static Range EndAt(Index end)    { return Range(Index::Start(), end); }
    };

} // namespace System
