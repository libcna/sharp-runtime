// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <memory>
#include <utility>
#include <vector>
#include "System/Reflection/MemberInfo.hpp"

namespace System::ComponentModel::Design::Serialization {

    /**
     * @brief Provides the information necessary to create an instance of an object. This class
     *        cannot be inherited.
     *
     * C++ counterpart of .NET System.ComponentModel.Design.Serialization.InstanceDescriptor: a
     * member -- a constructor, in every use this runtime has -- and the boxed arguments to pass
     * it, from which `Invoke` recreates the value. It is what a `TypeConverter` returns for the
     * `InstanceDescriptor` destination type and accepts as a source, and it is how a designer
     * serializer writes `new Vector3(1, 2, 3)` for a value it cannot otherwise express.
     *
     * The member is a `System::Reflection::MemberInfo` from this runtime's explicit-metadata
     * subset (`ConstructorInfo::Of<T, Args...>()`); see that header for the boundary. Validation
     * is .NET's: a constructor may not be static and the argument count must match its
     * parameter count (`"Length mismatch."`), a static method or a readable static property or
     * field would be accepted if the runtime declared such members, and any other member kind
     * is stored without validation and invokes to an empty value.
     */
    class InstanceDescriptor final {
    public:
        /**
         * @brief Initializes a new instance using the specified member information and
         *        arguments.
         *
         * C++ counterpart of .NET InstanceDescriptor(MemberInfo, ICollection): complete.
         * @param member The member to invoke, or null.
         * @param arguments The boxed arguments, in order.
         * @throws System::ArgumentException if the argument count does not match an invocable
         *         member's parameters.
         */
        InstanceDescriptor(std::shared_ptr<const System::Reflection::MemberInfo> member, std::vector<std::any> arguments)
            : InstanceDescriptor(std::move(member), std::move(arguments), true) {}

        /**
         * @brief Initializes a new instance using the specified member information, arguments,
         *        and flag indicating whether the specified information completely describes the
         *        instance.
         *
         * C++ counterpart of .NET InstanceDescriptor(MemberInfo, ICollection, bool).
         * @param member The member to invoke, or null.
         * @param arguments The boxed arguments, in order.
         * @param isComplete Whether invoking the member fully recreates the instance.
         * @throws System::ArgumentException if the argument count does not match an invocable
         *         member's parameters.
         */
        InstanceDescriptor(std::shared_ptr<const System::Reflection::MemberInfo> member, std::vector<std::any> arguments,
                           bool isComplete);

        /**
         * @brief Gets the collection of arguments that can be used to reconstruct an instance of
         *        the object that this instance descriptor represents.
         *
         * C++ counterpart of .NET InstanceDescriptor.Arguments.
         * @return The boxed arguments.
         */
        [[nodiscard]] const std::vector<std::any>& getArgumentsProperty() const noexcept { return arguments_; }

        /**
         * @brief Gets a value indicating whether the contents of this InstanceDescriptor
         *        completely identify the instance.
         *
         * C++ counterpart of .NET InstanceDescriptor.IsComplete.
         * @return true if invoking the member recreates the instance in full.
         */
        [[nodiscard]] bool getIsCompleteProperty() const noexcept { return isComplete_; }

        /**
         * @brief Gets the member information that describes the instance this descriptor is
         *        associated with.
         *
         * C++ counterpart of .NET InstanceDescriptor.MemberInfo.
         * @return The member, or null.
         */
        [[nodiscard]] std::shared_ptr<const System::Reflection::MemberInfo> getMemberInfoProperty() const noexcept {
            return member_;
        }

        /**
         * @brief Invokes this instance descriptor and returns the object the descriptor
         *        describes.
         *
         * C++ counterpart of .NET InstanceDescriptor.Invoke(): nested `InstanceDescriptor`
         * arguments are invoked first, then a constructor is invoked with the translated
         * arguments; a null or non-invocable member yields an empty value.
         * @return The recreated instance, boxed.
         * @throws System::ArgumentException if an argument holds a value of the wrong type.
         */
        [[nodiscard]] std::any Invoke() const;

    private:
        std::shared_ptr<const System::Reflection::MemberInfo> member_;
        std::vector<std::any> arguments_;
        bool isComplete_;
    };

} // namespace System::ComponentModel::Design::Serialization
