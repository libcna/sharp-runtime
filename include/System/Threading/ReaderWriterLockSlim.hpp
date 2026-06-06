// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <shared_mutex>
#include "System/IDisposable.hpp"
#include "System/Threading/LockRecursionPolicy.hpp"

namespace System::Threading {

    class ReaderWriterLockSlim : public System::IDisposable {
        mutable std::shared_mutex mtx_;
        bool disposed_ = false;

    public:
        ReaderWriterLockSlim() = default;
        explicit ReaderWriterLockSlim(LockRecursionPolicy /*recursionPolicy*/) {}

        void EnterReadLock()  { mtx_.lock_shared(); }
        void ExitReadLock()   { mtx_.unlock_shared(); }
        bool TryEnterReadLock(int /*millisecondsTimeout*/) { return mtx_.try_lock_shared(); }

        void EnterWriteLock() { mtx_.lock(); }
        void ExitWriteLock()  { mtx_.unlock(); }
        bool TryEnterWriteLock(int /*millisecondsTimeout*/) { return mtx_.try_lock(); }

        void EnterUpgradeableReadLock()  { mtx_.lock_shared(); }
        void ExitUpgradeableReadLock()   { mtx_.unlock_shared(); }

        void Dispose() override { disposed_ = true; }

        [[nodiscard]] bool getIsReadLockHeldProperty() const  { return false; } // can't query
        [[nodiscard]] bool getIsWriteLockHeldProperty() const { return false; }
    };

} // namespace System::Threading
