// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <cstring>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;
    using SharpRuntime::longcs;
    using SharpRuntime::shortcs;
    using SharpRuntime::Single;

    /**
     * @brief Converts base data types to an array of bytes, and an array of
     * bytes to base data types (little-endian).
     *
     * Partial C++ counterpart of .NET System.BitConverter.
     *
     * @note Status: Implemented
     */
    class BitConverter {
    public:
        BitConverter() = delete;

        static inline const bool IsLittleEndian = true; // we assume LE; real check would use runtime test

        // --- To bytes ---
        [[nodiscard]] static std::array<bytecs,2> GetBytes(shortcs value) {
            std::array<bytecs,2> b;
            std::memcpy(b.data(), &value, 2);
            return b;
        }
        [[nodiscard]] static std::array<bytecs,4> GetBytes(intcs value) {
            std::array<bytecs,4> b;
            std::memcpy(b.data(), &value, 4);
            return b;
        }
        [[nodiscard]] static std::array<bytecs,8> GetBytes(longcs value) {
            std::array<bytecs,8> b;
            std::memcpy(b.data(), &value, 8);
            return b;
        }
        [[nodiscard]] static std::array<bytecs,4> GetBytes(Single value) {
            std::array<bytecs,4> b;
            std::memcpy(b.data(), &value, 4);
            return b;
        }
        [[nodiscard]] static std::array<bytecs,8> GetBytes(double value) {
            std::array<bytecs,8> b;
            std::memcpy(b.data(), &value, 8);
            return b;
        }
        [[nodiscard]] static std::array<bytecs,1> GetBytes(bool value) {
            return { static_cast<bytecs>(value ? 1 : 0) };
        }

        // --- From bytes ---
        [[nodiscard]] static shortcs ToInt16 (const bytecs* value, intcs startIndex) { shortcs r; std::memcpy(&r, value+startIndex, 2); return r; }
        [[nodiscard]] static intcs   ToInt32 (const bytecs* value, intcs startIndex) { intcs   r; std::memcpy(&r, value+startIndex, 4); return r; }
        [[nodiscard]] static longcs  ToInt64 (const bytecs* value, intcs startIndex) { longcs  r; std::memcpy(&r, value+startIndex, 8); return r; }
        [[nodiscard]] static Single  ToSingle(const bytecs* value, intcs startIndex) { Single  r; std::memcpy(&r, value+startIndex, 4); return r; }
        [[nodiscard]] static double  ToDouble(const bytecs* value, intcs startIndex) { double  r; std::memcpy(&r, value+startIndex, 8); return r; }
        [[nodiscard]] static bool    ToBoolean(const bytecs* value, intcs startIndex) { return value[startIndex] != 0; }

        // Vector overloads
        [[nodiscard]] static shortcs ToInt16 (const std::vector<bytecs>& v, intcs i) { return ToInt16 (v.data(), i); }
        [[nodiscard]] static intcs   ToInt32 (const std::vector<bytecs>& v, intcs i) { return ToInt32 (v.data(), i); }
        [[nodiscard]] static longcs  ToInt64 (const std::vector<bytecs>& v, intcs i) { return ToInt64 (v.data(), i); }
        [[nodiscard]] static Single  ToSingle(const std::vector<bytecs>& v, intcs i) { return ToSingle(v.data(), i); }
        [[nodiscard]] static double  ToDouble(const std::vector<bytecs>& v, intcs i) { return ToDouble(v.data(), i); }

        [[nodiscard]] static std::string ToString(const bytecs* value, intcs startIndex, intcs length);
        [[nodiscard]] static std::string ToString(const std::vector<bytecs>& value);
    };

} // namespace System
