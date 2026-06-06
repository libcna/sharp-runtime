// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::ComponentModel {

    /**
     * @brief Specifies a description for a property or event.
     *
     * Metadata-only stub. Partial C++ counterpart of .NET System.ComponentModel.DescriptionAttribute.
     *
     * @note Status: Stub — stores text only; no reflection integration.
     */
    struct DescriptionAttribute {
        std::string Description;
        explicit DescriptionAttribute(const std::string& description) : Description(description) {}
    };

    /**
     * @brief Specifies the default value for a property.
     *
     * Metadata-only stub. Partial C++ counterpart of .NET System.ComponentModel.DefaultValueAttribute.
     *
     * @note Status: Stub — stores value as string only; no reflection integration.
     */
    struct DefaultValueAttribute {
        std::string Value;
        explicit DefaultValueAttribute(const std::string& value) : Value(value) {}
        explicit DefaultValueAttribute(int value)    : Value(std::to_string(value)) {}
        explicit DefaultValueAttribute(double value) : Value(std::to_string(value)) {}
        explicit DefaultValueAttribute(bool value)   : Value(value ? "true" : "false") {}
    };

} // namespace System::ComponentModel
