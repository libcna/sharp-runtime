// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <mutex>

namespace System::Threading {

    /// A lightweight, non-reentrant mutual-exclusion lock (.NET 9 Lock type).
    class Lock {
        std::mutex mtx_;

    public:
        /// Acquires the lock, blocking until it is available.
        void Enter() { mtx_.lock(); }
        /// Releases the lock.
        void Exit()  { mtx_.unlock(); }
        /// Attempts to acquire the lock without blocking; returns true if successful.
        bool TryEnter() { return mtx_.try_lock(); }

        /// RAII scope helper that holds the lock for its lifetime.
        class Scope {
            Lock& lock_;
        public:
            /// Acquires the lock on construction.
            explicit Scope(Lock& lk) : lock_(lk) { lock_.Enter(); }
            /// Releases the lock on destruction.
            ~Scope() { lock_.Exit(); }
            /// Copying is not allowed.
            Scope(const Scope&) = delete;
            /// Copy assignment is not allowed.
            Scope& operator=(const Scope&) = delete;
        };

        /// Acquires the lock and returns a Scope RAII guard that releases it on destruction.
        Scope EnterScope() { return Scope(*this); }
    };

} // namespace System::Threading
