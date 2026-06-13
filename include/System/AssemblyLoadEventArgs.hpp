// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/EventArgs.hpp"

namespace System {

    /// Provides data for the event that is raised when an assembly is loaded.
    class AssemblyLoadEventArgs : public EventArgs {
        std::string assemblyName_;
    public:
        /// Initializes a new instance with the name of the loaded assembly.
        explicit AssemblyLoadEventArgs(const std::string& assemblyName)
            : assemblyName_(assemblyName) {}

        /// Returns the name of the assembly that was loaded.
        [[nodiscard]] const std::string& getLoadedAssemblyProperty() const { return assemblyName_; }
    };

    /// Delegate type for assembly-load event handlers.
    using AssemblyLoadEventHandler = std::function<void(void*, AssemblyLoadEventArgs&)>;

} // namespace System
