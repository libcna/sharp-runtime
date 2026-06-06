// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cstddef>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO
{
    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;

    /**
     * @brief Represents a readable stream.
     *
     * This is a lightweight subset of the .NET Stream API intended for
     * practical use in source ports.
     *
     * @note Status: PARTIAL
     */
    class Stream
    {
    public:
        virtual ~Stream() = default;

        /**
         * @brief Reads bytes from the stream into the specified buffer.
         *
         * @param buffer Destination buffer.
         * @param offset Offset in destination buffer.
         * @param count Maximum number of bytes to read.
         * @return Number of bytes actually read.
         *
         * @note Status: IMPLEMENTED
         */
        virtual intcs Read(bytecs buffer[], intcs offset, intcs count) = 0;

        /**
         * @brief Closes the stream.
         *
         * @note Status: IMPLEMENTED
         */
        virtual void Close() = 0;

        /**
         * @brief Gets the length of the stream in bytes.
         *
         * @return Stream length in bytes.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] virtual intcs getLengthProperty() const = 0;

        /**
         * @brief Writes a sequence of bytes to the current stream and
         * advances the current position by the number of bytes written.
         *
         * Default implementation throws NotSupportedException.
         *
         * @note Status: STUB (override in writable streams)
         */
        virtual void Write(const bytecs buffer[], intcs offset, intcs count);

        /**
         * @brief Writes a single byte to the current stream.
         *
         * @note Status: STUB (override in writable streams)
         */
        virtual void WriteByte(bytecs value);

        /**
         * @brief When overridden in a derived class, gets a value indicating
         * whether the stream supports writing.
         */
        [[nodiscard]] virtual bool getCanWriteProperty() const { return false; }

        /**
         * @brief When overridden in a derived class, gets a value indicating
         * whether the stream supports reading.
         */
        [[nodiscard]] virtual bool getCanReadProperty()  const { return true; }

        /**
         * @brief Flushes any buffered data to the underlying device.
         * Default is no-op.
         */
        virtual void Flush() {}
    };
}