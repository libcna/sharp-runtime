#pragma once

#include "System/Exception.hpp"

namespace System::IO::IsolatedStorage
{
    /**
     * @brief Represents errors related to isolated storage operations.
     *
     * @note Status: IMPLEMENTED
     */
    class IsolatedStorageException : public System::Exception
    {
    public:
        /**
         * @brief Initializes a new instance of the IsolatedStorageException class.
         *
         * @param message Exception message.
         *
         * @note Status: IMPLEMENTED
         */
        explicit IsolatedStorageException(const std::string& message);
    };
}