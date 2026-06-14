// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Tuple.hpp"

namespace System {

    /**
     * @brief Provides extension-like free functions for Tuple instances to interop with
     * std::tuple (C++ equivalent of ValueTuple) and to support deconstruction.
     *
     * C++ counterpart of .NET System.TupleExtensions.
     */
    struct TupleExtensions {
        TupleExtensions() = delete;

        // --- ToValueTuple (Tuple → std::tuple) ---

        /** @brief Converts a Tuple2 to an equivalent std::tuple (ValueTuple equivalent). */
        template<typename T1, typename T2>
        [[nodiscard]] static std::tuple<T1, T2> ToValueTuple(const Tuple2<T1, T2>& t) {
            return t.ToStdTuple();
        }

        /** @brief Converts a Tuple3 to an equivalent std::tuple. */
        template<typename T1, typename T2, typename T3>
        [[nodiscard]] static std::tuple<T1, T2, T3> ToValueTuple(const Tuple3<T1, T2, T3>& t) {
            return t.ToStdTuple();
        }

        /** @brief Converts a Tuple4 to an equivalent std::tuple. */
        template<typename T1, typename T2, typename T3, typename T4>
        [[nodiscard]] static std::tuple<T1, T2, T3, T4> ToValueTuple(const Tuple4<T1, T2, T3, T4>& t) {
            return t.ToStdTuple();
        }

        // --- ToTuple (std::tuple → Tuple) ---

        /** @brief Converts a std::tuple to an equivalent Tuple2. */
        template<typename T1, typename T2>
        [[nodiscard]] static Tuple2<T1, T2> ToTuple(const std::tuple<T1, T2>& t) {
            return {std::get<0>(t), std::get<1>(t)};
        }

        /** @brief Converts a std::tuple to an equivalent Tuple3. */
        template<typename T1, typename T2, typename T3>
        [[nodiscard]] static Tuple3<T1, T2, T3> ToTuple(const std::tuple<T1, T2, T3>& t) {
            return {std::get<0>(t), std::get<1>(t), std::get<2>(t)};
        }

        /** @brief Converts a std::tuple to an equivalent Tuple4. */
        template<typename T1, typename T2, typename T3, typename T4>
        [[nodiscard]] static Tuple4<T1, T2, T3, T4> ToTuple(const std::tuple<T1, T2, T3, T4>& t) {
            return {std::get<0>(t), std::get<1>(t), std::get<2>(t), std::get<3>(t)};
        }
    };

} // namespace System
