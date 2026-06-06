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
        CancellationTokenSource() = default;

        [[nodiscard]] CancellationToken getTokenProperty() const {
            return CancellationToken(flag_);
        }

        [[nodiscard]] bool getIsCancellationRequestedProperty() const { return flag_->load(); }

        void Cancel() { flag_->store(true); }

        void Dispose() { disposed_ = true; }
    };

} // namespace System::Threading
