// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <tuple>

namespace System {

    // Value-semantic tuples (struct layout, not shared_ptr).
    // Mirrors System.ValueTuple<T1>, System.ValueTuple<T1,T2>, etc.

    template<typename T1>
    struct ValueTuple1 {
        T1 Item1;
        explicit ValueTuple1(T1 item1) : Item1(std::move(item1)) {}
        bool operator==(const ValueTuple1& o) const { return Item1 == o.Item1; }
        bool operator!=(const ValueTuple1& o) const { return !(*this == o); }
    };

    template<typename T1, typename T2>
    struct ValueTuple2 {
        T1 Item1; T2 Item2;
        ValueTuple2(T1 i1, T2 i2) : Item1(std::move(i1)), Item2(std::move(i2)) {}
        bool operator==(const ValueTuple2& o) const { return Item1 == o.Item1 && Item2 == o.Item2; }
        bool operator!=(const ValueTuple2& o) const { return !(*this == o); }
    };

    template<typename T1, typename T2, typename T3>
    struct ValueTuple3 {
        T1 Item1; T2 Item2; T3 Item3;
        ValueTuple3(T1 i1, T2 i2, T3 i3)
            : Item1(std::move(i1)), Item2(std::move(i2)), Item3(std::move(i3)) {}
        bool operator==(const ValueTuple3& o) const {
            return Item1==o.Item1 && Item2==o.Item2 && Item3==o.Item3;
        }
        bool operator!=(const ValueTuple3& o) const { return !(*this == o); }
    };

    template<typename T1, typename T2, typename T3, typename T4>
    struct ValueTuple4 {
        T1 Item1; T2 Item2; T3 Item3; T4 Item4;
        ValueTuple4(T1 i1, T2 i2, T3 i3, T4 i4)
            : Item1(std::move(i1)), Item2(std::move(i2)), Item3(std::move(i3)), Item4(std::move(i4)) {}
        bool operator==(const ValueTuple4& o) const {
            return Item1==o.Item1 && Item2==o.Item2 && Item3==o.Item3 && Item4==o.Item4;
        }
        bool operator!=(const ValueTuple4& o) const { return !(*this == o); }
    };

    // Factory helpers
    template<typename T1>
    ValueTuple1<T1> MakeValueTuple(T1 i1) { return ValueTuple1<T1>(std::move(i1)); }

    template<typename T1, typename T2>
    ValueTuple2<T1,T2> MakeValueTuple(T1 i1, T2 i2) { return {std::move(i1),std::move(i2)}; }

    template<typename T1, typename T2, typename T3>
    ValueTuple3<T1,T2,T3> MakeValueTuple(T1 i1, T2 i2, T3 i3) { return {std::move(i1),std::move(i2),std::move(i3)}; }

    template<typename T1, typename T2, typename T3, typename T4>
    ValueTuple4<T1,T2,T3,T4> MakeValueTuple(T1 i1, T2 i2, T3 i3, T4 i4) { return {std::move(i1),std::move(i2),std::move(i3),std::move(i4)}; }

} // namespace System
