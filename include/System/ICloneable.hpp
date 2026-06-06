// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>

namespace System {

    /**
     * @brief Supports cloning, which creates a new instance of a class with the same value as an existing instance.
     *
     * Partial C++ counterpart of .NET System.ICloneable.
     *
     * @note Status: Stub
     */
    class ICloneable {
    public:
        virtual ~ICloneable() = default;
        [[nodiscard]] virtual std::shared_ptr<ICloneable> Clone() const = 0;
    };

} // namespace System
