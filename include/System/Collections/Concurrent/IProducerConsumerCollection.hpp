// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/Generic/IEnumerable.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Concurrent {

    template<typename T>
    class IProducerConsumerCollection : public System::Collections::Generic::IEnumerable<T> {
    public:
        virtual ~IProducerConsumerCollection() = default;
        virtual bool TryAdd(const T& item) = 0;
        virtual bool TryTake(T& item) = 0;
        [[nodiscard]] virtual SharpRuntime::intcs getCountProperty() const = 0;
        [[nodiscard]] virtual bool getIsEmptyProperty() const = 0;
    };

} // namespace System::Collections::Concurrent
