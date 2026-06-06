// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <functional>

namespace System {

    /**
     * @brief Encapsulates a method that has no parameters and returns a value
     * of type R.
     *
     * C++ counterpart of the .NET System.Func<TResult> delegate.
     */
    template<typename R>
    using Func = std::function<R()>;

    /**
     * @brief Encapsulates a method that has one parameter and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T, TResult> delegate.
     */
    template<typename T, typename R>
    using FuncT = std::function<R(T)>;

    /**
     * @brief Encapsulates a method that has two parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1, T2, TResult> delegate.
     */
    template<typename T1, typename T2, typename R>
    using FuncT2 = std::function<R(T1, T2)>;

    /**
     * @brief Encapsulates a method that has three parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1, T2, T3, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, typename R>
    using FuncT3 = std::function<R(T1, T2, T3)>;

} // namespace System
