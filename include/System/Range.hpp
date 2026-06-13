// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Index.hpp"

namespace System {

    /// <summary>Represents a range that has start and end indexes. Corresponds to C# System.Range.</summary>
    class Range {
        Index start_;
        Index end_;

    public:
        /// Constructs a Range that covers the entire collection (equivalent to Range.All()).
        Range() : start_(Index::Start()), end_(Index::End()) {}
        /// Constructs a Range with explicit start and end indexes.
        /// @param start Inclusive start index.
        /// @param end Exclusive end index.
        Range(Index start, Index end) : start_(start), end_(end) {}

        /// Returns the inclusive start index of the range.
        [[nodiscard]] const Index& getStartProperty() const noexcept { return start_; }
        /// Returns the exclusive end index of the range.
        [[nodiscard]] const Index& getEndProperty()   const noexcept { return end_; }

        /// Holds the resolved offset and length for a given collection size.
        struct OffsetAndLength {
            int Offset; ///< Zero-based start offset.
            int Length; ///< Number of elements covered.
        };

        /// Calculates the offset and length of the range relative to a collection of @p length elements.
        /// @param length Total number of elements in the collection.
        /// @return Resolved OffsetAndLength.
        /// @throws std::out_of_range if the resolved end index precedes the start index.
        [[nodiscard]] OffsetAndLength GetOffsetAndLength(int length) const {
            int start = start_.GetOffset(length);
            int end   = end_.GetOffset(length);
            if (end < start) throw std::out_of_range("Range: end index is before start index.");
            return { start, end - start };
        }

        /// Returns a Range that covers all elements (0..^0).
        static Range All()               { return Range(Index::Start(), Index::End()); }
        /// Returns a Range from @p start to the end of the collection.
        /// @param start Inclusive start index.
        static Range StartAt(Index start){ return Range(start, Index::End()); }
        /// Returns a Range from the beginning of the collection to @p end.
        /// @param end Exclusive end index.
        static Range EndAt(Index end)    { return Range(Index::Start(), end); }
    };

} // namespace System
