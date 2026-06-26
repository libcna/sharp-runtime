// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include "System/Threading/CancellationToken.hpp"

namespace System::Collections::Generic {

    template<typename T>
    class IAsyncEnumerator;

    /** Exposes an asynchronous enumerator that provides asynchronous iteration over a sequence of values. */
    template<typename T>
    class IAsyncEnumerable {
    public:
        /** Destroys the enumerable. */
        virtual ~IAsyncEnumerable() = default;
        /** Returns an async enumerator that iterates through the collection (approximated synchronously in C++). */
        [[nodiscard]] virtual std::shared_ptr<IAsyncEnumerator<T>> GetAsyncEnumerator(
            System::Threading::CancellationToken cancellationToken = {}) = 0;
    };

    /** Supports asynchronous iteration over a sequence of values. */
    template<typename T>
    class IAsyncEnumerator {
    public:
        /** Destroys the enumerator. */
        virtual ~IAsyncEnumerator() = default;
        /** Advances the enumerator asynchronously to the next element; returns true if another element is available. */
        virtual bool MoveNextAsync() = 0;
        /** Gets the element at the current position of the enumerator. */
        [[nodiscard]] virtual const T& getCurrent() const = 0;
        /** Performs application-defined tasks associated with freeing resources asynchronously. */
        virtual void DisposeAsync() {}
    };

} // namespace System::Collections::Generic
