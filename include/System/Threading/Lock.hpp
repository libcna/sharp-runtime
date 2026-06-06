// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <mutex>

namespace System::Threading {

    // .NET 9 Lock type — a lightweight, non-reentrant mutex wrapper.
    class Lock {
        std::mutex mtx_;

    public:
        void Enter() { mtx_.lock(); }
        void Exit()  { mtx_.unlock(); }
        bool TryEnter() { return mtx_.try_lock(); }

        // RAII scope helper (mirrors Lock.Scope in .NET 9).
        class Scope {
            Lock& lock_;
        public:
            explicit Scope(Lock& lk) : lock_(lk) { lock_.Enter(); }
            ~Scope() { lock_.Exit(); }
            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;
        };

        Scope EnterScope() { return Scope(*this); }
    };

} // namespace System::Threading
