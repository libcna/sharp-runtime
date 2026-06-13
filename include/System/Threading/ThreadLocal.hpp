// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include "System/IDisposable.hpp"

namespace System::Threading {

    /// Provides thread-local storage of data.
    template<typename T>
    class ThreadLocal : public System::IDisposable {
        static thread_local T* storage_;
        std::function<T()> factory_;
        bool trackAllValues_ = false;

    public:
        /// Constructs a ThreadLocal with default-constructed values.
        ThreadLocal() = default;
        /// Constructs a ThreadLocal; the trackAllValues flag is accepted but not used.
        explicit ThreadLocal(bool trackAllValues) : trackAllValues_(trackAllValues) {}
        /// Constructs a ThreadLocal that calls valueFactory to create the initial value.
        explicit ThreadLocal(std::function<T()> valueFactory)
            : factory_(std::move(valueFactory)) {}
        /// Constructs a ThreadLocal with a value factory and a tracking flag.
        ThreadLocal(std::function<T()> valueFactory, bool trackAllValues)
            : factory_(std::move(valueFactory)), trackAllValues_(trackAllValues) {}

        /// Returns true if the value has been initialised for the current thread.
        [[nodiscard]] bool getIsValueCreatedProperty() const {
            return storage_ != nullptr;
        }

        /// Returns the value for the current thread, initialising it on first access.
        [[nodiscard]] T& getValueProperty() {
            if (!storage_) {
                storage_ = new T(factory_ ? factory_() : T{});
            }
            return *storage_;
        }

        /// Sets the value for the current thread.
        void setValueProperty(const T& v) {
            if (!storage_) storage_ = new T(v);
            else *storage_ = v;
        }

        /// Returns the value for the current thread (alias for getValueProperty).
        [[nodiscard]] T& Value() { return getValueProperty(); }

        /// Releases resources for the current thread's value.
        void Dispose() override {
            delete storage_;
            storage_ = nullptr;
        }
    };

    template<typename T>
    thread_local T* ThreadLocal<T>::storage_ = nullptr;

} // namespace System::Threading
