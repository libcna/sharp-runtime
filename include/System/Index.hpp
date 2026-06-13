// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <stdexcept>

namespace System {

    /// Represents a type that can be used to index a collection from the start or the end.
    class Index {
        int value_ = 0;  // negative means from-end: ~value is the offset
        bool fromEnd_ = false;

    public:
        /// Initializes an Index that refers to the first element of a collection.
        Index() = default;
        /// Initializes an Index with the specified value, optionally from the end.
        explicit Index(int value, bool fromEnd = false) : value_(value), fromEnd_(fromEnd) {
            if (value < 0) throw std::out_of_range("Index: value must be non-negative.");
        }

        /// Returns true if the index is from the end of the collection.
        [[nodiscard]] bool getIsFromEndProperty() const noexcept { return fromEnd_; }
        /// Returns the index value.
        [[nodiscard]] int getValueProperty() const noexcept { return value_; }

        /// Returns the offset for this index given a collection of the specified length.
        [[nodiscard]] int GetOffset(int length) const {
            int offset = fromEnd_ ? length - value_ : value_;
            if (offset < 0 || offset > length) throw std::out_of_range("Index is out of range.");
            return offset;
        }

        /// Creates an Index from the start of a collection at the specified value.
        static Index FromStart(int value) { return Index(value, false); }
        /// Creates an Index from the end of a collection at the specified value.
        static Index FromEnd(int value)   { return Index(value, true); }

        /// Returns an Index that points to the first element (index 0).
        static Index Start() { return Index(0, false); }
        /// Returns an Index that points past the last element (from end, offset 0).
        static Index End()   { return Index(0, true); }
    };

} // namespace System
