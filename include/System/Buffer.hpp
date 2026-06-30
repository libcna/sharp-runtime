// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstring>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace System {

    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;
    using SharpRuntime::longcs;
    using SharpRuntime::ulongcs;

    /**
     * @brief Manipulates arrays of primitive types as raw binary memory.
     *
     * C++ counterpart of .NET System.Buffer (static class).
     * Provides low-level byte-oriented operations: BlockCopy, ByteLength,
     * GetByte, SetByte, and MemoryCopy.
     */
    class Buffer {
    public:
        /** @brief Deleted constructor — all members are static. */
        Buffer() = delete;

        // -----------------------------------------------------------------------
        // BlockCopy
        // -----------------------------------------------------------------------

        /**
         * @brief Copies @p count bytes from @p src (at @p srcOffset) to @p dst
         * (at @p dstOffset).
         *
         * C++ counterpart of .NET Buffer.BlockCopy(Array, int, Array, int, int).
         * Uses std::memmove, so overlapping source and destination regions are
         * safe — matching .NET, whose BlockCopy copies via Memmove.
         *
         * @param src       Pointer to the source region.
         * @param srcOffset Byte offset into @p src.
         * @param dst       Pointer to the destination region.
         * @param dstOffset Byte offset into @p dst.
         * @param count     Number of bytes to copy.
         */
        static void BlockCopy(const void* src, intcs srcOffset,
                               void* dst, intcs dstOffset, intcs count) {
            std::memmove(static_cast<bytecs*>(dst) + dstOffset,
                         static_cast<const bytecs*>(src) + srcOffset,
                         static_cast<size_t>(count));
        }

        /**
         * @brief Copies @p count bytes between two byte vectors.
         *
         * C++ counterpart of .NET Buffer.BlockCopy(Array, int, Array, int, int)
         * specialised for byte vectors.
         *
         * @param src       Source byte vector.
         * @param srcOffset Byte offset into @p src.
         * @param dst       Destination byte vector.
         * @param dstOffset Byte offset into @p dst.
         * @param count     Number of bytes to copy.
         */
        static void BlockCopy(const std::vector<bytecs>& src, intcs srcOffset,
                               std::vector<bytecs>& dst, intcs dstOffset, intcs count) {
            std::memmove(dst.data() + dstOffset,
                         src.data() + srcOffset,
                         static_cast<size_t>(count));
        }

        /**
         * @brief Copies @p count raw bytes from @p src (at byte offset @p srcOffset)
         * into @p dst (at byte offset @p dstOffset), treating both vectors as
         * contiguous binary storage.
         *
         * C++ counterpart of .NET Buffer.BlockCopy(Array, int, Array, int, int)
         * for arbitrary primitive element types.
         *
         * @tparam T Element type (must be trivially-copyable).
         * @param src       Source vector.
         * @param srcOffset Byte offset into @p src raw memory.
         * @param dst       Destination vector.
         * @param dstOffset Byte offset into @p dst raw memory.
         * @param count     Number of bytes to copy.
         */
        template<typename T>
        static void BlockCopy(const std::vector<T>& src, intcs srcOffset,
                               std::vector<T>& dst, intcs dstOffset, intcs count) {
            std::memmove(reinterpret_cast<bytecs*>(dst.data()) + dstOffset,
                         reinterpret_cast<const bytecs*>(src.data()) + srcOffset,
                         static_cast<size_t>(count));
        }

        // -----------------------------------------------------------------------
        // ByteLength
        // -----------------------------------------------------------------------

        /**
         * @brief Returns the number of bytes in the underlying memory of @p array.
         *
         * C++ counterpart of .NET Buffer.ByteLength(Array).
         * Equal to @c array.size() * sizeof(T).
         *
         * @tparam T Element type (must be a primitive / trivially-copyable type).
         * @param  array The vector whose byte length is returned.
         * @return Total byte count.
         */
        template<typename T>
        static intcs ByteLength(const std::vector<T>& array) {
            return static_cast<intcs>(array.size() * sizeof(T));
        }

        // -----------------------------------------------------------------------
        // GetByte / SetByte
        // -----------------------------------------------------------------------

        /**
         * @brief Returns the byte at position @p index in the raw binary
         * representation of @p array.
         *
         * C++ counterpart of .NET Buffer.GetByte(Array, int).
         * The value is read from the underlying memory of the vector element
         * storage in native byte order.
         *
         * @tparam T Element type.
         * @param  array The vector to read from.
         * @param  index Zero-based byte index into the raw memory.
         * @return The byte at @p index.
         */
        template<typename T>
        static bytecs GetByte(const std::vector<T>& array, intcs index) {
            return reinterpret_cast<const bytecs*>(array.data())[index];
        }

        /**
         * @brief Sets the byte at position @p index in the raw binary
         * representation of @p array to @p value.
         *
         * C++ counterpart of .NET Buffer.SetByte(Array, int, byte).
         *
         * @tparam T    Element type.
         * @param  array The vector to modify.
         * @param  index Zero-based byte index into the raw memory.
         * @param  value The byte value to write.
         */
        template<typename T>
        static void SetByte(std::vector<T>& array, intcs index, bytecs value) {
            reinterpret_cast<bytecs*>(array.data())[index] = value;
        }

        // -----------------------------------------------------------------------
        // MemoryCopy
        // -----------------------------------------------------------------------

        /**
         * @brief Copies @p sourceBytesToCopy bytes from @p source to
         * @p destination, allowing overlapping regions.
         *
         * C++ counterpart of .NET Buffer.MemoryCopy(void*, void*, long, long).
         * Uses std::memmove so overlapping source and destination are safe.
         *
         * @param source                 Pointer to the source memory block.
         * @param destination            Pointer to the destination memory block.
         * @param destinationSizeInBytes Capacity of the destination block in bytes.
         * @param sourceBytesToCopy      Number of bytes to copy.
         * @throws System::ArgumentOutOfRangeException if
         *         @p sourceBytesToCopy > @p destinationSizeInBytes.
         */
        static void MemoryCopy(const void* source, void* destination,
                                longcs destinationSizeInBytes,
                                longcs sourceBytesToCopy) {
            if (sourceBytesToCopy > destinationSizeInBytes)
                throw ArgumentOutOfRangeException(
                    "sourceBytesToCopy exceeds destinationSizeInBytes");
            std::memmove(destination, source, static_cast<size_t>(sourceBytesToCopy));
        }

        /**
         * @brief Copies @p sourceBytesToCopy bytes from @p source to @p destination,
         * allowing overlapping regions. Unsigned-size overload.
         *
         * C++ counterpart of .NET Buffer.MemoryCopy(void*, void*, ulong, ulong).
         *
         * @param source                 Pointer to the source memory block.
         * @param destination            Pointer to the destination memory block.
         * @param destinationSizeInBytes Capacity of the destination block in bytes.
         * @param sourceBytesToCopy      Number of bytes to copy.
         * @throws System::ArgumentOutOfRangeException if
         *         @p sourceBytesToCopy > @p destinationSizeInBytes.
         */
        static void MemoryCopy(const void* source, void* destination,
                                ulongcs destinationSizeInBytes,
                                ulongcs sourceBytesToCopy) {
            if (sourceBytesToCopy > destinationSizeInBytes)
                throw ArgumentOutOfRangeException(
                    "sourceBytesToCopy exceeds destinationSizeInBytes");
            std::memmove(destination, source, static_cast<size_t>(sourceBytesToCopy));
        }
    };

} // namespace System
