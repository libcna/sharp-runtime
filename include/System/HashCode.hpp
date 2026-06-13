// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

namespace System {

    /// Provides a FNV-1a hash accumulator, similar in spirit to .NET System.HashCode.
    class HashCode {
        uint32_t hash_ = 2166136261u; // FNV offset basis

        static constexpr uint32_t FNV_PRIME = 16777619u;

        void mix(uint32_t value) noexcept {
            hash_ ^= value;
            hash_ *= FNV_PRIME;
        }

    public:
        /// Adds the specified value to the hash.
        template<typename T>
        void Add(const T& value) noexcept {
            mix(static_cast<uint32_t>(std::hash<T>{}(value)));
        }

        /// Returns the final hash code produced by the accumulated values.
        [[nodiscard]] int ToHashCode() const noexcept {
            return static_cast<int>(hash_);
        }

        // Convenience: combine 1–4 values into a hash without creating an instance.
        /// Combines one value into a single hash code.
        template<typename T1>
        static int Combine(const T1& v1) {
            HashCode hc; hc.Add(v1); return hc.ToHashCode();
        }
        /// Combines two values into a single hash code.
        template<typename T1, typename T2>
        static int Combine(const T1& v1, const T2& v2) {
            HashCode hc; hc.Add(v1); hc.Add(v2); return hc.ToHashCode();
        }
        /// Combines three values into a single hash code.
        template<typename T1, typename T2, typename T3>
        static int Combine(const T1& v1, const T2& v2, const T3& v3) {
            HashCode hc; hc.Add(v1); hc.Add(v2); hc.Add(v3); return hc.ToHashCode();
        }
        /// Combines four values into a single hash code.
        template<typename T1, typename T2, typename T3, typename T4>
        static int Combine(const T1& v1, const T2& v2, const T3& v3, const T4& v4) {
            HashCode hc; hc.Add(v1); hc.Add(v2); hc.Add(v3); hc.Add(v4); return hc.ToHashCode();
        }
        /// Combines five values into a single hash code.
        template<typename T1, typename T2, typename T3, typename T4, typename T5>
        static int Combine(const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5) {
            HashCode hc; hc.Add(v1); hc.Add(v2); hc.Add(v3); hc.Add(v4); hc.Add(v5);
            return hc.ToHashCode();
        }
        /// Combines six values into a single hash code.
        template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
        static int Combine(const T1& v1, const T2& v2, const T3& v3, const T4& v4,
                           const T5& v5, const T6& v6) {
            HashCode hc; hc.Add(v1); hc.Add(v2); hc.Add(v3); hc.Add(v4); hc.Add(v5); hc.Add(v6);
            return hc.ToHashCode();
        }
        /// Combines seven values into a single hash code.
        template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
        static int Combine(const T1& v1, const T2& v2, const T3& v3, const T4& v4,
                           const T5& v5, const T6& v6, const T7& v7) {
            HashCode hc; hc.Add(v1); hc.Add(v2); hc.Add(v3); hc.Add(v4);
            hc.Add(v5); hc.Add(v6); hc.Add(v7);
            return hc.ToHashCode();
        }
        /// Combines eight values into a single hash code.
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
