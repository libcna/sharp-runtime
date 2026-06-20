// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/IConvertible.hpp"
#include "System/IFormatProvider.hpp"
#include "System/InvalidCastException.hpp"
#include "System/TypeCode.hpp"

namespace System {

    /**
     * @brief Represents a nonexistent value. This class cannot be inherited.
     *
     * C++ counterpart of .NET System.DBNull (sealed).
     * The DBNull class represents a nonexistent value, for example as returned
     * from a database query whose result set contains no matching rows or fields.
     *
     * DBNull is a singleton; use DBNull::Value to obtain the sole instance.
     */
    class DBNull final : public IConvertible {
        DBNull() = default;

    public:
        /**
         * @brief Represents the sole instance of the DBNull class.
         *
         * C++ counterpart of .NET DBNull.Value.
         */
        static DBNull& Value() {
            static DBNull instance;
            return instance;
        }

        /** @brief Deleted copy constructor — DBNull is a singleton. */
        DBNull(const DBNull&) = delete;
        /** @brief Deleted copy assignment — DBNull is a singleton. */
        DBNull& operator=(const DBNull&) = delete;

        // -----------------------------------------------------------------------
        // IConvertible
        // -----------------------------------------------------------------------

        /**
         * @brief Gets the TypeCode for DBNull.
         *
         * C++ counterpart of .NET DBNull.GetTypeCode().
         * @return TypeCode::DBNull.
         */
        [[nodiscard]] TypeCode GetTypeCode() const override { return TypeCode::DBNull; }

        /**
         * @brief Not supported. Always throws InvalidCastException.
         *
         * C++ counterpart of .NET IConvertible.ToBoolean(IFormatProvider).
         * @throws InvalidCastException always.
         */
        [[nodiscard]] bool ToBoolean() const override {
            throw InvalidCastException("Cannot convert DBNull to Boolean.");
        }

        /**
         * @brief Not supported. Always throws InvalidCastException.
         *
         * C++ counterpart of .NET IConvertible.ToChar(IFormatProvider).
         * @throws InvalidCastException always.
         */
        [[nodiscard]] char ToChar() const override {
            throw InvalidCastException("Cannot convert DBNull to Char.");
        }

        /**
         * @brief Not supported. Always throws InvalidCastException.
         *
         * C++ counterpart of .NET IConvertible.ToSByte(IFormatProvider).
         * @throws InvalidCastException always.
         */
        [[nodiscard]] int8_t ToSByte() const override {
            throw InvalidCastException("Cannot convert DBNull to SByte.");
        }

        /**
         * @brief Not supported. Always throws InvalidCastException.
         *
         * C++ counterpart of .NET IConvertible.ToByte(IFormatProvider).
         * @throws InvalidCastException always.
         */
        [[nodiscard]] uint8_t ToByte() const override {
            throw InvalidCastException("Cannot convert DBNull to Byte.");
        }

        /**
         * @brief Not supported. Always throws InvalidCastException.
         *
         * C++ counterpart of .NET IConvertible.ToInt16(IFormatProvider).
         * @throws InvalidCastException always.
         */
        [[nodiscard]] int16_t ToInt16() const override {
            throw InvalidCastException("Cannot convert DBNull to Int16.");
        }

        /**
         * @brief Not supported. Always throws InvalidCastException.
         *
         * C++ counterpart of .NET IConvertible.ToUInt16(IFormatProvider).
         * @throws InvalidCastException always.
         */
        [[nodiscard]] uint16_t ToUInt16() const override {
            throw InvalidCastException("Cannot convert DBNull to UInt16.");
        }

        /**
         * @brief Not supported. Always throws InvalidCastException.
         *
         * C++ counterpart of .NET IConvertible.ToInt32(IFormatProvider).
         * @throws InvalidCastException always.
         */
        [[nodiscard]] int32_t ToInt32() const override {
            throw InvalidCastException("Cannot convert DBNull to Int32.");
        }

        /**
         * @brief Not supported. Always throws InvalidCastException.
         *
         * C++ counterpart of .NET IConvertible.ToUInt32(IFormatProvider).
         * @throws InvalidCastException always.
         */
        [[nodiscard]] uint32_t ToUInt32() const override {
            throw InvalidCastException("Cannot convert DBNull to UInt32.");
        }

        /**
         * @brief Not supported. Always throws InvalidCastException.
         *
         * C++ counterpart of .NET IConvertible.ToInt64(IFormatProvider).
         * @throws InvalidCastException always.
         */
        [[nodiscard]] int64_t ToInt64() const override {
            throw InvalidCastException("Cannot convert DBNull to Int64.");
        }

        /**
         * @brief Not supported. Always throws InvalidCastException.
         *
         * C++ counterpart of .NET IConvertible.ToUInt64(IFormatProvider).
         * @throws InvalidCastException always.
         */
        [[nodiscard]] uint64_t ToUInt64() const override {
            throw InvalidCastException("Cannot convert DBNull to UInt64.");
        }

        /**
         * @brief Not supported. Always throws InvalidCastException.
         *
         * C++ counterpart of .NET IConvertible.ToSingle(IFormatProvider).
         * @throws InvalidCastException always.
         */
        [[nodiscard]] float ToSingle() const override {
            throw InvalidCastException("Cannot convert DBNull to Single.");
        }

        /**
         * @brief Not supported. Always throws InvalidCastException.
         *
         * C++ counterpart of .NET IConvertible.ToDouble(IFormatProvider).
         * @throws InvalidCastException always.
         */
        [[nodiscard]] double ToDouble() const override {
            throw InvalidCastException("Cannot convert DBNull to Double.");
        }

        // -----------------------------------------------------------------------
        // ToString
        // -----------------------------------------------------------------------

        /**
         * @brief Returns an empty string, consistent with .NET DBNull.ToString().
         *
         * C++ counterpart of .NET DBNull.ToString().
         * @return An empty string.
         */
        [[nodiscard]] std::string ToString() const override { return ""; }

        /**
         * @brief Returns an empty string (format provider is ignored).
         *
         * C++ counterpart of .NET DBNull.ToString(IFormatProvider).
         * @param provider Ignored. Accepted for API compatibility.
         * @return An empty string.
         */
        [[nodiscard]] std::string ToString(const IFormatProvider* /*provider*/) const {
            return "";
        }
    };

} // namespace System
