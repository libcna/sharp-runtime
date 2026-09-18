// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "System/ArgumentException.hpp"
#include "System/Reflection/MethodBase.hpp"
#include "System/Reflection/TargetParameterCountException.hpp"
#include "System/Type.hpp"

namespace System::Reflection {

/**
 * @brief Discovers the attributes of a class constructor and provides access to constructor
 *        metadata.
 *
 * C++ counterpart of .NET System.Reflection.ConstructorInfo. .NET hands one out from
 * `Type.GetConstructor(Type[])`; this runtime has no such lookup, so the *author* of a type
 * declares the constructor once, with the template factory:
 *
 * @code
 * static const auto ctor = ConstructorInfo::Of<Vector3, float, float, float>();
 * std::any v = ctor->Invoke({1.0f, 2.0f, 3.0f});   // holds a Vector3
 * @endcode
 *
 * The description is complete for its job: it names the declaring type and the parameter
 * types, and `Invoke` constructs the value. Two descriptions of the same constructor compare
 * equal. The type is final -- there is no `RuntimeConstructorInfo` behind it because there is
 * no runtime to ask.
 */
class ConstructorInfo final : public MethodBase {
public:
    /** @brief The invocation a description carries: boxed arguments in, boxed instance out. */
    using Invoker = std::function<std::any(const std::vector<std::any>&)>;

    /** @brief Represents the name of the class constructor method as it is stored in metadata. */
    static const std::string ConstructorName;

    /** @brief Represents the name of the type constructor method as it is stored in metadata. */
    static const std::string TypeConstructorName;

    /**
     * @brief Describes a constructor explicitly.
     *
     * Prefer `Of<T, Args...>()`, which derives the parameter list and the invoker from the
     * signature; this constructor exists for a description whose invocation is not a plain
     * `T(Args...)` expression.
     *
     * @param declaringType The type the constructor belongs to.
     * @param parameters The parameters, in order.
     * @param invoker The invocation; it receives exactly `parameters.size()` arguments.
     */
    ConstructorInfo(System::Type declaringType, std::vector<ParameterInfo> parameters, Invoker invoker)
        : declaringType_(declaringType), parameters_(std::move(parameters)), invoker_(std::move(invoker)) {}

    /**
     * @brief Describes the constructor `T(Args...)`.
     *
     * The parameter types are `Args...` in order; @p parameterNames, when given, must have one
     * entry per parameter and supplies `ParameterInfo::Name`. The invoker unboxes each argument
     * as its exact parameter type and returns `std::any(T(args...))`.
     *
     * @tparam T The constructed type.
     * @tparam Args The constructor's parameter types.
     * @param parameterNames Optional declared parameter names.
     * @return A shared description; the same signature always describes an equal member.
     * @throws System::ArgumentException if @p parameterNames is non-empty and its size differs
     *         from the parameter count.
     */
    template<class T, class... Args>
    [[nodiscard]] static std::shared_ptr<const ConstructorInfo> Of(std::vector<std::string> parameterNames = {}) {
        constexpr std::size_t count = sizeof...(Args);
        if (!parameterNames.empty() && parameterNames.size() != count) {
            throw System::ArgumentException("parameterNames must name every parameter or none.", "parameterNames");
        }
        std::vector<ParameterInfo> parameters;
        parameters.reserve(count);
        const System::Type parameterTypes[] = {System::Type::From<Args>()..., System::Type()};
        for (std::size_t i = 0; i < count; ++i) {
            parameters.emplace_back(parameterTypes[i], static_cast<SharpRuntime::intcs>(i),
                                    parameterNames.empty() ? std::string{} : parameterNames[i]);
        }
        Invoker invoker = [](const std::vector<std::any>& arguments) -> std::any {
            return construct<T, Args...>(arguments, std::index_sequence_for<Args...>{});
        };
        return std::make_shared<const ConstructorInfo>(System::Type::From<T>(), std::move(parameters), std::move(invoker));
    }

    /** @copydoc MemberInfo::getNameProperty */
    [[nodiscard]] const std::string& getNameProperty() const override { return ConstructorName; }

    /** @copydoc MemberInfo::getDeclaringTypeProperty */
    [[nodiscard]] System::Type getDeclaringTypeProperty() const override { return declaringType_; }

    /** @copydoc MemberInfo::getMemberTypeProperty */
    [[nodiscard]] MemberTypes getMemberTypeProperty() const override { return MemberTypes::Constructor; }

    /** @brief A constructor is never static. C++ counterpart of .NET ConstructorInfo.IsStatic. */
    [[nodiscard]] bool getIsStaticProperty() const noexcept override { return false; }

    /** @copydoc MethodBase::GetParameters */
    [[nodiscard]] const std::vector<ParameterInfo>& GetParameters() const noexcept override { return parameters_; }

    /**
     * @brief Invokes the constructor with the given arguments.
     *
     * C++ counterpart of .NET ConstructorInfo.Invoke(object[]).
     * @param parameters The boxed arguments, one per parameter.
     * @return The constructed instance, boxed.
     * @throws TargetParameterCountException if @p parameters has the wrong length.
     * @throws System::ArgumentException if an argument holds a value of the wrong type.
     */
    [[nodiscard]] std::any Invoke(const std::vector<std::any>& parameters) const {
        if (parameters.size() != parameters_.size()) {
            throw TargetParameterCountException();
        }
        return invoker_(parameters);
    }

    /** @copydoc MethodBase::Invoke */
    [[nodiscard]] std::any Invoke(const std::any& obj, const std::vector<std::any>& parameters) const override {
        (void)obj;
        return Invoke(parameters);
    }

    /**
     * @brief Two descriptions are equal when they name the same constructor: same declaring
     *        type and the same parameter types in the same order.
     */
    [[nodiscard]] bool Equals(const MemberInfo& other) const override {
        const auto* ctor = dynamic_cast<const ConstructorInfo*>(&other);
        if (ctor == nullptr || ctor->declaringType_ != declaringType_ ||
            ctor->parameters_.size() != parameters_.size()) {
            return false;
        }
        for (std::size_t i = 0; i < parameters_.size(); ++i) {
            if (parameters_[i].getParameterTypeProperty() != ctor->parameters_[i].getParameterTypeProperty()) {
                return false;
            }
        }
        return true;
    }

    /** @brief Hash consistent with `Equals`: the declaring type and the parameter types. */
    [[nodiscard]] int GetHashCode() const override {
        std::size_t hash = declaringType_.GetHashCode();
        for (const auto& parameter : parameters_) {
            hash = hash * 31u + parameter.getParameterTypeProperty().GetHashCode();
        }
        return static_cast<int>(hash);
    }

    /**
     * @brief Returns `"Void .ctor(<parameter types>)"`, the shape .NET prints for a constructor.
     */
    [[nodiscard]] std::string ToString() const override {
        std::string text = "Void .ctor(";
        for (std::size_t i = 0; i < parameters_.size(); ++i) {
            if (i != 0) text += ", ";
            text += parameters_[i].getParameterTypeProperty().getNameProperty();
        }
        text.push_back(')');
        return text;
    }

private:
    System::Type declaringType_;
    std::vector<ParameterInfo> parameters_;
    Invoker invoker_;

    template<class A>
    static std::decay_t<A> unbox(const std::any& argument, std::size_t position) {
        using Value = std::decay_t<A>;
        if (!argument.has_value()) {
            // .NET's binder substitutes the default value of a value-type parameter for null.
            return Value{};
        }
        if (const auto* value = std::any_cast<Value>(&argument)) {
            return *value;
        }
        throw System::ArgumentException(
            "Object of type '" + System::Type::FromTypeInfo(argument.type()).getNameProperty() +
            "' cannot be converted to type '" + System::Type::From<Value>().getNameProperty() +
            "' (parameter " + std::to_string(position) + ").");
    }

    template<class T, class... Args, std::size_t... I>
    static std::any construct(const std::vector<std::any>& arguments, std::index_sequence<I...>) {
        (void)arguments;   // unreferenced for a parameterless constructor
        return std::any(T(unbox<Args>(arguments[I], I)...));
    }
};

inline const std::string ConstructorInfo::ConstructorName = ".ctor";
inline const std::string ConstructorInfo::TypeConstructorName = ".cctor";

} // namespace System::Reflection
