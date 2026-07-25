// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include "System/Attribute.hpp"

namespace System::Runtime::CompilerServices {

/**
 * Indicates that compiler support for a particular feature is required at the annotated location.
 *
 * C++ counterpart of .NET System.Runtime.CompilerServices.CompilerFeatureRequiredAttribute. It
 * stores metadata only; C++ compilers do not consume .NET compiler-feature annotations.
 */
class CompilerFeatureRequiredAttribute final : public System::Attribute {
    std::string featureName_;
    bool isOptional_ = false;

public:
    /** The feature-name token used by C# for ref-struct support. */
    static constexpr std::string_view RefStructs = "RefStructs";

    /** The feature-name token used by C# for required-member support. */
    static constexpr std::string_view RequiredMembers = "RequiredMembers";

    /** Initializes the attribute with the name of its required compiler feature. */
    explicit CompilerFeatureRequiredAttribute(std::string featureName)
        : featureName_(std::move(featureName)) {}

    /** Gets the required compiler feature's name. */
    [[nodiscard]] const std::string& getFeatureNameProperty() const noexcept { return featureName_; }

    /** Gets whether a compiler may ignore an unknown feature name. */
    [[nodiscard]] bool getIsOptionalProperty() const noexcept { return isOptional_; }

    /** Sets whether a compiler may ignore an unknown feature name. */
    void setIsOptionalProperty(bool value) noexcept { isOptional_ = value; }
};

} // namespace System::Runtime::CompilerServices
