//
// Created by robertvokac on 6/7/25.
//

#pragma once

#include <string>

#include "System/SystemException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when a requested method or operation
     * is not implemented.
     *
     * This is a partial C++ counterpart of the .NET
     * System::NotImplementedException type.
     *
     * @note Status: Partial.
     * @note HResult, serialization, and inner exception support
     * are not implemented here.
     */
    class NotImplementedException : public SystemException {
    public:
        /**
         * @brief Initializes a new instance of the NotImplementedException class
         * with a default message.
         */
        NotImplementedException();

        /**
         * @brief Initializes a new instance of the NotImplementedException class
         * with the specified error message.
         *
         * @param message A null-terminated character string that describes the error.
         */
        explicit NotImplementedException(const char* message);

        /**
         * @brief Initializes a new instance of the NotImplementedException class
         * with the specified error message.
         *
         * @param message A string that describes the error.
         */
        explicit NotImplementedException(const std::string& message);

    };

} // namespace System
