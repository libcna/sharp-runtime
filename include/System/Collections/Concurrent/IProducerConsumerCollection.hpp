// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/Generic/IEnumerable.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Concurrent {

    /** Defines methods to manipulate thread-safe collections intended for producer/consumer usage. */
    template<typename T>
    class IProducerConsumerCollection : public System::Collections::Generic::IEnumerable<T> {
    public:
        /** Destroys the collection. */
        virtual ~IProducerConsumerCollection() = default;
        /** Attempts to add an object to the collection; returns true if successful. */
        virtual bool TryAdd(const T& item) = 0;
        /** Attempts to remove and return an object from the collection; returns true if successful. */
        virtual bool TryTake(T& item) = 0;
        /** Gets the number of elements contained in the collection. */
        [[nodiscard]] virtual SharpRuntime::intcs getCountProperty() const = 0;
        /** Returns true if the collection contains no elements. */
        [[nodiscard]] virtual bool getIsEmptyProperty() const = 0;
    };

} // namespace System::Collections::Concurrent
