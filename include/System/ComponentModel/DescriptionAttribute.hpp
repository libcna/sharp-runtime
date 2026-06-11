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


} // namespace System::ComponentModel
