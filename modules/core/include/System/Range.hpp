// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Index.hpp"
#include <cstdint>

#include "HashCode.hpp"

namespace System {

    /**
     * @brief Represents a range that has start and end indexes.
     *
     * C++ counterpart of .NET System.Range. Used with Index to express slices of
     * collections. The range [start, end) is half-open: start is inclusive, end is
     * exclusive.
     */
    struct Range {
    private:
        Index start_;
        Index end_;

    public:
        /**
         * @brief Constructs a Range that covers the entire collection (equivalent to Range.All).
         */
        constexpr Range() : start_(Index::Start()), end_(Index::End()) {}

        /**
         * @brief Constructs a Range with explicit start and end indexes.
         * @param start Inclusive start index.
         * @param end   Exclusive end index.
         */
        constexpr Range(Index start, Index end) : start_(start), end_(end) {}

        /** @brief Returns the inclusive start index of the range. */
        [[nodiscard]] constexpr const Index& getStartProperty() const noexcept { return start_; }

        /** @brief Returns the exclusive end index of the range. */
        [[nodiscard]] constexpr const Index& getEndProperty() const noexcept { return end_; }

        /**
         * @brief Returns a Range that covers all elements of a collection (0..^0).
         *
         * C++ counterpart of .NET Range.All.
         */
        [[nodiscard]] static constexpr Range getAllProperty() { return Range(Index::Start(), Index::End()); }

        /**
         * @brief Returns a Range that starts at the specified index and ends at the end of the collection.
         * @param start Inclusive start index.
         */
        [[nodiscard]] static constexpr Range StartAt(Index start) { return Range(start, Index::End()); }

        /**
         * @brief Returns a Range from the beginning of the collection to the specified end index.
         * @param end Exclusive end index.
         */
        [[nodiscard]] static constexpr Range EndAt(Index end) { return Range(Index::Start(), end); }

        /** @brief Holds the resolved offset and length for a given collection size. */
        struct OffsetAndLength {
            /** @brief Zero-based start offset. */
            SharpRuntime::intcs Offset;
            /** @brief Number of elements covered. */
            SharpRuntime::intcs Length;
        };

        /**
         * @brief Calculates the start offset and length of the range relative to a collection of the given size.
         *
         * C++ counterpart of .NET Range.GetOffsetAndLength(int). Unlike Index.GetOffset, this
         * method does validate: it throws if the resolved end index exceeds @p length or if the
         * resolved start index exceeds the resolved end index (matching .NET's unsigned-compare
         * checks, which also reject negative offsets).
         * The resolved-length subtraction is performed in `SharpRuntime::uintcs` and
         * converted back (ticket #1830, CCF-004 class A). This site is **its own** defect,
         * not merely one inherited from `Index::GetOffset`: the unsigned checks above pass
         * for a maximal from-end range over an `INTCS_MIN` length, and the `end - start`
         * that follows was then UBSan-confirmed undefined behaviour at this line
         * (`signed integer overflow: -2147483648 - 1`, measured alongside `Index.hpp`'s
         * report in the same process). Neither the audit's SR-AUD-057 text nor
         * `docs/DefinedArithmeticBoundaryPlan.md` §2 listed it; both described `Range` as
         * only *consuming* `Index`'s operation. The resolved values are unchanged.
         *
         * @param length Total number of elements in the collection.
         * @return Resolved OffsetAndLength value.
         * @throws System::ArgumentOutOfRangeException if the resolved range falls outside [0, length].
         */
        [[nodiscard]] constexpr OffsetAndLength GetOffsetAndLength(SharpRuntime::intcs length) const {
            using SharpRuntime::uintcs;
            using SharpRuntime::intcs;
            intcs start = start_.GetOffset(length);
            intcs end   = end_.GetOffset(length);
            if (static_cast<uintcs>(end) > static_cast<uintcs>(length) ||
                static_cast<uintcs>(start) > static_cast<uintcs>(end)) {
                throw ArgumentOutOfRangeException("length");
            }
            return { start, static_cast<intcs>(
                                static_cast<uintcs>(end)
                                - static_cast<uintcs>(start)) };
        }

        /**
         * @brief Returns true if this range equals the specified other range.
         *
         * C++ counterpart of .NET Range.Equals(Range).
         * @param other The range to compare.
         */
        [[nodiscard]] constexpr bool Equals(const Range& other) const noexcept {
            return *this == other;
        }

        /**
         * @brief Returns a hash code for this range.
         *
         * C++ counterpart of .NET Range.GetHashCode().
         */
        [[nodiscard]] SharpRuntime::intcs GetHashCode() const {
            return HashCode::Combine(start_.GetHashCode(), end_.GetHashCode());
        }

        /**
         * @brief Returns a string representation of this range in C# range expression syntax.
         *
         * C++ counterpart of .NET Range.ToString(). Examples: "0..^0", "1..5", "^2..^0".
         */
        [[nodiscard]] std::string ToString() const {
            return start_.ToString() + ".." + end_.ToString();
        }

        /** @brief Returns true if the two ranges are equal. */
        friend constexpr bool operator==(const Range&, const Range&) noexcept = default;
    };

} // namespace System
