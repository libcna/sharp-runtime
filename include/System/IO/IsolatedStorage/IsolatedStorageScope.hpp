// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::IO::IsolatedStorage {

    /**
     * @brief Enumerates the levels of isolated storage scope.
     *
     * Partial C++ counterpart of .NET System.IO.IsolatedStorage.IsolatedStorageScope.
     *
     * @note Status: Implemented
     */
    enum class IsolatedStorageScope {
        None         = 0x00,
        User         = 0x01,
        Domain       = 0x02,
        Assembly     = 0x04,
        Roaming      = 0x08,
        Machine      = 0x10,
        Application  = 0x20
    };

    inline IsolatedStorageScope operator|(IsolatedStorageScope a, IsolatedStorageScope b) {
        return static_cast<IsolatedStorageScope>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline IsolatedStorageScope operator&(IsolatedStorageScope a, IsolatedStorageScope b) {
        return static_cast<IsolatedStorageScope>(static_cast<int>(a) & static_cast<int>(b));
    }

} // namespace System::IO::IsolatedStorage
