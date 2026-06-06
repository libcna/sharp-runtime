// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/EventArgs.hpp"

namespace System {

    class ResolveEventArgs : public EventArgs {
        std::string name_;
        std::string requestingAssemblyName_;

    public:
        explicit ResolveEventArgs(const std::string& name) : name_(name) {}
        ResolveEventArgs(const std::string& name, const std::string& requestingAssembly)
            : name_(name), requestingAssemblyName_(requestingAssembly) {}

        [[nodiscard]] const std::string& getNameProperty()                     const { return name_; }
        [[nodiscard]] const std::string& getRequestingAssemblyNameProperty()   const { return requestingAssemblyName_; }
    };

} // namespace System
