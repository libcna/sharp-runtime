// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/IO/IsolatedStorage/IsolatedStorageScope.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO::IsolatedStorage {

    using SharpRuntime::longcs;

    /**
     * @brief Abstract base class for isolated storage implementations.
     *
     * Partial C++ counterpart of .NET System.IO.IsolatedStorage.IsolatedStorage.
     *
     * @note Status: Stub — concrete functionality is in IsolatedStorageFile.
     */
    class IsolatedStorage {
    protected:
        IsolatedStorageScope scope_ = IsolatedStorageScope::None;
        IsolatedStorage() = default;
    public:
        virtual ~IsolatedStorage() = default;

        [[nodiscard]] IsolatedStorageScope getScopeProperty() const { return scope_; }

        [[nodiscard]] virtual longcs getAvailableFreeSpaceProperty() const { return 0; }
        [[nodiscard]] virtual longcs getQuotaProperty() const { return 0; }
        [[nodiscard]] virtual longcs getUsedSizeProperty() const { return 0; }

        virtual void Remove() = 0;
        virtual void Close()  {}
    };

} // namespace System::IO::IsolatedStorage
