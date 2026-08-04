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
     * @note Thread safety: a Process instance is **not** safe for concurrent use, and this is
     * stronger than the usual "no synchronisation" caveat -- getHasExitedProperty() is declared
     * const yet mutates the object's exit state, and the captured-output getters hand out a
     * reference into a buffer an internal thread is still writing. Confine an instance to one
     * thread. Ticket #2030 gates the repair.
     *
     * @note Blocking, when output is redirected: reaping the child also **joins** the internal
     * pipe-reader threads, and a reader cannot finish until every holder of the pipe's write end
     * closes it -- including a **grandchild** that inherited it and outlives the direct child.
     * Five public members reach that join and can therefore block for as long as such a holder
     * lives: getHasExitedProperty() (and getExitCodeProperty() through it), Kill() and
     * Kill(bool), the restart Start(), WaitForExit(intcs), and ~Process. Measured 2026-08-04
     * with an 8 s grandchild: 7,502 ms, 7,502 ms, 7,503 ms and 8,004 ms respectively
     * (`build-probe/2033_probe1_reader_join_entry_points.log`). Only WaitForExit(intcs) declares
     * a bound to exceed; the others simply take arbitrarily long. Removing this requires deciding
     * what a reader that cannot finish should do -- join, detach, or abandon the descriptor --
     * which is the policy ticket **#2029** gates (`docs/SystemDiagnosticsNamespaceReviewPlan.md`
     * §14.1); the bound violation is tracked as **#2032** and the disclosure as **#2033**. The
     * behaviour is pinned by permanent tests. A caller that must not block should not redirect
     * output, or should ensure the child does not leave the write end open in a descendant
     * (`exec`ing the real program from a shell wrapper is enough).
     *
     * @note Status: Partial, POSIX-only (uses fork()/execvpe()/waitpid() in the .cpp body, guarded
     * behind #ifdef so the public header stays portable; throws
     * System::PlatformNotSupportedException on Emscripten). Implemented: Start (instance and the
     * three static overloads), WaitForExit (blocking and timeout forms), Kill (single process
     * and process-GROUP via killpg -- see Kill(bool), which is not the full process tree its
     * parameter name suggests), ExitCode, HasExited, Id, GetCurrentProcess, and optional
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
         * @brief Destroys the wrapper. Does **not** reap or terminate the associated process.
         *
         * @warning This destructor has two known, opposite shortcomings, both deliberately
         * left unrepaired because the repair is a policy decision awaiting approval
         * (ticket #2029, `docs/SystemDiagnosticsNamespaceReviewPlan.md` §14.1). Both are
         * pinned by permanent tests so neither can change unnoticed.
         *
         * - **Without redirection:** destruction returns immediately and the child is never
         *   waited for, so once it exits it remains a **zombie** for the lifetime of this
         *   process.
         * - **With redirection:** destruction **blocks** until the child closes its standard
         *   output, because the internal reader threads are joined. That is the child's whole
         *   remaining lifetime, and is unbounded in general (2005 ms measured for a 2 s child).
         *
         * Call WaitForExit() before destroying a Process to avoid both.
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
         * @warning A restart first resolves the previous child, which with redirected output
         * joins its reader threads, so this call blocks for as long as any descendant of the
         * previous child still holds that pipe's write end (7,503 ms measured against an 8 s
         * grandchild). Tickets #2029 and #2033.
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
         * @warning This is **not** a cheap poll when output is redirected. Observing the exit
         * also reaps the child, which joins the internal reader threads, so the call blocks
         * until every holder of the pipe's write end has closed it -- a **grandchild** that
         * inherited it will hold it after the direct child is gone (7,502 ms measured against an
         * 8 s grandchild). It takes no timeout and so has no bound to exceed. See the class
         * note; ticket #2029 gates the reader-thread policy, #2033 this disclosure.
         *
         * @warning Declared const, yet it mutates the object's exit state (ticket #2030).
         */
        [[nodiscard]] bool getHasExitedProperty() const;

        /**
         * @brief Gets the exit code returned by the associated process.
         *
         * @warning Reached through getHasExitedProperty(), so it inherits that member's
         * redirected-output blocking behaviour verbatim.
         *
         * @throws System::InvalidOperationException if the process has not exited yet.
         */
        [[nodiscard]] intcs getExitCodeProperty() const;

        /**
         * @brief Gets the captured standard output text.
         *
         * @warning The returned reference denotes a buffer an internal reader thread appends
         * to while the child runs, so **reading it before WaitForExit() has returned is a data
         * race** and the contents may change under the caller (the same reference was measured
         * holding 4 bytes mid-run and 8 bytes after exit). Read it only after WaitForExit().
         * Making this safe requires returning by value, which changes the public return type,
         * so it awaits approval (ticket #2030, plan §14.2); the current shape is pinned by a
         * permanent test.
         *
         * The text is reset when the process is restarted, so it always describes the most
         * recently started process rather than accumulating across restarts.
         *
         * @throws System::InvalidOperationException unless RedirectStandardOutput was set before Start().
         */
        [[nodiscard]] const std::string& getStandardOutputTextProperty() const;

        /**
         * @brief Gets the captured standard error text.
         *
         * @warning Carries the same live-reference caveat as getStandardOutputTextProperty():
         * read it only after WaitForExit(). Ticket #2030.
         *
         * @throws System::InvalidOperationException unless RedirectStandardError was set before Start().
         */
        [[nodiscard]] const std::string& getStandardErrorTextProperty() const;

        /**
         * @brief Immediately stops the associated process by sending SIGKILL to it alone.
         *
         * Descendants the child created are **not** signalled and keep running, reparented to
         * init. Does nothing if the process has already exited or was obtained from
         * GetCurrentProcess().
         *
         * @warning "Immediately" describes the signal, not this call. With redirected output,
         * Kill first checks whether the child has already exited, and observing that reaps it
         * and joins the internal reader threads -- so the call blocks for as long as any
         * descendant still holds the pipe's write end (7,502 ms measured against an 8 s
         * grandchild), with no timeout parameter to bound it. See the class note; ticket #2029
         * gates the reader-thread policy, #2033 this disclosure.
         */
        void Kill();

        /**
         * @brief Immediately stops the associated process, and optionally its process group,
         * via SIGKILL.
         *
         * @param entireProcessTree When true, signals the child's **process group** with
         * `killpg` rather than only the child.
         *
         * @warning Despite the parameter's name this is **not** a full process-tree kill. A
         * descendant that called `setsid()` -- or otherwise left the child's process group --
         * has a different group and **survives** (measured). Widening this to the transitive
         * descendant set changes how many processes get killed and needs a Linux-specific
         * `/proc` walk, so it awaits approval (ticket #2031, plan §14.3); the current
         * behaviour is pinned by a permanent test.
         *
         * @warning Carries Kill()'s redirected-output blocking caveat verbatim: this call can
         * take arbitrarily long while a descendant holds the pipe's write end, and has no
         * timeout parameter. Tickets #2029 and #2033.
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
         * terminates it joins the internal reader threads, which cannot finish until every
         * holder of the pipe's write end has closed it. That is deliberate here -- it is what
         * makes the captured text complete when this call returns -- but it means the wait can
         * outlast the child by the lifetime of any descendant that inherited the pipe. The
         * timeout overload inherits the same behaviour and so can exceed its bound (#2032).
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
         * @warning When output is redirected, this call can exceed @p milliseconds: reaping
         * the child also joins the internal reader threads, and a reader cannot finish until
         * every holder of the pipe's write end closes it -- including a **grandchild** that
         * inherited it. Measured at 29,951 ms against a 5,000 ms bound. Repairing it requires
         * deciding the reader-thread policy that ticket #2029 gates, so it is tracked
         * separately as ticket #2032.
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
