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
 *
 * @note #2405 REMOVED a second, public `std::string PropertyName` member that this type used to
 * carry beside `propertyName_`. Its own doc-comment described the defect: it was "empty when
 * getPropertyNameProperty() has no value", i.e. it COLLAPSED `std::nullopt` and `""` into one
 * state -- the #2295 defect, where an absent value and an empty one become indistinguishable. It
 * was also **mutable**, where .NET's is `public virtual string? PropertyName { get; }`
 * (PropertyChangingEventArgs.cs), so a subscriber could retarget the args object mid-dispatch and every later
 * subscriber would see the changed value -- and after such a write the field and the accessor
 * DISAGREED, so the object contradicted itself.
 *
 * It was kept "for existing sharp-runtime consumers that predate the nullable-property port". That
 * reason was measured and is empty: **zero** sites in `cna` and **zero** in `mobile-eggbert`, and
 * one first-party read, in this repository's own tests.
 */
class PropertyChangingEventArgs : public System::EventArgs {
    std::optional<std::string> propertyName_;

public:
    /** Initializes event data with the changing property's name, or no name for all properties. */
    explicit PropertyChangingEventArgs(std::optional<std::string> propertyName)
        : propertyName_(std::move(propertyName)) {}

    /** Gets the name of the property that is changing, if a specific property was supplied. */
    [[nodiscard]] virtual const std::optional<std::string>& getPropertyNameProperty() const noexcept {
        return propertyName_;
    }
};

} // namespace System::ComponentModel
