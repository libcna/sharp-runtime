// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/EventHandler.hpp"
#include "System/TimeSpan.hpp"
#include "System/Timers/ElapsedEventArgs.hpp"

namespace System::Threading {
    class Timer;
}

namespace System::Timers {

    /**
     * @brief Generates recurring events in an application, raising Elapsed at a configurable interval.
     *
     * C++ counterpart of .NET System.Timers.Timer. Built on System::Threading::Timer.
     *
     * @note Reduced scope: does not derive from System::ComponentModel::Component (no designer/
     * ISite integration is meaningful here) and does not implement `SynchronizingObject`
     * (`ISynchronizeInvoke`-based marshaling is a WinForms/UI-thread concept with no equivalent
     * in this runtime — `Elapsed` is always raised directly from the timer's background thread,
     * matching how `System::Threading::Timer` itself already works here).
     *
     * @note Lifetime contract: the background timer callback captures `this` directly (see
     * Timer.cpp's `startTimerThread()`), and the underlying `System::Threading::Timer::Dispose()`
     * detaches its thread rather than joining it. Destroying (or `Close()`-ing) a `Timer` does
     * not guarantee an in-flight callback invocation has finished — a callback that is already
     * executing when the `Timer` is destroyed can dereference a dangling `this`. Callers must
     * ensure a `Timer` outlives any callback it might still be dispatching, e.g. by not
     * destroying it from within its own `Elapsed` handler and by providing external
     * synchronization if destruction can race with a pending tick. This is the same class of
     * this-capture-into-background-thread hazard documented on `Socket`'s async methods and
     * `ClientWebSocket`'s Send/ReceiveAsync — not redesigned here for the same reason: fixing it
     * needs a broader ownership change (e.g. `enable_shared_from_this`/`weak_ptr` in the
     * callback), out of scope for a single audit pass.
     */
    class Timer {
        double interval_ = 100;
        bool enabled_ = false;
        bool autoReset_ = true;
        bool initializing_ = false;
        bool delayedEnable_ = false;
        std::unique_ptr<System::Threading::Timer> timer_;
        std::shared_ptr<int> cookie_;

        void updateTimer();
        void startTimerThread();

    public:
        /**
         * @brief Event handler collection for the Elapsed event.
         *
         * **Exception boundary** (ticket #2154, SR-AUD-238). Handlers run on a background thread
         * whose entry point is a raw `std::thread`, where an escaping exception is `std::terminate`
         * by definition — before #2154 an ordinary handler that threw killed the whole process,
         * taking every other timer in it down as well. Any exception a handler throws is now
         * **caught and discarded**, matching .NET's `Timer.MyTimerCallback`.
         *
         * Two consequences a subscriber must know:
         *
         * - a handler failure is **invisible**: there is no log, no event and no status flag. A
         *   handler that needs to report a failure must do so itself, from inside the handler;
         * - the timer **keeps running**. A periodic timer whose handler throws on every tick loops
         *   silently at the configured interval rather than stopping.
         *
         * @note The sender passed to handlers is currently `nullptr` rather than the raising
         * timer (SR-AUD-239). That is not an oversight: `EventHandler<T>::Raise` types its sender
         * as `System::Object*`, this type does not derive from `System::Object`, and giving it that
         * base is an object-layout and vtable change (`sizeof` 104 → 112, non-polymorphic →
         * polymorphic) held by the blocked ticket #2155.
         */
        System::EventHandler<ElapsedEventArgs> Elapsed;

        /** @brief Constructs with a default 100ms interval, initially disabled. */
        Timer();

        /**
         * @brief Constructs with the given interval, in milliseconds.
         *
         * The accepted domain is `(0, INT32_MAX]`, and **`NaN` is rejected** (ticket #2156): all
         * three doors that write the interval share one check, because a value outside that domain
         * later reaches a floating-to-integral conversion that is undefined behaviour rather than a
         * diagnostic.
         *
         * @throws System::ArgumentException if @p interval is `NaN` or not in (0, INT32_MAX].
         */
        explicit Timer(double interval);

        /** @brief Constructs with the given interval. */
        explicit Timer(System::TimeSpan interval);

        ~Timer();

        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;

        /** @return true if Elapsed is raised repeatedly (every Interval) while Enabled. */
        [[nodiscard]] bool getAutoResetProperty() const { return autoReset_; }
        /** @brief Sets whether Elapsed is raised repeatedly while Enabled. */
        void setAutoResetProperty(bool value);

        /** @return true if the timer is currently running. */
        [[nodiscard]] bool getEnabledProperty() const { return enabled_; }
        /** @brief Starts (true) or stops (false) the timer. */
        void setEnabledProperty(bool value);

        /**
         * @return The interval, in milliseconds, at which to raise Elapsed.
         *
         * @note The constructor stores `std::ceil(interval)` while this setter stores the value
         * verbatim, so `Timer(0.5).getIntervalProperty()` is `1` but a timer whose interval was
         * *set* to `0.5` reports `0.5`. Both schedule the same 1 ms tick, because the schedule is
         * derived with `std::ceil` at that point. Recorded and pinned by a test rather than
         * changed: no finding names it, and either answer is a public observable change.
         */
        [[nodiscard]] double getIntervalProperty() const { return interval_; }
        /**
         * @brief Sets the interval, in milliseconds.
         *
         * Applies exactly the constructor's domain (ticket #2156). It previously checked only
         * `value <= 0`, so it accepted `+inf`, `2147483648` and `3e9` — three values the
         * constructor rejects — and both doors accepted `NaN`; every value in that gap then
         * reached an undefined float-to-integer conversion and surfaced, if at all, as an
         * `ArgumentOutOfRangeException` naming an internal parameter of a private dependency,
         * thrown from `Start()`.
         *
         * @throws System::ArgumentException if @p value is `NaN` or not in (0, INT32_MAX].
         */
        void setIntervalProperty(double value);

        /** @brief Starts raising the Elapsed event (equivalent to setEnabledProperty(true)). */
        void Start() { setEnabledProperty(true); }
        /** @brief Stops raising the Elapsed event (equivalent to setEnabledProperty(false)). */
        void Stop() { setEnabledProperty(false); }

        /** @brief Stops the timer and prepares for a batch of property changes (see EndInit()). */
        void BeginInit();
        /** @brief Applies any Enabled value set since BeginInit() and resumes normal operation. */
        void EndInit();

        /** @brief Stops and releases the underlying timer resources. */
        void Close();
        /** @brief Equivalent to Close(). */
        void Dispose() { Close(); }
    };

} // namespace System::Timers
