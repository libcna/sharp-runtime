// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"
#include <memory>

namespace System {

    /**
     * @brief The exception thrown when the static initializer for a type throws
     * an exception.
     *
     * C++ counterpart of .NET System.TypeInitializationException.
     * Stores the fully-qualified type name accessible via getTypeNameProperty().
     */
    class TypeInitializationException : public SystemException {
        std::string typeName_;
    public:
        /**
         * @brief Initializes a new instance with the fully-qualified type name
         * and an optional inner exception.
         * @param fullTypeName Fully-qualified name of the type whose initializer failed.
         * @param inner        The inner exception, or nullptr if none.
         */
        TypeInitializationException(const std::string& fullTypeName, const std::exception* inner)
            : SystemException("The type initializer for '" + fullTypeName + "' threw an exception."
                + (inner ? std::string(" | inner: ") + inner->what() : "")),
              typeName_(fullTypeName) {}

        /** @brief Returns the fully-qualified name of the type whose initializer failed. */
        [[nodiscard]] const std::string& getTypeNameProperty() const { return typeName_; }
    };

} // namespace System
