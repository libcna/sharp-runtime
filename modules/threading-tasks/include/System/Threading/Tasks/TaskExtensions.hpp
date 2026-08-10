// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <exception>
#include <memory>
#include <thread>

#include "System/ArgumentNullException.hpp"
#include "System/Threading/Tasks/TaskCompletionSource.hpp"

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
#  include "System/PlatformNotSupportedException.hpp"
#endif

namespace System::Threading::Tasks {

    /**
     * @brief Provides static helpers for working with nested Task instances.
     *
     * C++ counterpart of .NET System.Threading.Tasks.TaskExtensions.  C++ has no
     * extension-method syntax, so call these helpers as
     * `TaskExtensions::Unwrap(std::move(task))`.
     */
    class TaskExtensions {
        static bool isValid(const Task& task) {
            return static_cast<bool>(task.state_);
        }

        template<typename TResult>
        static bool isValid(const TaskT<TResult>& task) {
            return static_cast<bool>(task.state_);
        }

        static void completeProxy(TaskT<Task>& task,
                                  const std::shared_ptr<TaskCompletionSource<void>>& source) {
            try {
                Task inner = task.getResultProperty();
                if (!isValid(inner)) {
                    source->TrySetCanceled();
                    return;
                }

                try {
                    inner.Wait();
                    source->TrySetResult();
                } catch (...) {
                    if (inner.getIsCanceledProperty()) {
                        source->TrySetCanceled();
                    } else {
                        source->TrySetException(std::current_exception());
                    }
                }
            } catch (...) {
                if (task.getIsCanceledProperty()) {
                    source->TrySetCanceled();
                } else {
                    source->TrySetException(std::current_exception());
                }
            }
        }

        template<typename TResult>
        static void completeProxy(TaskT<TaskT<TResult>>& task,
                                  const std::shared_ptr<TaskCompletionSource<TResult>>& source) {
            try {
                TaskT<TResult> inner = task.getResultProperty();
                if (!isValid(inner)) {
                    source->TrySetCanceled();
                    return;
                }

                try {
                    source->TrySetResult(inner.getResultProperty());
                } catch (...) {
                    if (inner.getIsCanceledProperty()) {
                        source->TrySetCanceled();
                    } else {
                        source->TrySetException(std::current_exception());
                    }
                }
            } catch (...) {
                if (task.getIsCanceledProperty()) {
                    source->TrySetCanceled();
                } else {
                    source->TrySetException(std::current_exception());
                }
            }
        }

    public:
        /**
         * Creates a proxy Task for the async operation represented by a TaskT<Task>.
         *
         * C++ counterpart of .NET TaskExtensions.Unwrap(Task<Task>).  If the outer
         * task has already completed successfully, returns its inner Task directly.
         * A moved-from Task is this port's equivalent of a null inner .NET Task and
         * yields a canceled proxy, matching .NET's null-inner behavior.
         *
         * @param task The nested task to unwrap.
         * @return The inner task or a proxy which forwards its completion state.
         * @throws System::ArgumentNullException if @p task is moved-from.
         */
        static Task Unwrap(TaskT<Task> task) {
            if (!isValid(task)) {
                throw System::ArgumentNullException("task");
            }

            if (task.getIsCompletedSuccessfullyProperty()) {
                Task inner = task.getResultProperty();
                return isValid(inner)
                    ? inner
                    : Task::FromCanceled(System::Threading::CancellationToken::None());
            }

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
            throw System::PlatformNotSupportedException(
                "TaskExtensions::Unwrap requires pthreads (not available in Emscripten single-threaded build).");
#else
            auto source = std::make_shared<TaskCompletionSource<void>>();
            Task proxy = source->getTaskProperty();
            std::thread([task = std::move(task), source]() mutable {
                completeProxy(task, source);
            }).detach();
            return proxy;
#endif
        }

        /**
         * Creates a proxy TaskT for the async operation represented by a TaskT<TaskT>.
         *
         * C++ counterpart of .NET TaskExtensions.Unwrap<TResult>(Task<Task<TResult>>).
         * If the outer task has already completed successfully, returns its inner TaskT
         * directly. A moved-from inner TaskT yields a canceled proxy.
         *
         * @tparam TResult The result type produced by the inner task.
         * @param task The nested task to unwrap.
         * @return The inner task or a proxy which forwards its completion state.
         * @throws System::ArgumentNullException if @p task is moved-from.
         */
        template<typename TResult>
        static TaskT<TResult> Unwrap(TaskT<TaskT<TResult>> task) {
            if (!isValid(task)) {
                throw System::ArgumentNullException("task");
            }

            if (task.getIsCompletedSuccessfullyProperty()) {
                TaskT<TResult> inner = task.getResultProperty();
                return isValid(inner)
                    ? inner
                    : TaskT<TResult>::FromCanceled(System::Threading::CancellationToken::None());
            }

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
            throw System::PlatformNotSupportedException(
                "TaskExtensions::Unwrap requires pthreads (not available in Emscripten single-threaded build).");
#else
            auto source = std::make_shared<TaskCompletionSource<TResult>>();
            TaskT<TResult> proxy = source->getTaskProperty();
            std::thread([task = std::move(task), source]() mutable {
                completeProxy(task, source);
            }).detach();
            return proxy;
#endif
        }
    };

} // namespace System::Threading::Tasks
