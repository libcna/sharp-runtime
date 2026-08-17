// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>

#include "System/OperationCanceledException.hpp"
#include "System/Threading/CancellationToken.hpp"

namespace System::Threading::Tasks {

    class Task;

    /**
     * @brief Represents an exception used to communicate task cancellation.
     *
     * C++ counterpart of .NET System.Threading.Tasks.TaskCanceledException.
     */
    class TaskCanceledException : public System::OperationCanceledException {
        /**
         * Owns a COPY of the cancelled task's handle, which is what removes the dangling
         * pointer SR-AUD-230 reported (ticket #1970).
         *
         * A `Task` is a handle over a `std::shared_ptr<State>`, so copying one does not copy the
         * task -- both handles refer to the SAME state. Holding a copy here therefore keeps that
         * state alive for as long as the exception is reachable **and** keeps reporting the live
         * status, which is exactly .NET's contract: `TaskCanceledException.Task` is a GC-tracked
         * reference to the same task object, not a snapshot of it.
         *
         * `std::shared_ptr<const Task>` rather than `std::optional<Task>` because `Task.hpp`
         * includes THIS header (`Task.hpp:25`), so the complete type is not available here and
         * a by-value member would be a circular include. `shared_ptr` needs the complete type
         * only where it is constructed, which is this type's `.cpp`.
         */
        std::shared_ptr<const Task> canceledTask_;

    public:
        /** @brief Initializes a new instance with the default message ("A task was canceled."). */
        TaskCanceledException();

        /** @brief Initializes a new instance with the specified error message. */
        explicit TaskCanceledException(const std::string& message);

        /** @brief Initializes a new instance with a message and an inner exception. */
        TaskCanceledException(const std::string& message, std::exception_ptr innerException);

        /** @brief Initializes a new instance with a message, an inner exception, and the triggering token. */
        TaskCanceledException(const std::string& message, std::exception_ptr innerException,
                               const System::Threading::CancellationToken& token);

        /**
         * @brief Initializes a new instance referencing the Task that was canceled.
         *
         * @param task The task that has been canceled; may be nullptr. **The pointer is not
         *        retained**: a copy of the handle is taken, so the caller may destroy @p task
         *        immediately afterwards.
         * @note Before ticket #1970 this stored @p task as a raw, non-owning pointer, and
         * `getTaskProperty()` handed it back with no validity check -- ASan reported
         * stack-use-after-scope when the referenced `Task` was local to the construction scope,
         * which is the ordinary case for a task that has just been cancelled and thrown out of.
         * It now takes a copy of the handle. Because a `Task` shares its state through a
         * `std::shared_ptr<State>`, that copy keeps the state alive **and** keeps observing it,
         * so `getTaskProperty()->getStatusProperty()` reports the live status exactly as .NET's
         * GC-tracked `TaskCanceledException.Task` does.
         */
        explicit TaskCanceledException(const Task* task);

        /**
         * @brief Gets the task associated with this exception, or nullptr if none.
         *
         * C++ counterpart of .NET TaskCanceledException.Task. The returned pointer is valid for
         * as long as this exception object is, because the exception owns the handle it points
         * at -- see the pointer constructor.
         */
        [[nodiscard]] const Task* getTaskProperty() const { return canceledTask_.get(); }
    };

} // namespace System::Threading::Tasks
