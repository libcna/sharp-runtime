#pragma once

#include <cstddef>

#include "CppDotNet/CppDotNetHelper.hpp"

namespace System::IO
{
    using CppDotNet::bytecs;
    using CppDotNet::intcs;

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
    };
}