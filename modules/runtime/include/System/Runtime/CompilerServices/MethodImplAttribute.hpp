// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Attribute.hpp"
#include "System/Runtime/CompilerServices/MethodCodeType.hpp"
#include "System/Runtime/CompilerServices/MethodImplOptions.hpp"

namespace System::Runtime::CompilerServices {

    /**
     * Specifies the details of how a method is implemented (e.g. inlining hints).
     * 
     * C++ counterpart of .NET System.Runtime.CompilerServices.MethodImplAttribute.
     * The values describe metadata/JIT behaviour and are informational in C++.
     */
    class MethodImplAttribute : public System::Attribute {
        MethodImplOptions value_{};
        MethodCodeType methodCodeType_ = MethodCodeType::IL;
    public:
        /** Constructs the attribute with the default implementation options. */
        MethodImplAttribute() = default;

        /** @param value The MethodImplOptions flags controlling the implementation. */
        explicit MethodImplAttribute(MethodImplOptions value) : value_(value) {}

        /**
         * Constructs from a raw integer (cast to MethodImplOptions).
         * @param value Integer representation of MethodImplOptions flags.
         */
        explicit MethodImplAttribute(SharpRuntime::shortcs value)
            : value_(static_cast<MethodImplOptions>(value)) {}

        /** @return The MethodImplOptions value. */
        [[nodiscard]] MethodImplOptions getValueProperty() const { return value_; }

        /** Gets the type of method implementation. Defaults to IL. */
        [[nodiscard]] MethodCodeType getMethodCodeTypeProperty() const noexcept {
            return methodCodeType_;
        }

        /** Sets the type of method implementation metadata. */
        void setMethodCodeTypeProperty(MethodCodeType value) noexcept { methodCodeType_ = value; }
    };

} // namespace System::Runtime::CompilerServices
