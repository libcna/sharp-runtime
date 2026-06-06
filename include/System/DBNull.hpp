// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System {

    class DBNull {
        DBNull() = default;
    public:
        static DBNull& Value() {
            static DBNull instance;
            return instance;
        }

        [[nodiscard]] std::string ToString() const { return ""; }

        DBNull(const DBNull&) = delete;
        DBNull& operator=(const DBNull&) = delete;
    };

} // namespace System
