// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Attribute.hpp"
#include "System/LoaderOptimization.hpp"

namespace System {

    /**
     * @brief Allows developers to specify the particular optimization that
     * the loader uses when loading an assembly for an application.
     *
     * C++ counterpart of .NET System.LoaderOptimizationAttribute.
     */
    class LoaderOptimizationAttribute final : public Attribute {
    public:
        /** @brief Initializes a new instance with the specified optimization value. */
        explicit LoaderOptimizationAttribute(LoaderOptimization value) : value_(value) {}

        /** @brief Gets the loader optimization value. */
        [[nodiscard]] LoaderOptimization getValueProperty() const { return value_; }

    private:
        LoaderOptimization value_;
    };

} // namespace System
