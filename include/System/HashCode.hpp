// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
#include "System/Collections/Generic/IEqualityComparer.hpp"

namespace System {

    /**
     * @brief Provides a hash code accumulator that combines multiple values into
     * a single hash code.
     *
     * This is a C++ counterpart of .NET System.HashCode.
     * The internal algorithm is FNV-1a; exact hash values will differ from
     * the .NET xxHash32-based implementation, but the API contract is identical.
     */
    class HashCode {
        uint32_t hash_ = 2166136261u; // FNV offset basis

        static constexpr uint32_t FNV_PRIME = 16777619u;

        void mix(uint32_t value) noexcept {
            hash_ ^= value;
            hash_ *= FNV_PRIME;
        }

    public:
        /**
         * @brief Adds a single value to the hash code.
         * @tparam T The type of the value to add.
         * @param value The value to add.
         */
        template<typename T>
        void Add(const T& value) noexcept {
            mix(static_cast<uint32_t>(std::hash<T>{}(value)));
        }

        /**
         * @brief Adds a single value to the hash code, using the specified comparer.
         * @tparam T The type of the value to add.
         * @param value The value to add.
         * @param comparer The comparer whose GetHashCode is used.
         */
        template<typename T>
        void Add(const T& value, const System::Collections::Generic::IEqualityComparer<T>& comparer) noexcept {
            mix(static_cast<uint32_t>(comparer.GetHashCode(value)));
        }

        /**
         * @brief Adds a span of bytes to the hash code.
         * @param data Pointer to the byte array.
         * @param length Number of bytes.
         */
        void AddBytes(const uint8_t* data, std::size_t length) noexcept {
            for (std::size_t i = 0; i < length; ++i)
                mix(static_cast<uint32_t>(data[i]));
        }

        /**
         * @brief Adds a vector of bytes to the hash code.
         * @param bytes The bytes to add.
         */
        void AddBytes(const std::vector<uint8_t>& bytes) noexcept {
            AddBytes(bytes.data(), bytes.size());
        }

        /**
         * @brief Returns the final hash code produced by the accumulated values.
         * @return The computed hash code.
         */
        [[nodiscard]] int ToHashCode() const noexcept {
            return static_cast<int>(hash_);
        }

        /** @brief Combines one value into a single hash code. */
        template<typename T1>
        static int Combine(const T1& v1) {
            HashCode hc; hc.Add(v1); return hc.ToHashCode();
        }
        /** @brief Combines two values into a single hash code. */
        template<typename T1, typename T2>
        static int Combine(const T1& v1, const T2& v2) {
            HashCode hc; hc.Add(v1); hc.Add(v2); return hc.ToHashCode();
        }
        /** @brief Combines three values into a single hash code. */
        template<typename T1, typename T2, typename T3>
        static int Combine(const T1& v1, const T2& v2, const T3& v3) {
            HashCode hc; hc.Add(v1); hc.Add(v2); hc.Add(v3); return hc.ToHashCode();
        }
        /** @brief Combines four values into a single hash code. */
        template<typename T1, typename T2, typename T3, typename T4>
        static int Combine(const T1& v1, const T2& v2, const T3& v3, const T4& v4) {
            HashCode hc; hc.Add(v1); hc.Add(v2); hc.Add(v3); hc.Add(v4); return hc.ToHashCode();
        }
        /** @brief Combines five values into a single hash code. */
        template<typename T1, typename T2, typename T3, typename T4, typename T5>
        static int Combine(const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5) {
            HashCode hc; hc.Add(v1); hc.Add(v2); hc.Add(v3); hc.Add(v4); hc.Add(v5);
            return hc.ToHashCode();
        }
        /** @brief Combines six values into a single hash code. */
        template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
        static int Combine(const T1& v1, const T2& v2, const T3& v3, const T4& v4,
                           const T5& v5, const T6& v6) {
            HashCode hc; hc.Add(v1); hc.Add(v2); hc.Add(v3); hc.Add(v4); hc.Add(v5); hc.Add(v6);
            return hc.ToHashCode();
        }
        /** @brief Combines seven values into a single hash code. */
        template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
        static int Combine(const T1& v1, const T2& v2, const T3& v3, const T4& v4,
                           const T5& v5, const T6& v6, const T7& v7) {
            HashCode hc; hc.Add(v1); hc.Add(v2); hc.Add(v3); hc.Add(v4);
            hc.Add(v5); hc.Add(v6); hc.Add(v7);
            return hc.ToHashCode();
        }
        /** @brief Combines eight values into a single hash code. */
        template<typename T1, typename T2, typename T3, typename T4,
                 typename T5, typename T6, typename T7, typename T8>
        static int Combine(const T1& v1, const T2& v2, const T3& v3, const T4& v4,
                           const T5& v5, const T6& v6, const T7& v7, const T8& v8) {
            HashCode hc; hc.Add(v1); hc.Add(v2); hc.Add(v3); hc.Add(v4);
            hc.Add(v5); hc.Add(v6); hc.Add(v7); hc.Add(v8);
            return hc.ToHashCode();
        }
    };

} // namespace System
