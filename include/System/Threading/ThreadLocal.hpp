// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include "System/IDisposable.hpp"

namespace System::Threading {

    template<typename T>
    class ThreadLocal : public System::IDisposable {
        static thread_local T* storage_;
        std::function<T()> factory_;
        bool trackAllValues_ = false;

    public:
        ThreadLocal() = default;
        explicit ThreadLocal(bool trackAllValues) : trackAllValues_(trackAllValues) {}
        explicit ThreadLocal(std::function<T()> valueFactory)
            : factory_(std::move(valueFactory)) {}
        ThreadLocal(std::function<T()> valueFactory, bool trackAllValues)
            : factory_(std::move(valueFactory)), trackAllValues_(trackAllValues) {}

        [[nodiscard]] bool getIsValueCreatedProperty() const {
            return storage_ != nullptr;
        }

        [[nodiscard]] T& getValueProperty() {
            if (!storage_) {
                storage_ = new T(factory_ ? factory_() : T{});
            }
            return *storage_;
        }

        void setValueProperty(const T& v) {
            if (!storage_) storage_ = new T(v);
            else *storage_ = v;
        }

        [[nodiscard]] T& Value() { return getValueProperty(); }

        void Dispose() override {
            delete storage_;
            storage_ = nullptr;
        }
    };

    template<typename T>
    thread_local T* ThreadLocal<T>::storage_ = nullptr;

} // namespace System::Threading
