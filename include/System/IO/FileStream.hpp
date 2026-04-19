#pragma once

#include <fstream>
#include <string>

#include "System/IO/Stream.hpp"

namespace System::IO
{
    /**
     * @brief Represents a file-backed stream.
     *
     * This is a lightweight implementation used by TitleContainer and
     * other ported code requiring a Stream object.
     *
     * @note Status: IMPLEMENTED
     */
    class FileStream : public Stream
    {
    private:
        std::ifstream file;
        intcs length;

    public:
        /**
         * @brief Opens a file stream for reading.
         *
         * @param path File path.
         *
         * @note Status: IMPLEMENTED
         */
        explicit FileStream(const std::string& path);

        /**
         * @brief Destroys the FileStream instance.
         *
         * @note Status: IMPLEMENTED
         */
        ~FileStream() override;

        intcs Read(bytecs buffer[], intcs offset, intcs count) override;
        void Close() override;
        [[nodiscard]] intcs getLengthProperty() const override;

        /**
         * @brief Returns true if the file is open.
         *
         * @return True if open, otherwise false.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] bool IsOpen() const;
    };
}