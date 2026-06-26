// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <stdexcept>

namespace System {

    /**
     * @brief Represents a type that can be used to index a collection from the
     * start or from the end.
     *
     * C++ counterpart of .NET System.Index.
     */
    class Index {
        int value_ = 0;
        bool fromEnd_ = false;

    public:
        /** @brief Initializes an Index that refers to the first element of a collection. */
        Index() = default;

        /**
         * @brief Initializes an Index with the specified value, optionally from the end.
         * @param value   The index value (must be non-negative).
         * @param fromEnd If true, the index is measured from the end of the collection.
         * @throws std::out_of_range if @p value is negative.
         */
        explicit Index(int value, bool fromEnd = false) : value_(value), fromEnd_(fromEnd) {
            if (value < 0) throw std::out_of_range("Index: value must be non-negative.");
        }

        /** @brief Returns true if the index is from the end of the collection. */
        [[nodiscard]] bool getIsFromEndProperty() const noexcept { return fromEnd_; }

        /** @brief Returns the index value. */
        [[nodiscard]] int getValueProperty() const noexcept { return value_; }

        /**
         * @brief Returns the offset for this index given a collection of the specified length.
         * @param length The length of the collection.
         * @return The zero-based offset into the collection.
         * @throws std::out_of_range if the resulting offset is out of bounds.
         */
        [[nodiscard]] int GetOffset(int length) const {
            int offset = fromEnd_ ? length - value_ : value_;
            if (offset < 0 || offset > length) throw std::out_of_range("Index is out of range.");
            return offset;
        }

        /** @brief Creates an Index from the start of a collection at the specified value. */
        static Index FromStart(int value) { return Index(value, false); }

        /** @brief Creates an Index from the end of a collection at the specified value. */
        static Index FromEnd(int value)   { return Index(value, true); }

        /** @brief Returns an Index that points to the first element (index 0). */
        static Index Start() { return Index(0, false); }

        /** @brief Returns an Index that points past the last element (from end, offset 0). */
        static Index End()   { return Index(0, true); }
    };

} // namespace System
