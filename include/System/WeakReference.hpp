// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>

namespace System {

    // Non-generic weak reference. Holds std::weak_ptr<void> internally.
    // TrackResurrection is ignored in this C++ port.
    class WeakReference {
        std::weak_ptr<void> weakPtr_;

    public:
        WeakReference() = default;

        explicit WeakReference(std::shared_ptr<void> target, bool /*trackResurrection*/ = false)
            : weakPtr_(target) {}

        [[nodiscard]] bool getIsAliveProperty() const noexcept {
            return !weakPtr_.expired();
        }

        [[nodiscard]] std::shared_ptr<void> getTargetProperty() const noexcept {
            return weakPtr_.lock();
        }

        void setTargetProperty(std::shared_ptr<void> value) noexcept {
            weakPtr_ = value;
        }

        [[nodiscard]] bool TryGetTarget(std::shared_ptr<void>& target) const noexcept {
            target = weakPtr_.lock();
            return target != nullptr;
        }
    };

    // Generic typed weak reference.
    template<typename T>
    class WeakReferenceT {
        std::weak_ptr<T> weakPtr_;

    public:
        WeakReferenceT() = default;
        explicit WeakReferenceT(std::shared_ptr<T> target) : weakPtr_(target) {}

        [[nodiscard]] bool TryGetTarget(std::shared_ptr<T>& target) const noexcept {
            target = weakPtr_.lock();
            return target != nullptr;
        }

        void SetTarget(std::shared_ptr<T> target) noexcept { weakPtr_ = target; }

        [[nodiscard]] bool getIsAliveProperty() const noexcept { return !weakPtr_.expired(); }
    };

} // namespace System
