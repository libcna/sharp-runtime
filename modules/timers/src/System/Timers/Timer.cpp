// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Timers/Timer.hpp"
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <utility>
#include "System/ArgumentException.hpp"
#include "System/DateTime.hpp"
#include "System/Threading/Timer.hpp"

namespace System::Timers {

    GetTypeNameCPP(Timer, "System.Timers.Timer")

struct Timer::CallbackLifetime {
    std::mutex mutex;
    std::condition_variable callbacksFinished;
    Timer* owner = nullptr;
    std::uint64_t generation = 0;
    std::size_t activeCallbacks = 0;
    bool destructionStarted = false;
};

std::atomic<Timer::BeforeArmTestHook> Timer::beforeArmTestHook_{nullptr};

namespace {
    constexpr double kMaxInterval = 2147483647.0; // int32 max, matching .NET's validation

    // Identifies the lifetime gate currently dispatching on this worker thread. Destruction from
    // an Elapsed handler is not a supported public pattern (a later handler would see a dangling
    // sender), but avoiding a self-wait here keeps that failure mode from becoming a deadlock and
    // lets the already-snapshotted callback unwind without touching Timer again.
    thread_local const void* currentCallbackLifetime = nullptr;

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

Timer::Timer() : callbackLifetime_(std::make_shared<CallbackLifetime>()) {}

Timer::Timer(double interval) : Timer() {
    validateInterval(interval, "interval");
    interval_ = std::ceil(interval);
}

Timer::Timer(System::TimeSpan interval) : Timer(interval.getTotalMillisecondsProperty()) {}

Timer::~Timer() {
    // Close() intentionally does not wait: .NET permits an already-queued Elapsed event to finish
    // after Close returns, and a handler must be able to call Close on its own timer. Destruction
    // has the stronger obligation. Invalidate pending entries, then wait until any callback that
    // already acquired this object has stopped using its members.
    stopTimerThread(true);
}

void Timer::setAutoResetProperty(bool value) {
    if (autoReset_.load() != value) {
        autoReset_.store(value);
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
    auto lifetime = callbackLifetime_;
    std::lock_guard<std::mutex> lock(lifetime->mutex);
    if (!timer_ || !enabled_.load() || lifetime->destructionStarted) return;
    auto i = static_cast<SharpRuntime::intcs>(std::ceil(interval_));
    timer_->Change(i, autoReset_.load() ? i : -1);
}

void Timer::startTimerThread() {
    auto i = static_cast<SharpRuntime::intcs>(std::ceil(interval_));
    auto lifetime = callbackLifetime_;
    std::uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(lifetime->mutex);
        // An Elapsed handler may call Close/Start while another thread is destroying the
        // Timer. The object remains alive until that handler leaves, but publishing a new
        // worker from inside it would outlive the destructor's first invalidation.
        if (lifetime->destructionStarted) {
            enabled_.store(false);
            return;
        }
        // Reserve a fresh callback generation, but do not publish this owner yet. The underlying
        // worker is constructed paused below and cannot enter until timer_ owns it and Change()
        // has completed under this same gate.
        lifetime->owner = nullptr;
        generation = ++lifetime->generation;
    }

    try {
        auto pendingTimer = std::make_unique<System::Threading::Timer>(
        [lifetime, generation](void* /*state*/) {
            Timer* owner = nullptr;
            {
                std::lock_guard<std::mutex> lock(lifetime->mutex);
                // A stopped/restarted timer can still have a detached worker carrying the old
                // callback. The generation rejects that pending work without reading Timer.
                if (lifetime->owner == nullptr || lifetime->generation != generation) return;
                owner = lifetime->owner;
                ++lifetime->activeCallbacks;
            }

            // Once activeCallbacks is incremented, an external destructor waits for this scope.
            // The guard owns only shared state and the callback performs no Timer access after
            // Elapsed.Raise returns, so Close() from inside the handler needs no self-join.
            struct ActiveCallback final {
                std::shared_ptr<CallbackLifetime> lifetime;
                const void* previousLifetime;

                explicit ActiveCallback(std::shared_ptr<CallbackLifetime> value)
                    : lifetime(std::move(value)), previousLifetime(currentCallbackLifetime) {
                    currentCallbackLifetime = lifetime.get();
                }

                ~ActiveCallback() {
                    currentCallbackLifetime = previousLifetime;
                    {
                        std::lock_guard<std::mutex> lock(lifetime->mutex);
                        --lifetime->activeCallbacks;
                    }
                    lifetime->callbacksFinished.notify_all();
                }
            } active(lifetime);

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
                const bool autoReset = owner->autoReset_.load();
                if (!autoReset) {
                    owner->enabled_.store(false);
                }
                ElapsedEventArgs args(System::DateTime::getNowProperty());
                // #2155: the raising timer, matching .NET's intervalElapsed(this, ...)
                // at Timer.cs:313. Reported nullptr until Timer gained the Object base.
                owner->Elapsed.Raise(owner, args);
            } catch (...) {
            }
        },
        nullptr, -1, -1);

        // The old construction armed the worker before std::make_unique returned. A 1 ms tick
        // could therefore enter an Elapsed handler which called Close() while the assignment to
        // timer_ was still in progress -- a unique_ptr data race, or a running timer stored after
        // Close had already reset the old null member. Store a paused worker first. Holding the
        // lifetime mutex across Change() also prevents an immediately-ready callback from
        // acquiring the owner until Change has completely returned.
        if (auto hook = beforeArmTestHook_.load(); hook != nullptr) hook();
        {
            std::lock_guard<std::mutex> lock(lifetime->mutex);
            if (lifetime->destructionStarted) {
                enabled_.store(false);
                return;
            }
            // A later Close(), possibly followed by a successful Start(), owns Enabled now.
            // The stale generation discards only its local pending worker and must not overwrite
            // the newer operation's state (#2417).
            if (lifetime->generation != generation) return;
            timer_ = std::move(pendingTimer);
            lifetime->owner = this;
            timer_->Change(i, autoReset_.load() ? i : -1);
        }
    } catch (...) {
        // Do not leave THIS failed generation looking enabled or publishing this object through
        // the gate. A stale Start can fail after a concurrent Close()+Start() has already
        // published a newer generation; in that case it owns only its local pendingTimer and
        // must not steal the newer timer_ or clear its Enabled state (#2417).
        std::unique_ptr<System::Threading::Timer> failedTimer;
        {
            std::lock_guard<std::mutex> lock(lifetime->mutex);
            if (lifetime->generation == generation) {
                lifetime->owner = nullptr;
                ++lifetime->generation;
                failedTimer = std::move(timer_);
                enabled_.store(false);
            }
        }
        failedTimer.reset();
        throw;
    }
}

void Timer::stopTimerThread(bool waitForCallbacks) {
    auto lifetime = callbackLifetime_;
    std::unique_ptr<System::Threading::Timer> stoppedTimer;
    std::unique_lock<std::mutex> lock(lifetime->mutex);
    if (waitForCallbacks) lifetime->destructionStarted = true;
    lifetime->owner = nullptr;
    ++lifetime->generation;

    // A worker that has not entered our gate will now reject its generation. During external
    // destruction, wait before moving timer_: an already-entered Elapsed handler is allowed to
    // call Close/Stop and therefore may move that same member. Every timer_ read/write happens
    // under this gate; the detached Threading::Timer is destroyed only after the gate is released.
    // The destroy-from-handler branch remains unsupported and cannot wait for itself.
    if (waitForCallbacks && currentCallbackLifetime != lifetime.get()) {
        lifetime->callbacksFinished.wait(lock, [&] { return lifetime->activeCallbacks == 0; });
    }
    stoppedTimer = std::move(timer_);
    lock.unlock();
    stoppedTimer.reset();
}

void Timer::setEnabledProperty(bool value) {
    if (initializing_) {
        delayedEnable_ = value;
        return;
    }
    if (enabled_.load() == value) return;

    if (!value) {
        stopTimerThread(false);
        enabled_.store(false);
    } else {
        enabled_.store(true);
        bool hasTimer = false;
        {
            std::lock_guard<std::mutex> lock(callbackLifetime_->mutex);
            hasTimer = static_cast<bool>(timer_);
        }
        if (!hasTimer) {
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
    stopTimerThread(false);
    initializing_ = false;
    delayedEnable_ = false;
    enabled_.store(false);
}

} // namespace System::Timers
