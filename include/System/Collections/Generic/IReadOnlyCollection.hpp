// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Collections/Generic/IEnumerable.hpp"

namespace System::Collections::Generic {

    /**
     * @brief Represents a strongly-typed, read-only collection of elements.
     *
     * @tparam T The type of the elements.
     *
     * @note Status: Implemented
     */
    template<typename T>
    class IReadOnlyCollection : public IEnumerable<T> {
    public:
        /** Destroys the collection. */
        virtual ~IReadOnlyCollection() = default;

        /** Gets the number of elements in the collection. */
        [[nodiscard]] virtual int getCountProperty() const = 0;
    };

} // namespace System::Collections::Generic
