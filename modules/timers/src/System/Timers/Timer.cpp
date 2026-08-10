// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Timers/Timer.hpp"
#include <cmath>
#include "System/ArgumentException.hpp"
#include "System/DateTime.hpp"
#include "System/Threading/Timer.hpp"

namespace System::Timers {

namespace {
    constexpr double kMaxInterval = 2147483647.0; // int32 max, matching .NET's validation

    // Ticket #2156 (cause TM-C). ONE domain check, shared by all three doors that write interval_.
    //
    // Before this, `Timer(double)` applied the upper bound and `setIntervalProperty` did not, so the
    // setter accepted +inf, 2147483648 and 3e9 -- three values the constructor rejects -- and both
    // doors accepted NaN, because `value <= 0` and `std::ceil(value) > kMaxInterval` are BOTH false
    // for a NaN. Every value in that gap then reached
    // `static_cast<SharpRuntime::intcs>(std::ceil(interval_))` below, a floating-to-integral
    // conversion of a value not representable in the destination: undefined behaviour per
    // [conv.fpint]/1, confirmed by
    //   Timer.cpp:51:56: runtime error: nan is outside the range of representable values of type 'int'
    // (build-probe/2153_probe2_fco.log). It surfaced -- when it surfaced at all -- as
    // ArgumentOutOfRangeException naming `dueTime`, an internal parameter of the PRIVATE
    // System::Threading::Timer dependency, thrown from Start() rather than from the door that
    // accepted the value.
    //
    // Note for future sanitizer claims in this repository: GCC's `-fsanitize=undefined` does NOT
    // include `float-cast-overflow`. It must be requested by name, which is why this survived.
    //
    // DELIBERATE NARROWING, disclosed: .NET's own setter is `if (value <= 0) throw`, which a NaN
    // also passes, so .NET may accept NaN and convert it to a defined value. `/rv` is absent here,
    // so that cannot be confirmed. Rejecting is chosen because undefined behaviour is not an
    // option, and it is the same answer #2146 gave for a negative length reaching zlib.
    //
    // The exception type, message shape and paramName of every value the constructor already
    // rejected are preserved exactly.
    void validateInterval(double value, const char* paramName) {
        if (std::isnan(value) || value <= 0 || std::ceil(value) > kMaxInterval) {
            throw System::ArgumentException("Invalid value for interval: " + std::to_string(value), paramName);
        }
    }
}

Timer::Timer() = default;

Timer::Timer(double interval) : Timer() {
    validateInterval(interval, "interval");
    interval_ = std::ceil(interval);
}

Timer::Timer(System::TimeSpan interval) : Timer(interval.getTotalMillisecondsProperty()) {}

Timer::~Timer() { Close(); }

void Timer::setAutoResetProperty(bool value) {
    if (autoReset_ != value) {
        autoReset_ = value;
        updateTimer();
    }
}

void Timer::setIntervalProperty(double value) {
    // Ticket #2156: the same domain the constructor applies, with this door's own paramName kept.
    validateInterval(value, "value");
    interval_ = value;
    updateTimer();
}

void Timer::updateTimer() {
    if (!timer_ || !enabled_) return;
    auto i = static_cast<SharpRuntime::intcs>(std::ceil(interval_));
    timer_->Change(i, autoReset_ ? i : -1);
}

void Timer::startTimerThread() {
    auto i = static_cast<SharpRuntime::intcs>(std::ceil(interval_));
    cookie_ = std::make_shared<int>(0);
    auto cookie = cookie_;
    bool autoReset = autoReset_;
    timer_ = std::make_unique<System::Threading::Timer>(
        [this, cookie, autoReset](void* /*state*/) {
            // Ticket #2154 (SR-AUD-238). Everything below runs on a background thread whose entry
            // point is `System::Threading::Timer::run`, invoked as the body of a raw std::thread.
            // An exception leaving a thread's entry function is std::terminate BY DEFINITION, so
            // before this try/catch an ordinary Elapsed handler that threw killed the process:
            // measured 7 of 7 SIGABRT across std::, sharp-runtime and non-std exception types,
            // one-shot and periodic, first fire and after three successful fires -- and in one case
            // an entirely UNRELATED second timer died with it, because the failure is process death
            // rather than thread death (build-probe/2153_probe1_before.log).
            //
            // The boundary belongs HERE, not in System::Threading::Timer::run. .NET's
            // System.Timers.Timer.MyTimerCallback wraps its event invocation in try/catch -- the
            // audit's own managed probe for SR-AUD-238 measured `throw_process=alive` -- while
            // .NET's System.Threading.Timer does NOT catch: an unhandled callback exception on a
            // thread-pool thread terminates the process there too. The layer below is already
            // right, and its behaviour is pinned by a test rather than changed.
            //
            // The catch is silent, matching .NET, and it deliberately covers the WHOLE body rather
            // than only Elapsed.Raise: nothing on this path may reach the thread entry point,
            // including a failure inside DateTime::getNowProperty or ElapsedEventArgs.
            //
            // Consequence, pinned by its own test: a periodic timer whose handler throws keeps
            // firing. That is what .NET does, and it is the point of the repair -- but a handler
            // that throws on every tick now loops silently, which the header documents.
            try {
                if (cookie != cookie_) return; // stale callback from a since-stopped timer
                if (!autoReset) {
                    enabled_ = false;
                }
                ElapsedEventArgs args(System::DateTime::getNowProperty());
                Elapsed.Raise(nullptr, args);
            } catch (...) {
            }
        },
        nullptr, i, autoReset_ ? i : -1);
}

void Timer::setEnabledProperty(bool value) {
    if (initializing_) {
        delayedEnable_ = value;
        return;
    }
    if (enabled_ == value) return;

    if (!value) {
        cookie_ = nullptr;
        timer_.reset();
        enabled_ = false;
    } else {
        enabled_ = true;
        if (!timer_) {
            startTimerThread();
        } else {
            updateTimer();
        }
    }
}

void Timer::BeginInit() {
    Close();
    initializing_ = true;
}

void Timer::EndInit() {
    initializing_ = false;
    setEnabledProperty(delayedEnable_);
}

void Timer::Close() {
    initializing_ = false;
    delayedEnable_ = false;
    enabled_ = false;
    cookie_ = nullptr;
    timer_.reset();
}

} // namespace System::Timers
