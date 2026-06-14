// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <functional>
#include <memory>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

/**
 * @brief Base class for all delegate types.
 *
 * C++ counterpart of .NET System.Delegate. In .NET, Delegate is the abstract root
 * of all compiler-generated delegate types. In C++ there is no delegate syntax, so
 * this class provides the core runtime API: Combine, Remove, GetInvocationList,
 * HasSingleTarget, and type-erased void invocation via Invoke().
 *
 * DynamicInvoke always throws NotImplementedException — C++ has no late-bound
 * object[] invocation equivalent.
 *
 * When GetInvocationList() is called on a single-target delegate the object must
 * already be heap-allocated and owned by a std::shared_ptr (std::bad_weak_ptr is
 * thrown otherwise).
 */
class Delegate : public std::enable_shared_from_this<Delegate> {
public:
    /** @brief Type-erased void callable stored in single-target delegates. */
    using ErasedInvoke = std::function<void()>;

private:
    struct MulticastTag {};

    ErasedInvoke                           invoke_;
    std::vector<std::shared_ptr<Delegate>> invocationList_;

    Delegate(MulticastTag, std::vector<std::shared_ptr<Delegate>> list)
        : invocationList_(std::move(list)) {}

public:
    /** @brief Constructs an empty (no-op) delegate. */
    Delegate() = default;

    /**
     * @brief Constructs a single-target delegate wrapping the given callable.
     * @param invoke Type-erased callable to invoke via Invoke().
     */
    explicit Delegate(ErasedInvoke invoke) : invoke_(std::move(invoke)) {}

    virtual ~Delegate() = default;

    // -------------------------------------------------------------------------

    /**
     * @brief Invokes all targets in the invocation list in order.
     * For a single-target delegate, invokes the wrapped callable.
     */
    virtual void Invoke() const;

    /** @brief Calls Invoke(). */
    void operator()() const { Invoke(); }

    // -------------------------------------------------------------------------

    /**
     * @brief Gets a value indicating whether the delegate has a single invocation target.
     * @return true if there is at most one target.
     */
    [[nodiscard]] bool getHasSingleTargetProperty() const;

    /**
     * @brief Returns the invocation list.
     * For multicast delegates this returns the stored list.
     * For a single-target delegate this returns a one-element vector containing
     * a shared_ptr to this object (requires that this is managed by a shared_ptr).
     */
    [[nodiscard]] std::vector<std::shared_ptr<Delegate>> GetInvocationList() const;

    /**
     * @brief Creates a shallow copy of this delegate.
     * @return A new shared_ptr<Delegate> with the same invoke target or invocation list.
     */
    [[nodiscard]] virtual std::shared_ptr<Delegate> Clone() const;

    /**
     * @brief Determines whether this delegate equals other.
     * Default uses object identity (pointer equality).
     * @return true if this and other are the same object instance.
     */
    [[nodiscard]] virtual bool Equals(const Delegate& other) const { return this == &other; }

    /**
     * @brief Not implemented — always throws NotImplementedException.
     * C++ has no equivalent of .NET late-bound object[] invocation.
     */
    virtual std::any DynamicInvoke(const std::vector<std::any>& args);

    // -------------------------------------------------------------------------

    /**
     * @brief Concatenates the invocation lists of a and b.
     * @return Combined delegate; if either is null the other is returned.
     *         Returns null if both are null.
     */
    static std::shared_ptr<Delegate> Combine(
        std::shared_ptr<Delegate> a, std::shared_ptr<Delegate> b);

    /**
     * @brief Concatenates the invocation lists of all delegates in the vector.
     * @return Combined delegate, or null if the vector is empty or all entries are null.
     */
    static std::shared_ptr<Delegate> Combine(
        const std::vector<std::shared_ptr<Delegate>>& delegates);

    /**
     * @brief Removes the last occurrence of value from source's invocation list.
     * @param source The delegate to remove from; null returns null.
     * @param value  The delegate to remove; null returns source unchanged.
     * @return New delegate without the removed entry, or null if the list becomes empty.
     *         Returns source unchanged (same pointer) if value was not found.
     */
    static std::shared_ptr<Delegate> Remove(
        std::shared_ptr<Delegate> source, std::shared_ptr<Delegate> value);

    /**
     * @brief Removes all occurrences of value from source's invocation list.
     * @return New delegate with all matching entries removed, or null if none remain.
     */
    static std::shared_ptr<Delegate> RemoveAll(
        std::shared_ptr<Delegate> source, std::shared_ptr<Delegate> value);

    // -------------------------------------------------------------------------

    bool operator==(const Delegate& o) const { return Equals(o); }
    bool operator!=(const Delegate& o) const { return !Equals(o); }
};

} // namespace System
