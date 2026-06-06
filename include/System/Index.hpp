// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <stdexcept>

namespace System {

    // Represents a type that can be used to index a collection from the start or end.
    // Corresponds to C# System.Index.
    class Index {
        int value_ = 0;  // negative means from-end: ~value is the offset
        bool fromEnd_ = false;

    public:
        Index() = default;
        explicit Index(int value, bool fromEnd = false) : value_(value), fromEnd_(fromEnd) {
            if (value < 0) throw std::out_of_range("Index: value must be non-negative.");
        }

        [[nodiscard]] bool getIsFromEndProperty() const noexcept { return fromEnd_; }
        [[nodiscard]] int getValueProperty() const noexcept { return value_; }

        [[nodiscard]] int GetOffset(int length) const {
            int offset = fromEnd_ ? length - value_ : value_;
            if (offset < 0 || offset > length) throw std::out_of_range("Index is out of range.");
            return offset;
        }

        static Index FromStart(int value) { return Index(value, false); }
        static Index FromEnd(int value)   { return Index(value, true); }

        static Index Start() { return Index(0, false); }
        static Index End()   { return Index(0, true); }
    };

} // namespace System
