// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stdexcept>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Threading/BarrierPostPhaseException.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    /** Enables multiple tasks to cooperatively work on an algorithm in parallel through multiple phases. */
    class Barrier {
        intcs participantCount_;
        intcs remainingCount_;
        longcs phaseCount_ = 0;
        std::function<void(Barrier&)> postPhaseAction_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;

    public:
        /** Constructs a Barrier with the specified number of participants and an optional post-phase action. */
        explicit Barrier(intcs participantCount, std::function<void(Barrier&)> postPhaseAction = nullptr)
            : participantCount_(participantCount), remainingCount_(participantCount),
              postPhaseAction_(std::move(postPhaseAction)) {
            if (participantCount < 0 || participantCount > 32767)
                throw System::ArgumentOutOfRangeException("participantCount");
        }

        /** Returns the total number of participants. */
        [[nodiscard]] intcs  getParticipantCountProperty() const { return participantCount_; }
        /** Returns the current phase number. */
        [[nodiscard]] longcs getCurrentPhaseNumberProperty() const { std::unique_lock lock(mutex_); return phaseCount_; }

        /** Signals that a participant has reached the barrier and blocks until all participants have arrived. */
        void SignalAndWait() {
            std::unique_lock lock(mutex_);
            if (participantCount_ == 0)
                throw System::InvalidOperationException("The barrier has no registered participants.");
            --remainingCount_;
            if (remainingCount_ == 0) {
                FinishPhase(lock);
            } else {
                longcs myPhase = phaseCount_;
                cv_.wait(lock, [this, myPhase]{ return phaseCount_ > myPhase; });
            }
        }

        /** Notifies the barrier that there will be one additional participant; returns new participant count. */
        intcs AddParticipant() {
            std::unique_lock lock(mutex_);
            if (participantCount_ >= 32767)
                throw System::InvalidOperationException("Adding the specified number of participants would cause the Barrier's participants count to exceed the maximum allowed.");
            ++participantCount_;
            ++remainingCount_;
            return participantCount_;
        }

        /** Notifies the barrier that there will be one fewer participant. */
        void RemoveParticipant() {
            std::unique_lock lock(mutex_);
            if (participantCount_ == 0)
                throw System::ArgumentOutOfRangeException("participantCount");
            if (remainingCount_ == 0)
                throw System::InvalidOperationException("The number of participants to remove would result in a negative number of remaining participants for this phase.");
            --participantCount_;
            if (--remainingCount_ == 0) {
                FinishPhase(lock);
            }
        }

        /** Releases resources used by the Barrier. */
        void Dispose() {}

    private:
        /** Advances to the next phase, invoking the post-phase action and wrapping any exception it throws. */
        void FinishPhase(std::unique_lock<std::mutex>& lock) {
            ++phaseCount_;
            remainingCount_ = participantCount_;
            std::exception_ptr caught;
            if (postPhaseAction_) {
                try {
                    postPhaseAction_(*this);
                } catch (...) {
                    caught = std::current_exception();
                }
            }
            cv_.notify_all();
            (void)lock;
            if (caught)
                throw BarrierPostPhaseException(caught);
        }
    };

} // namespace System::Threading
