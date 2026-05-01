#pragma once

#include <filesystem>
#include <fstream>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/IO/FileMode.hpp"

namespace System::IO::IsolatedStorage
{
    /**
     * @brief Represents a file stream inside isolated storage.
     *
     * This is a lightweight practical port of .NET IsolatedStorageFileStream.
     *
     * @note Status: PARTIAL
     */
    class IsolatedStorageFileStream
    {
    private:
        /**
         * @brief Underlying file stream.
         *
         * @note Status: IMPLEMENTED
         */
        std::fstream stream;

        /**
         * @brief Full file path.
         *
         * @note Status: IMPLEMENTED
         */
        std::filesystem::path fullPath;

    public:
        /**
         * @brief Opens an isolated storage file stream.
         *
         * @param fullPath Full filesystem path.
         * @param mode File mode.
         *
         * @note Status: IMPLEMENTED
         */
        IsolatedStorageFileStream(
            const std::filesystem::path& fullPath,
            System::IO::FileMode mode);

        /**
         * @brief Closes the stream.
         *
         * @note Status: IMPLEMENTED
         */
        void Close();

        /**
         * @brief Reads bytes from the stream.
         *
         * @param buffer Destination buffer.
         * @param offset Offset in destination buffer.
         * @param count Maximum number of bytes to read.
         * @return Number of bytes read.
         *
         * @note Status: IMPLEMENTED
         */
        SharpRuntime::intcs Read(SharpRuntime::bytecs buffer[], SharpRuntime::intcs offset, SharpRuntime::intcs count);

        /**
         * @brief Writes bytes to the stream.
         *
         * @param buffer Source buffer.
         * @param offset Offset in source buffer.
         * @param count Number of bytes to write.
         *
         * @note Status: IMPLEMENTED
         */
        void Write(const SharpRuntime::bytecs buffer[], SharpRuntime::intcs offset, SharpRuntime::intcs count);

        /**
         * @brief Gets the length of the file in bytes.
         *
         * @return File length in bytes.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] SharpRuntime::intcs getLengthProperty();

        /**
         * @brief Returns true if the stream is open.
         *
         * @return True if open.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] bool IsOpen() const;
    };
}