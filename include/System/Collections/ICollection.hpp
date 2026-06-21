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
    /** @brief Virtual destructor for safe polymorphic destruction. */
    virtual ~ICollection() = default;

    /**
     * @brief Gets the number of elements contained in the collection.
     *
     * C++ counterpart of .NET ICollection.Count.
     */
    [[nodiscard]] virtual int getCountProperty() const = 0;

    /**
     * @brief Gets an object that can be used to synchronize access to the collection.
     *
     * C++ counterpart of .NET ICollection.SyncRoot.
     * @return A pointer that can be used as a synchronization lock.
     */
    [[nodiscard]] virtual const void* getSyncRootProperty() const { return this; }

    /**
     * @brief Gets a value indicating whether access to the collection is synchronized (thread-safe).
     *
     * C++ counterpart of .NET ICollection.IsSynchronized.
     */
    [[nodiscard]] virtual bool getIsSynchronizedProperty() const { return false; }
};

} // namespace System::Collections
