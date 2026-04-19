#pragma once

#include <memory>
#include <string>

#include "System/IO/Stream.hpp"

namespace System::IO
{
    /**
     * @brief Reads characters from a Stream.
     *
     * This is a lightweight subset of the .NET StreamReader class.
     *
     * @note Status: PARTIAL
     */
    class StreamReader
    {
    private:
        Stream* stream;

    public:
        /**
         * @brief Initializes a StreamReader for the specified stream.
         *
         * The StreamReader does not own the stream.
         *
         * @param stream Stream to read from.
         *
         * @note Status: IMPLEMENTED
         */
        explicit StreamReader(Stream* stream);

        /**
         * @brief Reads all remaining text from the stream.
         *
         * @return Entire remaining stream contents as a string.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] std::string ReadToEnd() const;
    };
}