// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/EventArgs.hpp"

namespace System {

    class AssemblyLoadEventArgs : public EventArgs {
        std::string assemblyName_;
    public:
        explicit AssemblyLoadEventArgs(const std::string& assemblyName)
            : assemblyName_(assemblyName) {}

        [[nodiscard]] const std::string& getLoadedAssemblyProperty() const { return assemblyName_; }
    };

    using AssemblyLoadEventHandler = std::function<void(void*, AssemblyLoadEventArgs&)>;

} // namespace System
