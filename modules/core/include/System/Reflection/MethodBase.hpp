// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <vector>
#include "System/Reflection/MemberInfo.hpp"
#include "System/Reflection/ParameterInfo.hpp"

namespace System::Reflection {

/**
 * @brief Provides information about methods and constructors.
 *
 * C++ counterpart of .NET System.Reflection.MethodBase, reduced to what an explicitly declared
 * invocable member needs: whether it is static, its parameter list, and the ability to invoke
 * it with boxed (`std::any`) arguments. See `MemberInfo` for the boundary this runtime draws
 * around reflection.
 */
class MethodBase : public MemberInfo {
public:
    /**
     * @brief Gets a value indicating whether the method is static.
     *
     * C++ counterpart of .NET MethodBase.IsStatic.
     * @return true for a static method; false for an instance method or a constructor.
     */
    [[nodiscard]] virtual bool getIsStaticProperty() const noexcept = 0;

    /**
     * @brief Gets the parameters of the specified method or constructor.
     *
     * C++ counterpart of .NET MethodBase.GetParameters().
     * @return The parameters, in declaration order.
     */
    [[nodiscard]] virtual const std::vector<ParameterInfo>& GetParameters() const noexcept = 0;

    /**
     * @brief Invokes the method or constructor represented by the current instance.
     *
     * C++ counterpart of .NET MethodBase.Invoke(object, object[]). Each argument must hold a
     * value of the corresponding parameter's exact type (no widening); an empty `std::any` is
     * .NET's `null` and yields the parameter type's default value.
     *
     * @param obj The instance to invoke on; ignored by constructors and static methods.
     * @param parameters The boxed arguments, one per parameter.
     * @return The boxed result: the constructed value for a constructor.
     * @throws TargetParameterCountException if @p parameters has the wrong length.
     * @throws System::ArgumentException if an argument holds a value of the wrong type.
     */
    [[nodiscard]] virtual std::any Invoke(const std::any& obj, const std::vector<std::any>& parameters) const = 0;

protected:
    MethodBase() = default;
};

} // namespace System::Reflection
