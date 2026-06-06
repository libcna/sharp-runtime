// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Collections/Generic/IReadOnlyCollection.hpp"

namespace System::Collections::Generic {

    /**
     * @brief Represents a read-only collection of elements that can be
     * accessed by index.
     *
     * @tparam T The type of elements in the read-only list.
     *
     * @note Status: Implemented
     */
    template<typename T>
    class IReadOnlyList : public IReadOnlyCollection<T> {
    public:
        virtual ~IReadOnlyList() = default;

        /**
         * @brief Gets the element at the specified index.
         *
         * @param index Zero-based index of the element to get.
         */
        [[nodiscard]] virtual const T& operator[](int index) const = 0;
    };

} // namespace System::Collections::Generic
