// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/ComponentModel/Design/Serialization/InstanceDescriptor.hpp"

#include "System/ArgumentException.hpp"
#include "System/Reflection/ConstructorInfo.hpp"
#include "System/Reflection/MethodBase.hpp"

namespace System::ComponentModel::Design::Serialization {

InstanceDescriptor::InstanceDescriptor(std::shared_ptr<const System::Reflection::MemberInfo> member,
                                       std::vector<std::any> arguments, bool isComplete)
    : member_(std::move(member)), arguments_(std::move(arguments)), isComplete_(isComplete) {
    // InstanceDescriptor.cs: a constructor may not be static and its parameter count must
    // match; a method must be static; a field or property must be static and readable. This
    // runtime declares constructors (and the MethodBase they derive from), so those are the
    // branches that can be taken.
    if (const auto* method = dynamic_cast<const System::Reflection::MethodBase*>(member_.get())) {
        const bool isConstructor = method->getMemberTypeProperty() == System::Reflection::MemberTypes::Constructor;
        if (isConstructor && method->getIsStaticProperty()) {
            throw System::ArgumentException("Parameter cannot be static.");
        }
        if (!isConstructor && !method->getIsStaticProperty()) {
            throw System::ArgumentException("Parameter must be static.");
        }
        if (arguments_.size() != method->GetParameters().size()) {
            throw System::ArgumentException("Length mismatch.");
        }
    }
}

std::any InstanceDescriptor::Invoke() const {
    std::vector<std::any> translatedArguments = arguments_;
    for (std::any& argument : translatedArguments) {
        if (const auto* descriptor = std::any_cast<InstanceDescriptor>(&argument)) {
            argument = descriptor->Invoke();
        }
    }
    if (const auto* ctor = dynamic_cast<const System::Reflection::ConstructorInfo*>(member_.get())) {
        return ctor->Invoke(translatedArguments);
    }
    if (const auto* method = dynamic_cast<const System::Reflection::MethodBase*>(member_.get())) {
        return method->Invoke(std::any{}, translatedArguments);
    }
    return {};
}

} // namespace System::ComponentModel::Design::Serialization
