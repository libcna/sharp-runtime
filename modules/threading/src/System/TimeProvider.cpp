// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/TimeProvider.hpp"

namespace {

    class SystemTimeProvider : public System::TimeProvider {
    public:
        SystemTimeProvider() = default;
    };

    // ITimer wrapper around System::Threading::Timer
    class SystemTimeProviderTimer : public System::Threading::ITimer {
        System::Threading::Timer timer_;
        // Ticket #1956 / cause T-G. Change() used to `return true;` unconditionally, so a
        // DISPOSED timer reported that it had been rescheduled although its worker was stopped.
        //
        // ITimer::Change is the ONE member of this ticket's group that must NOT throw: .NET's
        // Timer.Change opens `if (_canceled) { return false; }` (Timer.cs:533-542) -- a false
        // RETURN is the documented contract, and making it throw for symmetry with the wait
        // handles would contradict the interface this type implements. The design record
        // excluded it deliberately and the reference confirms that exclusion.
        //
        // The flag lives here, in this .cpp-local class, so no public type gains a member.
        bool disposed_ = false;
    public:
        SystemTimeProviderTimer(System::Threading::TimerCallback cb, void* state,
                                System::TimeSpan dueTime, System::TimeSpan period)
            : timer_(std::move(cb), state,
                     static_cast<SharpRuntime::intcs>(dueTime.getTotalMillisecondsProperty()),
                     static_cast<SharpRuntime::intcs>(period.getTotalMillisecondsProperty()))
        {}

        bool Change(System::TimeSpan dueTime, System::TimeSpan period) override {
            if (disposed_) return false;   // Timer.cs:539-542
            timer_.Change(
                static_cast<SharpRuntime::intcs>(dueTime.getTotalMillisecondsProperty()),
                static_cast<SharpRuntime::intcs>(period.getTotalMillisecondsProperty()));
            return true;
        }

        /** Idempotent, matching .NET: a second Dispose is defined and harmless. */
        void Dispose() override {
            if (disposed_) return;
            disposed_ = true;
            timer_.Dispose();
        }
    };

} // anonymous namespace

namespace System {

    TimeProvider& TimeProvider::getSystemProperty() {
        static SystemTimeProvider instance;
        return instance;
    }

    std::unique_ptr<Threading::ITimer> TimeProvider::CreateTimer(
        Threading::TimerCallback callback, void* state,
        TimeSpan dueTime, TimeSpan period)
    {
        return std::make_unique<SystemTimeProviderTimer>(
            std::move(callback), state, dueTime, period);
    }

} // namespace System
