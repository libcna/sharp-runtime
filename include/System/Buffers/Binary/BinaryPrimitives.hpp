// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "System/Span.hpp"

namespace System::Buffers::Binary {

    /**
     * @brief Reads and writes primitive values from and to byte spans in a specific byte order.
     *
     * C++ counterpart of .NET System.Buffers.Binary.BinaryPrimitives.
     * All methods are static. Little-endian and big-endian variants are provided for
     * int16, uint16, int32, uint32, int64, and uint64 types.
     */
    class BinaryPrimitives {
    public:
        BinaryPrimitives() = delete;

        // -----------------------------------------------------------------------
        // Read — little-endian
        // -----------------------------------------------------------------------

        /** @brief Reads a little-endian int16 from the first 2 bytes of @p source. */
        [[nodiscard]] static int16_t ReadInt16LittleEndian(System::ReadOnlySpan<uint8_t> source) {
            checkSize(source.getLengthProperty(), 2);
            int16_t v; std::memcpy(&v, source.getPointer(), 2); return fromLE16(v);
        }
        /** @brief Reads a little-endian uint16 from the first 2 bytes of @p source. */
        [[nodiscard]] static uint16_t ReadUInt16LittleEndian(System::ReadOnlySpan<uint8_t> source) {
            checkSize(source.getLengthProperty(), 2);
            uint16_t v; std::memcpy(&v, source.getPointer(), 2); return fromLE16u(v);
        }
        /** @brief Reads a little-endian int32 from the first 4 bytes of @p source. */
        [[nodiscard]] static int32_t ReadInt32LittleEndian(System::ReadOnlySpan<uint8_t> source) {
            checkSize(source.getLengthProperty(), 4);
            int32_t v; std::memcpy(&v, source.getPointer(), 4); return fromLE32(v);
        }
        /** @brief Reads a little-endian uint32 from the first 4 bytes of @p source. */
        [[nodiscard]] static uint32_t ReadUInt32LittleEndian(System::ReadOnlySpan<uint8_t> source) {
            checkSize(source.getLengthProperty(), 4);
            uint32_t v; std::memcpy(&v, source.getPointer(), 4); return fromLE32u(v);
        }
        /** @brief Reads a little-endian int64 from the first 8 bytes of @p source. */
        [[nodiscard]] static int64_t ReadInt64LittleEndian(System::ReadOnlySpan<uint8_t> source) {
            checkSize(source.getLengthProperty(), 8);
            int64_t v; std::memcpy(&v, source.getPointer(), 8); return fromLE64(v);
        }
        /** @brief Reads a little-endian uint64 from the first 8 bytes of @p source. */
        [[nodiscard]] static uint64_t ReadUInt64LittleEndian(System::ReadOnlySpan<uint8_t> source) {
            checkSize(source.getLengthProperty(), 8);
            uint64_t v; std::memcpy(&v, source.getPointer(), 8); return fromLE64u(v);
        }

        // -----------------------------------------------------------------------
        // Read — big-endian
        // -----------------------------------------------------------------------

        /** @brief Reads a big-endian int16 from the first 2 bytes of @p source. */
        [[nodiscard]] static int16_t ReadInt16BigEndian(System::ReadOnlySpan<uint8_t> source) {
            checkSize(source.getLengthProperty(), 2);
            int16_t v; std::memcpy(&v, source.getPointer(), 2); return fromBE16(v);
        }
        /** @brief Reads a big-endian uint16 from the first 2 bytes of @p source. */
        [[nodiscard]] static uint16_t ReadUInt16BigEndian(System::ReadOnlySpan<uint8_t> source) {
            checkSize(source.getLengthProperty(), 2);
            uint16_t v; std::memcpy(&v, source.getPointer(), 2); return fromBE16u(v);
        }
        /** @brief Reads a big-endian int32 from the first 4 bytes of @p source. */
        [[nodiscard]] static int32_t ReadInt32BigEndian(System::ReadOnlySpan<uint8_t> source) {
            checkSize(source.getLengthProperty(), 4);
            int32_t v; std::memcpy(&v, source.getPointer(), 4); return fromBE32(v);
        }
        /** @brief Reads a big-endian uint32 from the first 4 bytes of @p source. */
        [[nodiscard]] static uint32_t ReadUInt32BigEndian(System::ReadOnlySpan<uint8_t> source) {
            checkSize(source.getLengthProperty(), 4);
            uint32_t v; std::memcpy(&v, source.getPointer(), 4); return fromBE32u(v);
        }
        /** @brief Reads a big-endian int64 from the first 8 bytes of @p source. */
        [[nodiscard]] static int64_t ReadInt64BigEndian(System::ReadOnlySpan<uint8_t> source) {
            checkSize(source.getLengthProperty(), 8);
            int64_t v; std::memcpy(&v, source.getPointer(), 8); return fromBE64(v);
        }
        /** @brief Reads a big-endian uint64 from the first 8 bytes of @p source. */
        [[nodiscard]] static uint64_t ReadUInt64BigEndian(System::ReadOnlySpan<uint8_t> source) {
            checkSize(source.getLengthProperty(), 8);
            uint64_t v; std::memcpy(&v, source.getPointer(), 8); return fromBE64u(v);
        }

        // -----------------------------------------------------------------------
        // Write — little-endian
        // -----------------------------------------------------------------------

        /** @brief Writes @p value as little-endian int16 into the first 2 bytes of @p destination. */
        static void WriteInt16LittleEndian(System::Span<uint8_t> destination, int16_t value) {
            checkSize(destination.getLengthProperty(), 2);
            int16_t v = toLE16(value); std::memcpy(destination.getPointer(), &v, 2);
        }
        /** @brief Writes @p value as little-endian uint16 into the first 2 bytes of @p destination. */
        static void WriteUInt16LittleEndian(System::Span<uint8_t> destination, uint16_t value) {
            checkSize(destination.getLengthProperty(), 2);
            uint16_t v = toLE16u(value); std::memcpy(destination.getPointer(), &v, 2);
        }
        /** @brief Writes @p value as little-endian int32 into the first 4 bytes of @p destination. */
        static void WriteInt32LittleEndian(System::Span<uint8_t> destination, int32_t value) {
            checkSize(destination.getLengthProperty(), 4);
            int32_t v = toLE32(value); std::memcpy(destination.getPointer(), &v, 4);
        }
        /** @brief Writes @p value as little-endian uint32 into the first 4 bytes of @p destination. */
        static void WriteUInt32LittleEndian(System::Span<uint8_t> destination, uint32_t value) {
            checkSize(destination.getLengthProperty(), 4);
            uint32_t v = toLE32u(value); std::memcpy(destination.getPointer(), &v, 4);
        }
        /** @brief Writes @p value as little-endian int64 into the first 8 bytes of @p destination. */
        static void WriteInt64LittleEndian(System::Span<uint8_t> destination, int64_t value) {
            checkSize(destination.getLengthProperty(), 8);
            int64_t v = toLE64(value); std::memcpy(destination.getPointer(), &v, 8);
        }
        /** @brief Writes @p value as little-endian uint64 into the first 8 bytes of @p destination. */
        static void WriteUInt64LittleEndian(System::Span<uint8_t> destination, uint64_t value) {
            checkSize(destination.getLengthProperty(), 8);
            uint64_t v = toLE64u(value); std::memcpy(destination.getPointer(), &v, 8);
        }

        // -----------------------------------------------------------------------
        // Write — big-endian
        // -----------------------------------------------------------------------

        /** @brief Writes @p value as big-endian int16 into the first 2 bytes of @p destination. */
        static void WriteInt16BigEndian(System::Span<uint8_t> destination, int16_t value) {
            checkSize(destination.getLengthProperty(), 2);
            int16_t v = toBE16(value); std::memcpy(destination.getPointer(), &v, 2);
        }
        /** @brief Writes @p value as big-endian uint16 into the first 2 bytes of @p destination. */
        static void WriteUInt16BigEndian(System::Span<uint8_t> destination, uint16_t value) {
            checkSize(destination.getLengthProperty(), 2);
            uint16_t v = toBE16u(value); std::memcpy(destination.getPointer(), &v, 2);
        }
        /** @brief Writes @p value as big-endian int32 into the first 4 bytes of @p destination. */
        static void WriteInt32BigEndian(System::Span<uint8_t> destination, int32_t value) {
            checkSize(destination.getLengthProperty(), 4);
            int32_t v = toBE32(value); std::memcpy(destination.getPointer(), &v, 4);
        }
        /** @brief Writes @p value as big-endian uint32 into the first 4 bytes of @p destination. */
        static void WriteUInt32BigEndian(System::Span<uint8_t> destination, uint32_t value) {
            checkSize(destination.getLengthProperty(), 4);
            uint32_t v = toBE32u(value); std::memcpy(destination.getPointer(), &v, 4);
        }
        /** @brief Writes @p value as big-endian int64 into the first 8 bytes of @p destination. */
        static void WriteInt64BigEndian(System::Span<uint8_t> destination, int64_t value) {
            checkSize(destination.getLengthProperty(), 8);
            int64_t v = toBE64(value); std::memcpy(destination.getPointer(), &v, 8);
        }
        /** @brief Writes @p value as big-endian uint64 into the first 8 bytes of @p destination. */
        static void WriteUInt64BigEndian(System::Span<uint8_t> destination, uint64_t value) {
            checkSize(destination.getLengthProperty(), 8);
            uint64_t v = toBE64u(value); std::memcpy(destination.getPointer(), &v, 8);
        }

    private:
        static void checkSize(int len, int needed) {
            if (len < needed) throw std::out_of_range("BinaryPrimitives: span too small");
        }

        // Little-endian helpers (no-op on LE architectures; byte-swap on BE)
        static int16_t  fromLE16(int16_t v)   { return leToHost16s(v); }
        static uint16_t fromLE16u(uint16_t v) { return leToHost16u(v); }
        static int32_t  fromLE32(int32_t v)   { return leToHost32s(v); }
        static uint32_t fromLE32u(uint32_t v) { return leToHost32u(v); }
        static int64_t  fromLE64(int64_t v)   { return leToHost64s(v); }
        static uint64_t fromLE64u(uint64_t v) { return leToHost64u(v); }

        static int16_t  toLE16(int16_t v)    { return leToHost16s(v); }
        static uint16_t toLE16u(uint16_t v)  { return leToHost16u(v); }
        static int32_t  toLE32(int32_t v)    { return leToHost32s(v); }
        static uint32_t toLE32u(uint32_t v)  { return leToHost32u(v); }
        static int64_t  toLE64(int64_t v)    { return leToHost64s(v); }
        static uint64_t toLE64u(uint64_t v)  { return leToHost64u(v); }

        // Big-endian helpers
        static int16_t  fromBE16(int16_t v)   { return beToHost16s(v); }
        static uint16_t fromBE16u(uint16_t v) { return beToHost16u(v); }
        static int32_t  fromBE32(int32_t v)   { return beToHost32s(v); }
        static uint32_t fromBE32u(uint32_t v) { return beToHost32u(v); }
        static int64_t  fromBE64(int64_t v)   { return beToHost64s(v); }
        static uint64_t fromBE64u(uint64_t v) { return beToHost64u(v); }

        static int16_t  toBE16(int16_t v)   { return beToHost16s(v); }
        static uint16_t toBE16u(uint16_t v) { return beToHost16u(v); }
        static int32_t  toBE32(int32_t v)   { return beToHost32s(v); }
        static uint32_t toBE32u(uint32_t v) { return beToHost32u(v); }
        static int64_t  toBE64(int64_t v)   { return beToHost64s(v); }
        static uint64_t toBE64u(uint64_t v) { return beToHost64u(v); }

        // Portable byte-swap
        static uint16_t bswap16(uint16_t v) {
            return static_cast<uint16_t>((v >> 8) | (v << 8));
        }
        static uint32_t bswap32(uint32_t v) {
            return ((v & 0xFF000000u) >> 24) | ((v & 0x00FF0000u) >> 8)
                 | ((v & 0x0000FF00u) << 8)  | ((v & 0x000000FFu) << 24);
        }
        static uint64_t bswap64(uint64_t v) {
            return ((uint64_t)bswap32(static_cast<uint32_t>(v)) << 32)
                 |  (uint64_t)bswap32(static_cast<uint32_t>(v >> 32));
        }

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        static int16_t  leToHost16s(int16_t v)  { uint16_t u; std::memcpy(&u,&v,2); u=bswap16(u); int16_t r; std::memcpy(&r,&u,2); return r; }
        static uint16_t leToHost16u(uint16_t v) { return bswap16(v); }
        static int32_t  leToHost32s(int32_t v)  { uint32_t u; std::memcpy(&u,&v,4); u=bswap32(u); int32_t r; std::memcpy(&r,&u,4); return r; }
        static uint32_t leToHost32u(uint32_t v) { return bswap32(v); }
        static int64_t  leToHost64s(int64_t v)  { uint64_t u; std::memcpy(&u,&v,8); u=bswap64(u); int64_t r; std::memcpy(&r,&u,8); return r; }
        static uint64_t leToHost64u(uint64_t v) { return bswap64(v); }

        static int16_t  beToHost16s(int16_t v)  { return v; }
        static uint16_t beToHost16u(uint16_t v) { return v; }
        static int32_t  beToHost32s(int32_t v)  { return v; }
        static uint32_t beToHost32u(uint32_t v) { return v; }
        static int64_t  beToHost64s(int64_t v)  { return v; }
        static uint64_t beToHost64u(uint64_t v) { return v; }
#else
        // Little-endian (x86/x64/ARM default) — memcpy is already correct
        static int16_t  leToHost16s(int16_t v)  { return v; }
        static uint16_t leToHost16u(uint16_t v) { return v; }
        static int32_t  leToHost32s(int32_t v)  { return v; }
        static uint32_t leToHost32u(uint32_t v) { return v; }
        static int64_t  leToHost64s(int64_t v)  { return v; }
        static uint64_t leToHost64u(uint64_t v) { return v; }

        static int16_t  beToHost16s(int16_t v)  { uint16_t u; std::memcpy(&u,&v,2); u=bswap16(u); int16_t r; std::memcpy(&r,&u,2); return r; }
        static uint16_t beToHost16u(uint16_t v) { return bswap16(v); }
        static int32_t  beToHost32s(int32_t v)  { uint32_t u; std::memcpy(&u,&v,4); u=bswap32(u); int32_t r; std::memcpy(&r,&u,4); return r; }
        static uint32_t beToHost32u(uint32_t v) { return bswap32(v); }
        static int64_t  beToHost64s(int64_t v)  { uint64_t u; std::memcpy(&u,&v,8); u=bswap64(u); int64_t r; std::memcpy(&r,&u,8); return r; }
        static uint64_t beToHost64u(uint64_t v) { return bswap64(v); }
#endif
    };

} // namespace System::Buffers::Binary
