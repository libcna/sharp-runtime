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

    /**
     * Initializes the attribute with its feature name and optionality.
     *
     * #1980 group G-4 / SR-AUD-160. .NET declares `public bool IsOptional { get; init; }`
     * (`CompilerFeatureRequiredAttribute.cs`), and `init` means the value can be supplied **at
     * construction and never afterwards** -- `new CompilerFeatureRequiredAttribute("X") {
     * IsOptional = true }` is legal, assigning to it later is not. C++ has no `init`, and the
     * exact analogue of that pair of facts is a constructor parameter with no setter. This
     * overload is what makes removing `setIsOptionalProperty` a *translation* rather than a
     * narrowing: the value is still settable, just no longer mutable.
     */
    CompilerFeatureRequiredAttribute(std::string featureName, bool isOptional)
        : featureName_(std::move(featureName)), isOptional_(isOptional) {}

    /** Gets the required compiler feature's name. */
    [[nodiscard]] const std::string& getFeatureNameProperty() const noexcept { return featureName_; }

    /** Gets whether a compiler may ignore an unknown feature name. */
    [[nodiscard]] bool getIsOptionalProperty() const noexcept { return isOptional_; }

    // #1980 G-4 / SR-AUD-160: setIsOptionalProperty is GONE. .NET's IsOptional is `{ get; init; }`
    // -- settable at construction, immutable afterwards -- so a full setter published a mutability
    // .NET does not have. Supply the value through the two-argument constructor above.
};

} // namespace System::Runtime::CompilerServices
