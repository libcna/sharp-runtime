//
// Created by robertvokac on 6/7/25.
//

#include "System/NotImplementedException.hpp"

namespace System {

    namespace {
        constexpr const char* DefaultNotImplementedMessage =
            "The method or operation is not implemented.";
    }

    NotImplementedException::NotImplementedException()
        : SystemException(DefaultNotImplementedMessage) {
    }

    NotImplementedException::NotImplementedException(const char* message)
        : SystemException(message) {
    }

    NotImplementedException::NotImplementedException(const std::string& message)
        : SystemException(message) {
    }


} // namespace System
