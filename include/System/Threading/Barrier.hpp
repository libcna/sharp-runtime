// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdexcept>

namespace System::Threading {

    class Barrier {
        int participantCount_;
        int remainingCount_;
        long phaseCount_ = 0;
        std::function<void(Barrier&)> postPhaseAction_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;

    public:
        explicit Barrier(int participantCount, std::function<void(Barrier&)> postPhaseAction = nullptr)
            : participantCount_(participantCount), remainingCount_(participantCount),
              postPhaseAction_(std::move(postPhaseAction)) {
            if (participantCount < 0) throw std::invalid_argument("participantCount must be >= 0.");
        }

        [[nodiscard]] int  getParticipantCountProperty() const { return participantCount_; }
        [[nodiscard]] long getCurrentPhaseNumberProperty() const { std::unique_lock lock(mutex_); return phaseCount_; }

        void SignalAndWait() {
            std::unique_lock lock(mutex_);
            --remainingCount_;
            if (remainingCount_ == 0) {
                ++phaseCount_;
                remainingCount_ = participantCount_;
                cv_.notify_all();
                if (postPhaseAction_) postPhaseAction_(*this);
            } else {
                long myPhase = phaseCount_;
                cv_.wait(lock, [this, myPhase]{ return phaseCount_ > myPhase; });
            }
        }

        int AddParticipant() {
            std::unique_lock lock(mutex_);
            ++participantCount_;
            ++remainingCount_;
            return participantCount_;
        }

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

        void Dispose() {}
    };

} // namespace System::Threading
