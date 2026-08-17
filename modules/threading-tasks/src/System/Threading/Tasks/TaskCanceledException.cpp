// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Threading/Tasks/TaskCanceledException.hpp"
#include <memory>

#include "System/Threading/Tasks/Task.hpp"

namespace System::Threading::Tasks {

    namespace {
        constexpr const char* kDefaultMessage = "A task was canceled.";
    }

    TaskCanceledException::TaskCanceledException()
        : OperationCanceledException(kDefaultMessage)
    {
    }

    TaskCanceledException::TaskCanceledException(const std::string& message)
        : OperationCanceledException(message)
    {
    }

    TaskCanceledException::TaskCanceledException(const std::string& message, std::exception_ptr innerException)
        : OperationCanceledException(message, std::move(innerException))
    {
    }

    TaskCanceledException::TaskCanceledException(const std::string& message, std::exception_ptr innerException,
                                                  const System::Threading::CancellationToken& token)
        : OperationCanceledException(message, std::move(innerException), token)
    {
    }

    TaskCanceledException::TaskCanceledException(const Task* task)
        : OperationCanceledException(kDefaultMessage,
                                      task != nullptr ? task->getCancellationTokenProperty()
                                                       : System::Threading::CancellationToken::None())
        // Ticket #1970 / SR-AUD-230. A COPY of the handle, not the pointer: `Task` shares its
        // state through a std::shared_ptr<State>, so this keeps the state alive for as long as
        // the exception is reachable and keeps observing the live status -- .NET's contract. The
        // caller may destroy its own Task immediately after constructing this exception, which
        // is the ordinary case and the one ASan reported as stack-use-after-scope.
        , canceledTask_(task != nullptr ? std::make_shared<const Task>(*task) : nullptr)
    {
    }

} // namespace System::Threading::Tasks
