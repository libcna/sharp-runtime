// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>

namespace System {

    /**
     * @brief Contains methods to create types of objects locally,
     * or obtain references to existing objects.
     *
     * Partial C++ stub of .NET System.Activator.
     * Only default-construction is supported; reflection-based overloads are not available.
     */
    class Activator {
    public:
        /**
         * @brief Creates an instance of type @p T using its default constructor.
         *
         * C++ counterpart of .NET Activator.CreateInstance<T>().
         * @tparam T Type to instantiate. Must be default-constructible.
         * @return New value-constructed instance of @p T.
         */
        template<typename T>
        [[nodiscard]] static T CreateInstance() {
            return T{};
        }

        /**
         * @brief Creates a heap-allocated instance of type @p T using its default constructor.
         *
         * @tparam T Type to instantiate. Must be default-constructible.
         * @return unique_ptr owning the new instance.
         */
        template<typename T>
        [[nodiscard]] static std::unique_ptr<T> CreateInstancePtr() {
            return std::make_unique<T>();
        }
    };

} // namespace System
