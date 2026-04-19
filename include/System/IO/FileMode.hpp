#pragma once

namespace System::IO
{
    /**
     * @brief Specifies how the operating system should open a file.
     *
     * This is a lightweight C++ port of the .NET FileMode enum.
     *
     * @note Status: PARTIAL
     */
    enum class FileMode
    {
        /**
         * @brief Creates a new file. If the file already exists, it is overwritten.
         *
         * @note Status: IMPLEMENTED
         */
        Create,

        /**
         * @brief Opens an existing file.
         *
         * @note Status: IMPLEMENTED
         */
        Open
    };
}