// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/IEnumerable.hpp"

namespace System::Collections {

    /**
     * @brief Defines size, enumerators, and synchronization methods for all non-generic collections.
     *
     * Partial C++ counterpart of .NET System.Collections.ICollection.
     *
     * @note Status: Stub
     */
    class ICollection : public IEnumerable {
    public:
        /// Destroys the collection.
        virtual ~ICollection() = default;
        /// Gets the number of elements contained in the collection.
        [[nodiscard]] virtual int getCountProperty() const = 0;
        /// Returns true if access to the collection is synchronized (thread-safe).
        [[nodiscard]] virtual bool getIsSynchronizedProperty() const { return false; }
    };

} // namespace System::Collections
