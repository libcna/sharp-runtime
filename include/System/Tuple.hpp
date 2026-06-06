// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <tuple>

namespace System {

    /**
     * @brief Thin wrappers matching .NET Tuple<> naming convention.
     *
     * Delegate to std::tuple. The Item1/Item2/... fields follow .NET naming.
     *
     * @note Status: Implemented (2- and 3-element variants)
     */
    template<typename T1, typename T2>
    struct Tuple2 {
        T1 Item1;
        T2 Item2;
        Tuple2(T1 i1, T2 i2) : Item1(std::move(i1)), Item2(std::move(i2)) {}
        bool operator==(const Tuple2& o) const { return Item1 == o.Item1 && Item2 == o.Item2; }
        bool operator!=(const Tuple2& o) const { return !(*this == o); }
    };

    template<typename T1, typename T2, typename T3>
    struct Tuple3 {
        T1 Item1;
        T2 Item2;
        T3 Item3;
        Tuple3(T1 i1, T2 i2, T3 i3) : Item1(std::move(i1)), Item2(std::move(i2)), Item3(std::move(i3)) {}
        bool operator==(const Tuple3& o) const { return Item1 == o.Item1 && Item2 == o.Item2 && Item3 == o.Item3; }
        bool operator!=(const Tuple3& o) const { return !(*this == o); }
    };

    template<typename T1, typename T2, typename T3, typename T4>
    struct Tuple4 {
        T1 Item1; T2 Item2; T3 Item3; T4 Item4;
        Tuple4(T1 i1, T2 i2, T3 i3, T4 i4) : Item1(std::move(i1)), Item2(std::move(i2)), Item3(std::move(i3)), Item4(std::move(i4)) {}
    };

} // namespace System
