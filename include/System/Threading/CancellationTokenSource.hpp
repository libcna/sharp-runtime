// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <memory>
#include "System/Threading/CancellationToken.hpp"

namespace System::Threading {

    /**
     * @brief Signals to a CancellationToken that it should be cancelled.
     *
     * Partial C++ counterpart of .NET System.Threading.CancellationTokenSource.
     *
     * @note Status: Partial
     */
    class CancellationTokenSource {
        std::shared_ptr<std::atomic<bool>> flag_ = std::make_shared<std::atomic<bool>>(false);
        bool disposed_ = false;
    public:
        /// Initializes a new CancellationTokenSource.
        CancellationTokenSource() = default;

        /// Returns the CancellationToken associated with this source.
        [[nodiscard]] CancellationToken getTokenProperty() const {
            return CancellationToken(flag_);
        }

        /// Returns true if cancellation has been requested.
        [[nodiscard]] bool getIsCancellationRequestedProperty() const { return flag_->load(); }

        /// Signals cancellation to all linked CancellationToken holders.
        void Cancel() { flag_->store(true); }

        /// Releases resources used by this CancellationTokenSource.
        void Dispose() { disposed_ = true; }
    };

} // namespace System::Threading
