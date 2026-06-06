// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <cstdint>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;
    using SharpRuntime::shortcs;
    using SharpRuntime::bytecs;
    using SharpRuntime::Single;

    /**
     * @brief Converts a base data type to another base data type.
     *
     * Partial C++ counterpart of .NET System.Convert.
     *
     * @note Status: Partial
     */
    class Convert {
    public:
        Convert() = delete;

        // --- To numeric types ---
        [[nodiscard]] static intcs    ToInt32(const std::string& value);
        [[nodiscard]] static intcs    ToInt32(bool value)   { return value ? 1 : 0; }
        [[nodiscard]] static intcs    ToInt32(double value);
        [[nodiscard]] static intcs    ToInt32(float value);
        [[nodiscard]] static intcs    ToInt32(intcs value)  { return value; }
        [[nodiscard]] static intcs    ToInt32(longcs value);
        [[nodiscard]] static intcs    ToInt32(bytecs value) { return static_cast<intcs>(value); }

        [[nodiscard]] static longcs   ToInt64(const std::string& value);
        [[nodiscard]] static longcs   ToInt64(intcs value)  { return static_cast<longcs>(value); }
        [[nodiscard]] static longcs   ToInt64(double value);

        [[nodiscard]] static shortcs  ToInt16(const std::string& value);
        [[nodiscard]] static shortcs  ToInt16(intcs value);

        [[nodiscard]] static double ToDouble(const std::string& value);
        [[nodiscard]] static double ToDouble(intcs value)  { return static_cast<double>(value); }
        [[nodiscard]] static double ToDouble(longcs value) { return static_cast<double>(value); }

        [[nodiscard]] static Single   ToSingle(const std::string& value);
        [[nodiscard]] static Single   ToSingle(double value) { return static_cast<Single>(value); }
        [[nodiscard]] static Single   ToSingle(intcs value)  { return static_cast<Single>(value); }

        [[nodiscard]] static bytecs   ToByte(intcs value);
        [[nodiscard]] static bytecs   ToByte(const std::string& value);

        [[nodiscard]] static bool     ToBoolean(intcs value)            { return value != 0; }
        [[nodiscard]] static bool     ToBoolean(const std::string& value);

        // --- To string ---
        [[nodiscard]] static std::string ToString(intcs value);
        [[nodiscard]] static std::string ToString(longcs value);
        [[nodiscard]] static std::string ToString(double value);
        [[nodiscard]] static std::string ToString(float value);
        [[nodiscard]] static std::string ToString(bool value) { return value ? "True" : "False"; }
        [[nodiscard]] static std::string ToString(char value);
        [[nodiscard]] static std::string ToString(bytecs value);

        // --- Base-N conversions ---
        [[nodiscard]] static std::string ToString(intcs value, int toBase);
        [[nodiscard]] static intcs        ToInt32(const std::string& value, int fromBase);
    };

} // namespace System
