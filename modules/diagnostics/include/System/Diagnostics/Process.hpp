// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <memory>
#include <string>
#include <thread>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Diagnostics/ProcessStartInfo.hpp"

namespace System::Diagnostics {

    using SharpRuntime::intcs;

    /**
     * @brief Provides access to launching, waiting on, and terminating a local external process.
     *
     * Partial C++ counterpart of .NET System.Diagnostics.Process, scoped to the core
     * launch/wait/exit-code/kill lifecycle a game runtime typically needs (e.g. running an
     * external updater/installer, invoking a build/asset-processing tool, opening a URL via a
     * platform "open" helper) rather than real .NET's full systems-programming surface.
     *
     * @note Thread safety: captured-output snapshots and lazy exit-state observation are
     * synchronized (#2030). Lifecycle operations such as Start, Kill and WaitForExit are not a
     * general concurrent-use API; callers should still serialize those operations themselves.
     *
     * @note Redirected-output waiting follows .NET's split contract (#2029/#2032): the finite
     * timeout, HasExited, ExitCode and Kill doors never wait for reader EOF, while the
     * parameterless WaitForExit waits for complete output. Destruction cancels readers and joins
     * them within bounded poll slices. Restarting an instance still resolves its previous reader
     * threads before reusing their `std::thread` objects and can therefore wait for an inherited
     * pipe writer; see Start().
     *
     * @note Status: Partial, POSIX-only (uses fork()/execvpe()/waitpid() in the .cpp body, guarded
     * behind #ifdef so the public header stays portable; throws
     * System::PlatformNotSupportedException on Emscripten). Implemented: Start (instance and the
     * three static overloads), WaitForExit (blocking and timeout forms), Kill (a single process
     * or, on Linux, its transitive descendant tree -- see Kill(bool)), ExitCode, HasExited, Id,
     * GetCurrentProcess, and optional
     * captured-text stdout/stderr redirection (a deliberate simplification of real .NET's
     * Stream-based StandardOutput/StandardError -- see getStandardOutputTextProperty). Child
     * setup and exec failures are reported synchronously by Start() rather than being exposed as
     * a later exit code 127. The following surfaces are explicitly not implemented, all
     * deliberately out of scope for this pass: process enumeration
     * (GetProcesses/GetProcessById), memory/CPU/priority/module/thread introspection properties,
     * the Exited/OutputDataReceived/ErrorDataReceived event-based async I/O model, UseShellExecute
     * (Windows-shell-specific), and the .NET 10 Run/RunAsync/RunAndCaptureText helper family.
     */
    class Process {
    public:
        struct Impl;
    private:
        std::unique_ptr<Impl> impl_;

        Process();
        static void reapIfNeeded(Impl& impl);

    public:
        /**
         * @brief Destroys the wrapper without terminating or waiting for its associated child.
         *
         * Redirected readers are cancelled and joined within bounded poll slices (#2029), so
         * destruction no longer waits for the child's lifetime. A child not explicitly reaped
         * through WaitForExit, HasExited or Kill can still become a zombie until this process
         * exits; call WaitForExit() before destruction when the child must be reaped and its
         * captured output completed.
         */
        ~Process();
        Process(const Process&) = delete;
        Process& operator=(const Process&) = delete;
        Process(Process&&) noexcept;
        Process& operator=(Process&&) noexcept;

        /** @brief Gets or sets the properties to pass to the Start method. */
        [[nodiscard]] const ProcessStartInfo& getStartInfoProperty() const;
        void setStartInfoProperty(const ProcessStartInfo& value);

        /**
         * @brief Starts (or restarts) the process using getStartInfoProperty().
         *
         * Because the default constructor is private, every call on an existing instance is a
         * restart: the first launch always goes through one of the static Start overloads or
         * GetCurrentProcess(). A restart is permitted only once the previously started process
         * has exited; the captured standard-output and standard-error text is then reset, so
         * the properties describe the newly started process rather than accumulating across
         * restarts.
         *
         * @warning A restart first resolves the previous child's redirected reader threads, so
         * this call can wait while a descendant still holds the old pipe's write end. That join
         * is required before assigning new `std::thread` objects; it is the one restart-specific
         * exception to the bounded-reader policy documented by #2032.
         *
         * @return true if a process resource was started.
         * @throws System::InvalidOperationException if getStartInfoProperty()'s FileName is
         * empty, if the previously started process is still running, or if child setup or exec
         * fails; in the last case its message includes the executable name and native error
         * text.
         * @throws System::ArgumentException if an environment variable name is empty or
         * contains '='.
         */
        bool Start();

        /**
         * @brief Gets the native process identifier.
         * @throws System::InvalidOperationException if the process has not been started.
         */
        [[nodiscard]] intcs getIdProperty() const;

        /**
         * @brief Gets a value indicating whether the associated process has terminated.
         *
         * This is a prompt non-blocking poll: it may lazily reap the child and update cached exit
         * state under a lock, but it does not wait for redirected reader EOF (#2030/#2032).
         */
        [[nodiscard]] bool getHasExitedProperty() const;

        /**
         * @brief Gets the exit code returned by the associated process.
         *
         * @throws System::InvalidOperationException if the process has not exited yet.
         */
        [[nodiscard]] intcs getExitCodeProperty() const;

        /**
         * @brief Gets the captured standard output text.
         *
         * Returns a by-value snapshot taken under the same lock used by the pipe reader (#2030),
         * so it is safe to call while the child is running. A mid-run snapshot may naturally be
         * incomplete; call parameterless WaitForExit() first when complete output is required.
         *
         * The text is reset when the process is restarted, so it always describes the most
         * recently started process rather than accumulating across restarts.
         *
         * @throws System::InvalidOperationException unless RedirectStandardOutput was set before Start().
         */
        [[nodiscard]] std::string getStandardOutputTextProperty() const;

        /**
         * @brief Gets the captured standard error text.
         *
         * Returns a synchronized by-value snapshot, with the same completeness contract as
         * getStandardOutputTextProperty().
         *
         * @throws System::InvalidOperationException unless RedirectStandardError was set before Start().
         */
        [[nodiscard]] std::string getStandardErrorTextProperty() const;

        /**
         * @brief Immediately stops the associated process by sending SIGKILL to it alone.
         *
         * Descendants the child created are **not** signalled and keep running, reparented to
         * init. Does nothing if the process has already exited or was obtained from
         * GetCurrentProcess().
         *
         * Reaping after the signal does not wait for redirected reader EOF (#2032).
         */
        void Kill();

        /**
         * @brief Immediately stops the associated process, and optionally its descendant tree,
         * via SIGKILL.
         *
         * @param entireProcessTree When true, recursively stops and signals the child and its
         * transitive descendants; when false, signals only the direct child.
         *
         * @note The tree walk is Linux-specific and reads `/proc`. On a POSIX host without a
         * readable `/proc`, `entireProcessTree=true` degrades to killing the direct child. It
         * guards against killing a tree containing the current process and reports collected
         * failures (#2031).
         */
        void Kill(bool entireProcessTree);

        /**
         * @brief Blocks the calling thread until the associated process terminates.
         *
         * The wait is retried on EINTR, so a signal whose handler was installed without
         * SA_RESTART interrupts the underlying wait but not this call. Returns immediately for
         * a Process obtained from GetCurrentProcess(), which is not a child of itself.
         *
         * @note With redirected output this waits for **more** than the process: after the child
         * terminates it joins the reader threads so captured text is complete. It can therefore
         * outlast the direct child while a descendant retains an inherited pipe. The finite
         * timeout overload deliberately does not join those readers (#2032).
         *
         * @throws System::InvalidOperationException if the process has not been started.
         */
        void WaitForExit();

        /**
         * @brief Blocks up to @p milliseconds for the process to terminate.
         *
         * @param milliseconds The number of milliseconds to wait, or -1 (.NET's
         *        Timeout.Infinite) to wait indefinitely.
         * @return true if the process exited before the timeout; false if the timeout elapsed
         *         first, and false for a Process obtained from GetCurrentProcess().
         *
         * @note For a finite timeout, observing the child exit does not join redirected reader
         * threads, so the deadline remains authoritative. A `true` result can therefore precede
         * the final output bytes; call parameterless WaitForExit() when complete output is
         * required. Passing -1 delegates to that unbounded overload (#2032).
         * @throws System::ArgumentOutOfRangeException if @p milliseconds is less than -1.
         * @throws System::InvalidOperationException if the process has not been started.
         */
        bool WaitForExit(intcs milliseconds);

        /** @brief Starts a process resource using the specified start info. */
        [[nodiscard]] static Process Start(const ProcessStartInfo& startInfo);
        /** @brief Starts a process resource for the specified executable path. */
        [[nodiscard]] static Process Start(const std::string& fileName);
        /** @brief Starts a process resource for the specified executable path and argument string. */
        [[nodiscard]] static Process Start(const std::string& fileName, const std::string& arguments);

        /** @brief Returns a Process wrapping the currently running process. WaitForExit/Kill are unsupported on it (it is not a child of itself). */
        [[nodiscard]] static Process GetCurrentProcess();
    };

} // namespace System::Diagnostics
