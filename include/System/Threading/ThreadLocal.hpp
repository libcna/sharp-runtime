// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "System/IDisposable.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/ObjectDisposedException.hpp"

namespace System::Threading {

    /**
     * @brief Provides thread-local storage of data.
     *
     * C++ counterpart of .NET System.Threading.ThreadLocal<T>. Each instance owns an
     * independent per-thread slot; storage is keyed by instance identity (this pointer)
     * within a thread_local map, so distinct ThreadLocal<T> instances never share state.
     */
    template<typename T>
    class ThreadLocal : public System::IDisposable {
        static std::unordered_map<const ThreadLocal*, std::unique_ptr<T>>& storageMap() {
            static thread_local std::unordered_map<const ThreadLocal*, std::unique_ptr<T>> map;
            return map;
        }

        // Verified against ThreadLocal.cs's GetValueSlow()/ThreadLocal_Value_RecursiveCallsToValue:
        // real .NET detects a value factory that reentrantly ends up accessing this same
        // instance's Value while the factory is still running and throws
        // InvalidOperationException rather than let it recurse. This port previously had no such
        // guard: factory_() ran before the map entry was inserted, so a reentrant
        // getValueProperty() call on the same thread would find no entry, invoke factory_() again,
        // and recurse until a (uncatchable) stack overflow.
        static std::unordered_set<const ThreadLocal*>& inProgress() {
            static thread_local std::unordered_set<const ThreadLocal*> set;
            return set;
        }

        std::function<T()> factory_;
        bool trackAllValues_ = false;
        bool disposed_ = false;

        void ThrowIfDisposed() const {
            if (disposed_) throw System::ObjectDisposedException("ThreadLocal`1");
        }

    public:
        /** Constructs a ThreadLocal with default-constructed values. */
        ThreadLocal() = default;
        /** Constructs a ThreadLocal; the trackAllValues flag is accepted but not used. */
        explicit ThreadLocal(bool trackAllValues) : trackAllValues_(trackAllValues) {}
        /** Constructs a ThreadLocal that calls valueFactory to create the initial value. */
        explicit ThreadLocal(std::function<T()> valueFactory)
            : factory_(std::move(valueFactory)) {}
        /** Constructs a ThreadLocal with a value factory and a tracking flag. */
        ThreadLocal(std::function<T()> valueFactory, bool trackAllValues)
            : factory_(std::move(valueFactory)), trackAllValues_(trackAllValues) {}

        /** Releases this instance's slot in the current thread's storage on destruction. */
        ~ThreadLocal() override { storageMap().erase(this); }

        /** Returns true if the value has been initialised for the current thread. */
        [[nodiscard]] bool getIsValueCreatedProperty() const {
            return storageMap().find(this) != storageMap().end();
        }

        /**
         * @brief Returns the value for the current thread, initialising it on first access.
         * @throws System::ObjectDisposedException if this instance has been disposed.
         * @throws System::InvalidOperationException if the value factory reentrantly accesses
         * this instance's value while still running.
         */
        [[nodiscard]] T& getValueProperty() {
            ThrowIfDisposed();
            auto& map = storageMap();
            auto it = map.find(this);
            if (it == map.end()) {
                auto& active = inProgress();
                if (!active.insert(this).second)
                    throw System::InvalidOperationException(
                        "ValueFactory attempted to access the Value property of this instance.");
                std::unique_ptr<T> value;
                try {
                    value = std::make_unique<T>(factory_ ? factory_() : T{});
                } catch (...) {
                    active.erase(this);
                    throw;
                }
                active.erase(this);
                it = map.emplace(this, std::move(value)).first;
            }
            return *it->second;
        }

        /**
         * @brief Sets the value for the current thread.
         * @throws System::ObjectDisposedException if this instance has been disposed.
         */
        void setValueProperty(const T& v) {
            ThrowIfDisposed();
            auto& map = storageMap();
            auto it = map.find(this);
            if (it == map.end()) map.emplace(this, std::make_unique<T>(v));
            else *it->second = v;
        }

        /** Returns the value for the current thread (alias for getValueProperty). */
        [[nodiscard]] T& Value() { return getValueProperty(); }

        /** Releases resources for the current thread's value. */
        void Dispose() override {
            disposed_ = true;
            storageMap().erase(this);
        }
    };

} // namespace System::Threading
