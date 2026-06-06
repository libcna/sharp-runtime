// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"
#include <memory>

namespace System {

    class TypeInitializationException : public SystemException {
        std::string typeName_;
    public:
        TypeInitializationException(const std::string& fullTypeName, const std::exception* inner)
            : SystemException("The type initializer for '" + fullTypeName + "' threw an exception."
                + (inner ? std::string(" | inner: ") + inner->what() : "")),
              typeName_(fullTypeName) {}

        [[nodiscard]] const std::string& getTypeNameProperty() const { return typeName_; }
    };

} // namespace System
