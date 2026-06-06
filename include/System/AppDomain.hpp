// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <string>
#include "System/MarshalByRefObject.hpp"
#include "System/UnhandledExceptionEventHandler.hpp"

namespace System {

    class AppDomain : public MarshalByRefObject {
        std::string friendlyName_ = "DefaultDomain";
        std::string baseDirectory_ = ".";

        AppDomain() = default;

    public:
        static AppDomain& CurrentDomain() {
            static AppDomain domain;
            return domain;
        }

        [[nodiscard]] const std::string& getFriendlyNameProperty()  const { return friendlyName_; }
        [[nodiscard]] const std::string& getBaseDirectoryProperty() const { return baseDirectory_; }

        void add_UnhandledException(const UnhandledExceptionEventHandler& /*handler*/) {}
        void remove_UnhandledException(const UnhandledExceptionEventHandler& /*handler*/) {}

        void add_ProcessExit(const std::function<void(void*, EventArgs&)>& /*handler*/) {}
        void remove_ProcessExit(const std::function<void(void*, EventArgs&)>& /*handler*/) {}

        void SetData(const std::string& /*name*/, void* /*data*/) {}
        void* GetData(const std::string& /*name*/) { return nullptr; }
    };

} // namespace System
