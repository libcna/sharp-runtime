// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/EventArgs.hpp"

namespace System {

    /// Event arguments for assembly/type resolution events, mirroring .NET System.ResolveEventArgs.
    class ResolveEventArgs : public EventArgs {
        std::string name_;
        std::string requestingAssemblyName_;

    public:
        /// @brief Constructs ResolveEventArgs for the given type or assembly name.
        /// @param name Fully qualified name of the type or assembly to resolve.
        explicit ResolveEventArgs(const std::string& name) : name_(name) {}
        /// @brief Constructs ResolveEventArgs with both the name and the requesting assembly name.
        /// @param name Fully qualified name of the type or assembly to resolve.
        /// @param requestingAssembly Name of the assembly that triggered the resolution.
        ResolveEventArgs(const std::string& name, const std::string& requestingAssembly)
            : name_(name), requestingAssemblyName_(requestingAssembly) {}

        /// Returns the name of the type or assembly to resolve.
        [[nodiscard]] const std::string& getNameProperty()                     const { return name_; }
        /// Returns the name of the assembly that triggered the resolution event.
        [[nodiscard]] const std::string& getRequestingAssemblyNameProperty()   const { return requestingAssemblyName_; }
    };

} // namespace System
