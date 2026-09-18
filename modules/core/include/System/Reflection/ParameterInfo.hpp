// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <utility>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Type.hpp"

namespace System::Reflection {

/**
 * @brief Discovers the attributes of a parameter and provides access to parameter metadata.
 *
 * C++ counterpart of .NET System.Reflection.ParameterInfo, reduced to the three facts a caller
 * declaring a member explicitly can state: the parameter's type, its position, and (optionally)
 * its name. A declared name is what `Name` reports; a parameter declared without one reports an
 * empty string rather than a fabricated identifier.
 */
class ParameterInfo {
    std::string name_;
    System::Type parameterType_;
    SharpRuntime::intcs position_;

public:
    /**
     * @brief Describes one parameter.
     * @param parameterType The parameter's type.
     * @param position The zero-based position in the parameter list.
     * @param name The declared parameter name, or empty when not stated.
     */
    ParameterInfo(System::Type parameterType, SharpRuntime::intcs position, std::string name = {})
        : name_(std::move(name)), parameterType_(parameterType), position_(position) {}

    /**
     * @brief Gets the name of the parameter.
     *
     * C++ counterpart of .NET ParameterInfo.Name.
     * @return The declared name, or an empty string when none was declared.
     */
    [[nodiscard]] const std::string& getNameProperty() const noexcept { return name_; }

    /**
     * @brief Gets the Type of this parameter.
     *
     * C++ counterpart of .NET ParameterInfo.ParameterType.
     * @return The parameter type.
     */
    [[nodiscard]] System::Type getParameterTypeProperty() const noexcept { return parameterType_; }

    /**
     * @brief Gets the zero-based position of the parameter in the formal parameter list.
     *
     * C++ counterpart of .NET ParameterInfo.Position.
     * @return The position.
     */
    [[nodiscard]] SharpRuntime::intcs getPositionProperty() const noexcept { return position_; }

    /**
     * @brief Gets the parameter type and name, if any, of the parameter.
     *
     * C++ counterpart of .NET ParameterInfo.ToString(): `"<Type> <name>"`, or just the type
     * when no name was declared.
     * @return The description.
     */
    [[nodiscard]] std::string ToString() const {
        std::string text = parameterType_.getNameProperty();
        if (!name_.empty()) {
            text.push_back(' ');
            text += name_;
        }
        return text;
    }
};

} // namespace System::Reflection
