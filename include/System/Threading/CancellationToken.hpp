// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <memory>

namespace System::Threading {

    /**
     * @brief Propagates notification that operations should be cancelled.
     *
     * Partial C++ counterpart of .NET System.Threading.CancellationToken.
     *
     * @note Status: Partial
     */
    class CancellationToken {
        std::shared_ptr<std::atomic<bool>> cancelled_;
    public:
        CancellationToken() : cancelled_(std::make_shared<std::atomic<bool>>(false)) {}
        explicit CancellationToken(std::shared_ptr<std::atomic<bool>> flag) : cancelled_(std::move(flag)) {}

        [[nodiscard]] bool getIsCancellationRequestedProperty() const { return cancelled_->load(); }

        void ThrowIfCancellationRequested() const;

        static const CancellationToken& None() {
            static CancellationToken none;
            return none;
        }
    };

} // namespace System::Threading
