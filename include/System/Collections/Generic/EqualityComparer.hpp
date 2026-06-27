// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <memory>
#include <stdexcept>

namespace System::Collections::Generic {

/**
 * @brief Provides a base class for equality comparison implementations.
 *
 * C++ counterpart of .NET System.Collections.Generic.EqualityComparer<T>.
 * Derive from this class and override Equals and GetHashCode to implement
 * a custom equality comparer for use with dictionaries, sets, and other collections.
 *
 * @tparam T The type of objects to compare.
 */
template<typename T>
class EqualityComparer {
public:
    /** @brief Virtual destructor for safe polymorphic destruction. */
    virtual ~EqualityComparer() = default;

    /**
     * @brief Determines whether two objects of type T are equal.
     *
     * C++ counterpart of .NET EqualityComparer<T>.Equals(T, T).
     * @param x The first object to compare.
     * @param y The second object to compare.
     * @return true if the specified objects are equal; otherwise false.
     */
    [[nodiscard]] virtual bool Equals(const T& x, const T& y) const = 0;

    /**
     * @brief Returns a hash code for the specified object.
     *
     * C++ counterpart of .NET EqualityComparer<T>.GetHashCode(T).
     * @param obj The object for which a hash code is to be returned.
     * @return A hash code for obj.
     */
    [[nodiscard]] virtual std::size_t GetHashCode(const T& obj) const = 0;

    /**
     * @brief Gets the default equality comparer for type T.
     *
     * C++ counterpart of .NET EqualityComparer<T>.Default.
     * Uses operator== for equality and std::hash<T> for hash codes.
     * @return A singleton reference to the default comparer.
     */
    static const EqualityComparer<T>& Default() {
        static struct DefaultImpl : EqualityComparer<T> {
            [[nodiscard]] bool   Equals(const T& x, const T& y) const override { return x == y; }
            [[nodiscard]] std::size_t GetHashCode(const T& obj) const override {
                return std::hash<T>{}(obj);
            }
        } instance;
        return instance;
    }

    /**
     * @brief Creates a custom equality comparer from lambda functions.
     *
     * C++ counterpart of .NET EqualityComparer<T>.Create(Func<T,T,bool>, Func<T,int>).
     * @param equals      A function that determines equality of two T values.
     * @param getHashCode A function that returns a hash code for a T value (optional;
     *                    throws std::runtime_error if called when not provided).
     * @return A shared_ptr to a new EqualityComparer wrapping the given functions.
     */
    static std::shared_ptr<EqualityComparer<T>> Create(
        std::function<bool(const T&, const T&)> equals,
        std::function<std::size_t(const T&)> getHashCode = nullptr)
    {
        struct DelegateImpl : EqualityComparer<T> {
            std::function<bool(const T&, const T&)> equals_;
            std::function<std::size_t(const T&)>    getHashCode_;
            DelegateImpl(std::function<bool(const T&, const T&)> eq,
                         std::function<std::size_t(const T&)> hc)
                : equals_(std::move(eq)), getHashCode_(std::move(hc)) {}
            [[nodiscard]] bool Equals(const T& x, const T& y) const override {
                return equals_(x, y);
            }
            [[nodiscard]] std::size_t GetHashCode(const T& obj) const override {
                if (!getHashCode_) throw std::runtime_error("GetHashCode not provided.");
                return getHashCode_(obj);
            }
        };
        return std::make_shared<DelegateImpl>(std::move(equals), std::move(getHashCode));
    }
};

} // namespace System::Collections::Generic
