// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Collections/IEnumerator.hpp"

namespace System::Collections::Generic
{
    /**
     * @brief Supports simple iteration over a generic collection.
     *
     * C++ counterpart of the .NET System.Collections.Generic.IEnumerator<T> interface.
     *
     * @tparam T The type of elements to enumerate.
     */
    template<typename T>
    class IEnumerator : public System::Collections::IEnumerator
    {
    public:
        ~IEnumerator() override = default;

        /// Returns the current element.
        [[nodiscard]] virtual const T& Current() const = 0;

        void* getCurrent() const override
        {
            return const_cast<T*>(&Current());
        }
    };
}
