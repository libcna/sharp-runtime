// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <vector>
#include "System/IDisposable.hpp"

namespace System::Buffers {

    template<typename T>
    class IMemoryOwner : public System::IDisposable {
    public:
        virtual ~IMemoryOwner() = default;
        virtual std::vector<T>& getMemoryProperty() = 0;
    };

} // namespace System::Buffers
