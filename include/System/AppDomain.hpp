// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <string>
#include "System/MarshalByRefObject.hpp"
#include "System/UnhandledExceptionEventHandler.hpp"

namespace System {

    /**
     * @brief Represents an application domain — the isolated execution environment for an application.
     *
     * Only the singleton @c CurrentDomain is supported. Implements the subset of the .NET
     * @c System.AppDomain API needed for game-engine porting (friendly name, base directory,
     * event stubs, and data storage stubs).
     */
    class AppDomain : public MarshalByRefObject {
        std::string friendlyName_  = "DefaultDomain";
        std::string baseDirectory_;

        AppDomain();

    public:
        /// @brief Returns the single application-domain instance for this process.
        static AppDomain& CurrentDomain() {
            static AppDomain domain;
            return domain;
        }

        /// @brief Returns the friendly name of this domain (always @c "DefaultDomain").
        [[nodiscard]] const std::string& getFriendlyNameProperty()  const { return friendlyName_; }

        /// @brief Returns the base directory of the application — the directory containing the
        ///        executable, with a trailing @c '/'. Reads @c /proc/self/exe at first use.
        [[nodiscard]] const std::string& getBaseDirectoryProperty() const { return baseDirectory_; }

        /// @brief Stub — event registration is not functional; provided for API compatibility.
        void add_UnhandledException(const UnhandledExceptionEventHandler& /*handler*/) {}
        /// @brief Stub — event registration is not functional; provided for API compatibility.
        void remove_UnhandledException(const UnhandledExceptionEventHandler& /*handler*/) {}

        /// @brief Stub — event registration is not functional; provided for API compatibility.
        void add_ProcessExit(const std::function<void(void*, EventArgs&)>& /*handler*/) {}
        /// @brief Stub — event registration is not functional; provided for API compatibility.
        void remove_ProcessExit(const std::function<void(void*, EventArgs&)>& /*handler*/) {}

        /// @brief Stub — stores nothing; provided for API compatibility.
        void SetData(const std::string& /*name*/, void* /*data*/) {}
        /// @brief Stub — always returns @c nullptr; provided for API compatibility.
        void* GetData(const std::string& /*name*/) { return nullptr; }
    };

} // namespace System
