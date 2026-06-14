// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <tuple>

namespace System {

    /// @brief Thin wrappers matching .NET Tuple<> naming convention.
    ///
    /// Delegate to std::tuple. The Item1/Item2/... fields follow .NET naming.
    ///
    /// @note Status: Implemented (2-, 3-, and 4-element variants)
    template<typename T1, typename T2>
    struct Tuple2 {
        T1 Item1; ///< First element.
        T2 Item2; ///< Second element.
        /// Constructs a Tuple2 from two values.
        Tuple2(T1 i1, T2 i2) : Item1(std::move(i1)), Item2(std::move(i2)) {}
        /// Deconstructs this tuple into its elements.
        void Deconstruct(T1& item1, T2& item2) const { item1 = Item1; item2 = Item2; }
        /// Converts this tuple to an equivalent std::tuple.
        [[nodiscard]] std::tuple<T1, T2> ToStdTuple() const { return {Item1, Item2}; }
        /// Returns true if all elements compare equal.
        bool operator==(const Tuple2& o) const { return Item1 == o.Item1 && Item2 == o.Item2; }
        /// Returns true if any element differs.
        bool operator!=(const Tuple2& o) const { return !(*this == o); }
    };

    template<typename T1, typename T2, typename T3>
    struct Tuple3 {
        T1 Item1; ///< First element.
        T2 Item2; ///< Second element.
        T3 Item3; ///< Third element.
        /// Constructs a Tuple3 from three values.
        Tuple3(T1 i1, T2 i2, T3 i3) : Item1(std::move(i1)), Item2(std::move(i2)), Item3(std::move(i3)) {}
        /// Deconstructs this tuple into its elements.
        void Deconstruct(T1& item1, T2& item2, T3& item3) const { item1 = Item1; item2 = Item2; item3 = Item3; }
        /// Converts this tuple to an equivalent std::tuple.
        [[nodiscard]] std::tuple<T1, T2, T3> ToStdTuple() const { return {Item1, Item2, Item3}; }
        /// Returns true if all elements compare equal.
        bool operator==(const Tuple3& o) const { return Item1 == o.Item1 && Item2 == o.Item2 && Item3 == o.Item3; }
        /// Returns true if any element differs.
        bool operator!=(const Tuple3& o) const { return !(*this == o); }
    };

    template<typename T1, typename T2, typename T3, typename T4>
    struct Tuple4 {
        T1 Item1; ///< First element.
        T2 Item2; ///< Second element.
        T3 Item3; ///< Third element.
        T4 Item4; ///< Fourth element.
        /// Constructs a Tuple4 from four values.
        Tuple4(T1 i1, T2 i2, T3 i3, T4 i4) : Item1(std::move(i1)), Item2(std::move(i2)), Item3(std::move(i3)), Item4(std::move(i4)) {}
        /// Deconstructs this tuple into its elements.
        void Deconstruct(T1& item1, T2& item2, T3& item3, T4& item4) const { item1 = Item1; item2 = Item2; item3 = Item3; item4 = Item4; }
        /// Converts this tuple to an equivalent std::tuple.
        [[nodiscard]] std::tuple<T1, T2, T3, T4> ToStdTuple() const { return {Item1, Item2, Item3, Item4}; }
    };

} // namespace System
