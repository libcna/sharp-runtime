// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstring>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;

    /**
     * @brief Manipulates arrays of primitive types as raw binary memory.
     *
     * Partial C++ counterpart of .NET System.Buffer.
     *
     * @note Status: Implemented
     */
    class Buffer {
    public:
        /// Deleted constructor; all members are static.
        Buffer() = delete;

        /**
         * @brief Copies a specified number of bytes from a source array starting
         * at a particular offset to a destination array starting at a particular offset.
         */
        static void BlockCopy(const void* src, intcs srcOffset,
                               void* dst, intcs dstOffset, intcs count) {
            std::memcpy(static_cast<bytecs*>(dst) + dstOffset,
                        static_cast<const bytecs*>(src) + srcOffset,
                        static_cast<size_t>(count));
        }

        /// Copies a range of bytes between two byte vectors.
        static void BlockCopy(const std::vector<bytecs>& src, intcs srcOffset,
                               std::vector<bytecs>& dst, intcs dstOffset, intcs count) {
            std::memcpy(dst.data() + dstOffset,
                        src.data() + srcOffset,
                        static_cast<size_t>(count));
        }

        /** @brief Returns the number of bytes in the specified primitive-type array. */
        template<typename T>
        static intcs ByteLength(const std::vector<T>& array) {
            return static_cast<intcs>(array.size() * sizeof(T));
        }

        /** @brief Returns the byte at a particular position in a specified array. */
        template<typename T>
        static bytecs GetByte(const std::vector<T>& array, intcs index) {
            return reinterpret_cast<const bytecs*>(array.data())[index];
        }

        /** @brief Assigns a specified value to a byte at a particular location in a specified array. */
        template<typename T>
        static void SetByte(std::vector<T>& array, intcs index, bytecs value) {
            reinterpret_cast<bytecs*>(array.data())[index] = value;
        }
    };

} // namespace System
