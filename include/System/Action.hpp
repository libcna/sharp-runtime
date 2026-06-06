#pragma once

#include <functional>

namespace System
{
    /**
     * @brief Encapsulates a method that has no parameters and does not return a value.
     *
     * C++ counterpart of the .NET System.Action delegate.
     */
    using Action = std::function<void()>;

    /**
     * @brief Encapsulates a method that has one parameter and does not return a value.
     *
     * C++ counterpart of the .NET System.Action<T> delegate.
     *
     * @tparam T The type of the parameter.
     */
    template<typename T>
    using ActionT = std::function<void(T)>;

    /**
     * @brief Encapsulates a method that has two parameters and does not return a value.
     *
     * C++ counterpart of the .NET System.Action<T1, T2> delegate.
     */
    template<typename T1, typename T2>
    using ActionT2 = std::function<void(T1, T2)>;

    /**
     * @brief Encapsulates a method that has three parameters and does not return a value.
     *
     * C++ counterpart of the .NET System.Action<T1, T2, T3> delegate.
     */
    template<typename T1, typename T2, typename T3>
    using ActionT3 = std::function<void(T1, T2, T3)>;
}
