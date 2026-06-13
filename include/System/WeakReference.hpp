// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>

namespace System {

    /// <summary>
    /// Non-generic weak reference. Holds std::weak_ptr&lt;void&gt; internally.
    /// TrackResurrection is accepted for API compatibility but ignored in this C++ port.
    /// </summary>
    class WeakReference {
        std::weak_ptr<void> weakPtr_;

    public:
        /// Constructs an empty WeakReference (target is nullptr).
        WeakReference() = default;

        /// Constructs a WeakReference pointing to @p target.
        /// @param target Shared pointer to track.
        /// @param trackResurrection Ignored in this port.
        explicit WeakReference(std::shared_ptr<void> target, bool /*trackResurrection*/ = false)
            : weakPtr_(target) {}

        /// Returns true if the referenced object has not yet been destroyed.
        [[nodiscard]] bool getIsAliveProperty() const noexcept {
            return !weakPtr_.expired();
        }

        /// Returns the referenced object as a shared_ptr, or nullptr if it has been destroyed.
        [[nodiscard]] std::shared_ptr<void> getTargetProperty() const noexcept {
            return weakPtr_.lock();
        }

        /// Updates the weak reference to point to @p value.
        /// @param value New target.
        void setTargetProperty(std::shared_ptr<void> value) noexcept {
            weakPtr_ = value;
        }

        /// Tries to retrieve the target; returns true and sets @p target on success.
        /// @param target Output parameter set to the live object, or nullptr if expired.
        /// @return True if the target is still alive.
        [[nodiscard]] bool TryGetTarget(std::shared_ptr<void>& target) const noexcept {
            target = weakPtr_.lock();
            return target != nullptr;
        }
    };

    /// <summary>Generic typed weak reference backed by std::weak_ptr&lt;T&gt;.</summary>
    template<typename T>
    class WeakReferenceT {
        std::weak_ptr<T> weakPtr_;

    public:
        /// Constructs an empty typed WeakReference.
        WeakReferenceT() = default;
        /// Constructs a typed WeakReference pointing to @p target.
        explicit WeakReferenceT(std::shared_ptr<T> target) : weakPtr_(target) {}

        /// Tries to retrieve the typed target; returns true and sets @p target on success.
        /// @param target Output parameter set to the live object, or nullptr if expired.
        /// @return True if the target is still alive.
        [[nodiscard]] bool TryGetTarget(std::shared_ptr<T>& target) const noexcept {
            target = weakPtr_.lock();
            return target != nullptr;
        }

        /// Updates the weak reference to point to @p target.
        void SetTarget(std::shared_ptr<T> target) noexcept { weakPtr_ = target; }

        /// Returns true if the referenced object has not yet been destroyed.
        [[nodiscard]] bool getIsAliveProperty() const noexcept { return !weakPtr_.expired(); }
    };

} // namespace System
