// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System {

    /// Represents a nonexistent value, equivalent to a null reference in a database field.
    class DBNull {
        DBNull() = default;
    public:
        /// Returns the singleton DBNull instance.
        static DBNull& Value() {
            static DBNull instance;
            return instance;
        }

        /// Returns an empty string, consistent with .NET DBNull.ToString().
        [[nodiscard]] std::string ToString() const { return ""; }

        /// Deleted copy constructor — DBNull is a singleton.
        DBNull(const DBNull&) = delete;
        /// Deleted copy assignment — DBNull is a singleton.
        DBNull& operator=(const DBNull&) = delete;
    };

} // namespace System
