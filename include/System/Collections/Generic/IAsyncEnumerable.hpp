// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include "System/Threading/CancellationToken.hpp"

namespace System::Collections::Generic {

    template<typename T>
    class IAsyncEnumerator;

    template<typename T>
    class IAsyncEnumerable {
    public:
        virtual ~IAsyncEnumerable() = default;
        // In C++ we approximate async enumeration synchronously.
        [[nodiscard]] virtual std::shared_ptr<IAsyncEnumerator<T>> GetAsyncEnumerator(
            System::Threading::CancellationToken cancellationToken = {}) = 0;
    };

    template<typename T>
    class IAsyncEnumerator {
    public:
        virtual ~IAsyncEnumerator() = default;
        // MoveNextAsync() returns true if another element is available.
        virtual bool MoveNextAsync() = 0;
        [[nodiscard]] virtual const T& getCurrent() const = 0;
        virtual void DisposeAsync() {}
    };

} // namespace System::Collections::Generic
