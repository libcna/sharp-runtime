// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <optional>
#include <string>
#include <utility>

#include "System/EventArgs.hpp"

namespace System::ComponentModel {

/**
 * Provides data for the PropertyChanging event.
 *
 * C++ counterpart of .NET System.ComponentModel.PropertyChangingEventArgs. A missing property
 * name represents the .NET `null` convention: every property may be about to change.
 */
class PropertyChangingEventArgs : public System::EventArgs {
    std::optional<std::string> propertyName_;

public:
    /**
     * Legacy string-form property name.
     *
     * Retained for existing sharp-runtime consumers that predate the nullable-property port.
     * It is empty when getPropertyNameProperty() has no value; new code should use that accessor
     * to distinguish an empty property name from the .NET `null` all-properties convention.
     */
    std::string PropertyName;

    /** Initializes event data with the changing property's name, or no name for all properties. */
    explicit PropertyChangingEventArgs(std::optional<std::string> propertyName)
        : propertyName_(std::move(propertyName)), PropertyName(propertyName_.value_or("")) {}

    /** Gets the name of the property that is changing, if a specific property was supplied. */
    [[nodiscard]] virtual const std::optional<std::string>& getPropertyNameProperty() const noexcept {
        return propertyName_;
    }
};

} // namespace System::ComponentModel
