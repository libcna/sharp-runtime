// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stdexcept>

namespace System::Threading {

    /// Enables multiple tasks to cooperatively work on an algorithm in parallel through multiple phases.
    class Barrier {
        int participantCount_;
        int remainingCount_;
        int64_t phaseCount_ = 0;
        std::function<void(Barrier&)> postPhaseAction_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;

    public:
        /// Constructs a Barrier with the specified number of participants and an optional post-phase action.
        explicit Barrier(int participantCount, std::function<void(Barrier&)> postPhaseAction = nullptr)
            : participantCount_(participantCount), remainingCount_(participantCount),
              postPhaseAction_(std::move(postPhaseAction)) {
            if (participantCount < 0) throw std::invalid_argument("participantCount must be >= 0.");
        }

        /// Returns the total number of participants.
        [[nodiscard]] int  getParticipantCountProperty() const { return participantCount_; }
        /// Returns the current phase number.
        [[nodiscard]] int64_t getCurrentPhaseNumberProperty() const { std::unique_lock lock(mutex_); return phaseCount_; }

        /// Signals that a participant has reached the barrier and blocks until all participants have arrived.
        void SignalAndWait() {
            std::unique_lock lock(mutex_);
            --remainingCount_;
            if (remainingCount_ == 0) {
                ++phaseCount_;
                remainingCount_ = participantCount_;
                cv_.notify_all();
                if (postPhaseAction_) postPhaseAction_(*this);
            } else {
                int64_t myPhase = phaseCount_;
                cv_.wait(lock, [this, myPhase]{ return phaseCount_ > myPhase; });
            }
        }

        /// Notifies the barrier that there will be one additional participant; returns new participant count.
        int AddParticipant() {
            std::unique_lock lock(mutex_);
            ++participantCount_;
            ++remainingCount_;
            return participantCount_;
        }

        /// Notifies the barrier that there will be one fewer participant.
        void RemoveParticipant() {
            std::unique_lock lock(mutex_);
            if (participantCount_ == 0) throw std::invalid_argument("No participants to remove.");
            --participantCount_;
            if (--remainingCount_ == 0) {
                ++phaseCount_;
                remainingCount_ = participantCount_;
                cv_.notify_all();
            }
        }

        /// Releases resources used by the Barrier.
        void Dispose() {}
    };

} // namespace System::Threading
