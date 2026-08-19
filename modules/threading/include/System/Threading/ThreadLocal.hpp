// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "System/ArgumentNullException.hpp"
#include "System/IDisposable.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/ObjectDisposedException.hpp"

namespace System::Threading {

    /**
     * @brief Provides thread-local storage of data.
     *
     * C++ counterpart of .NET System.Threading.ThreadLocal<T>. Each instance owns an
     * independent per-thread slot, keyed by a monotonically-increasing per-instance ID (not
     * `this`) within a thread_local map, so distinct ThreadLocal<T> instances never share state.
     *
     * @note Keying by `this` (the previous implementation) meant that once an instance was
     * destroyed, every *other* thread's thread_local map retained a stale entry under that
     * now-dangling pointer value forever (a destructor can only clean up the destroying thread's
     * own map) -- if a new ThreadLocal<T> was later allocated at the same address (routine with
     * heap reuse), other threads would silently read/write the old, unrelated instance's stale
     * entry instead of the new instance's, corrupting data across two logically-unrelated
     * ThreadLocal<T> objects. IDs are never reused, so a new instance can never collide with a
     * stale entry left by a destroyed one; the trade-off is that another thread's stale entry
     * for a destroyed instance is not proactively cleaned up (a bounded per-ID leak until that
     * thread exits or next touches its own map) rather than corrupting future reads.
     */
    template<typename T>
    class ThreadLocal : public System::IDisposable {
        static std::atomic<std::uint64_t>& nextId() {
            static std::atomic<std::uint64_t> id{0};
            return id;
        }
        std::uint64_t id_ = nextId().fetch_add(1, std::memory_order_relaxed);

        // Ticket #1958 / SR-AUD-220. This was unique_ptr<T>. A tracked value must be reachable
        // from BOTH the owning thread's map and the instance-wide registry, and it must OUTLIVE
        // the owning thread -- .NET's LinkedSlot hangs off the ThreadLocal's own linked list and
        // GetValuesAsList walks that list (ThreadLocal.cs:437-456, 584-598), so a value survives
        // its thread exiting and is released when the ThreadLocal is. shared_ptr is the direct
        // counterpart; a weak_ptr registry would silently drop dead threads' values, which .NET
        // does not do.
        static std::unordered_map<std::uint64_t, std::shared_ptr<T>>& storageMap() {
            static thread_local std::unordered_map<std::uint64_t, std::shared_ptr<T>> map;
            return map;
        }

        // Verified against ThreadLocal.cs's GetValueSlow()/ThreadLocal_Value_RecursiveCallsToValue:
        // real .NET detects a value factory that reentrantly ends up accessing this same
        // instance's Value while the factory is still running and throws
        // InvalidOperationException rather than let it recurse. This port previously had no such
        // guard: factory_() ran before the map entry was inserted, so a reentrant
        // getValueProperty() call on the same thread would find no entry, invoke factory_() again,
        // and recurse until a (uncatchable) stack overflow.
        static std::unordered_set<std::uint64_t>& inProgress() {
            static thread_local std::unordered_set<std::uint64_t> set;
            return set;
        }

        std::function<T()> factory_;
        bool trackAllValues_ = false;
        // Ticket #1958 / SR-AUD-220. trackAllValues_ was ACCEPTED AND NEVER READ, and the type
        // exposed no Values property at all -- so a caller who asked for tracking got a silent
        // no-op and had no way to notice. These two members are what makes the flag mean
        // something. They are populated ONLY when trackAllValues_ is set, so an untracking
        // instance pays a mutex it never locks and nothing else.
        mutable std::mutex          trackedMutex_;
        std::vector<std::shared_ptr<T>> trackedValues_;

        /// Registers a newly created value with the instance-wide registry, when tracking.
        void trackIfRequested(const std::shared_ptr<T>& value) {
            if (!trackAllValues_) return;
            std::lock_guard<std::mutex> lk(trackedMutex_);
            trackedValues_.push_back(value);
        }
        // Ticket #1955 / cause T-A of docs/ThreadingNamespaceReviewPlan.md. This was an
        // ordinary `bool`, written by Dispose() and read by the guard below with no
        // synchronisation between them. Mixing synchronised and unsynchronised access to the
        // same object is a data race and therefore undefined behaviour, and ThreadSanitizer
        // confirmed it both at audit time and again in
        // build-probe/1955_probe1_shared_state_races.cpp. std::atomic<bool> is 1 byte and
        // 1-byte aligned on every supported target -- measured before and after in
        // build-probe/1955_probe1_layout_{before,after}.log -- so the flag's type change is
        // layout-neutral and the header stays consumer-compatible.
        std::atomic<bool> disposed_{false};

        void ThrowIfDisposed() const {
            if (disposed_.load(std::memory_order_acquire))
                throw System::ObjectDisposedException("ThreadLocal`1");
        }

        // Rejects an *empty* std::function factory at construction, as .NET's
        // `ThreadLocal(Func<T> valueFactory)` and `ThreadLocal(Func<T>, bool)` both do with
        // `ArgumentNullException.ThrowIfNull(valueFactory)`. Called from the constructor
        // *body*, after the member initializer has moved the argument in, because a check
        // written in the body against the parameter would inspect a moved-from function and
        // report every factory as empty. This is the same shape as Lazy<T>::requireFactory
        // (modules/core/include/System/Lazy.hpp), which #1867 established for CCF-011.
        //
        // Measured correction to SR-AUD-219 (ticket #1951, probe
        // build-probe/1951_probe1_threading_empty_callables.cpp): the finding states that a
        // stored empty factory "fails later with bad_function_call" at first value access.
        // It did not. getValueProperty() wrote `factory_ ? factory_() : T{}`, so an empty
        // factory silently produced a default-constructed value on every thread and no
        // exception was ever raised -- a *silent wrong value*, not a deferred crash. The
        // ternary is left in place because the two non-factory constructors legitimately
        // leave factory_ empty and must keep defaulting; this check is what stops a factory
        // constructor from reaching it.
        void requireFactory() const {
            if (!factory_) throw System::ArgumentNullException("valueFactory");
        }

    public:
        /** Constructs a ThreadLocal with default-constructed values. */
        ThreadLocal() = default;
        /** Constructs a ThreadLocal; the trackAllValues flag is accepted but not used. */
        explicit ThreadLocal(bool trackAllValues) : trackAllValues_(trackAllValues) {}
        /**
         * @brief Constructs a ThreadLocal that calls valueFactory to create the initial value.
         * @throws System::ArgumentNullException if @p valueFactory is an empty std::function.
         */
        explicit ThreadLocal(std::function<T()> valueFactory)
            : factory_(std::move(valueFactory)) { requireFactory(); }
        /**
         * @brief Constructs a ThreadLocal with a value factory and a tracking flag.
         * @throws System::ArgumentNullException if @p valueFactory is an empty std::function.
         */
        ThreadLocal(std::function<T()> valueFactory, bool trackAllValues)
            : factory_(std::move(valueFactory)), trackAllValues_(trackAllValues) { requireFactory(); }

        /** Releases this instance's slot in the current thread's storage on destruction. */
        ~ThreadLocal() override { storageMap().erase(id_); }

        /**
         * @brief Returns true if the value has been initialised for the current thread.
         * @throws System::ObjectDisposedException if this instance has been disposed.
         *
         * Ticket #1956 / cause T-G (SR-AUD-219). This was the ONE accessor on the type that did
         * not check, so a disposed ThreadLocal answered `false` -- indistinguishable from "alive,
         * and no value yet". .NET checks here too: `ObjectDisposedException.ThrowIf(id < 0, this)`
         * opens `ThreadLocal<T>.IsValueCreated` (ThreadLocal.cs:478-488), the same guard its
         * `Value` getter uses.
         */
        [[nodiscard]] bool getIsValueCreatedProperty() const {
            ThrowIfDisposed();
            return storageMap().find(id_) != storageMap().end();
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
            auto it = map.find(id_);
            if (it == map.end()) {
                auto& active = inProgress();
                if (!active.insert(id_).second)
                    throw System::InvalidOperationException(
                        "ValueFactory attempted to access the Value property of this instance.");
                std::shared_ptr<T> value;
                try {
                    value = std::make_shared<T>(factory_ ? factory_() : T{});
                } catch (...) {
                    active.erase(id_);
                    throw;
                }
                active.erase(id_);
                trackIfRequested(value);
                it = map.emplace(id_, std::move(value)).first;
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
            auto it = map.find(id_);
            if (it == map.end()) {
                auto value = std::make_shared<T>(v);
                trackIfRequested(value);
                map.emplace(id_, std::move(value));
            } else {
                // An existing value is UPDATED IN PLACE, so the registry -- which co-owns the
                // same object -- sees the new value without a second entry. That is why the
                // registry holds the pointer rather than a copy.
                *it->second = v;
            }
        }

        /** Returns the value for the current thread (alias for getValueProperty). */
        [[nodiscard]] T& Value() { return getValueProperty(); }

        /**
         * @brief Returns the values held for every thread that has one, as a snapshot.
         * @throws System::InvalidOperationException if this instance was not constructed with
         *         `trackAllValues = true`.
         * @throws System::ObjectDisposedException if this instance has been disposed.
         *
         * Ticket #1958 / SR-AUD-220. Transcribed from `ThreadLocal<T>.Values`
         * (`ThreadLocal.cs:421-434`):
         * @code
         * if (!_trackAllValues) throw new InvalidOperationException(SR.ThreadLocal_ValuesNotAvailable);
         * List<T>? list = GetValuesAsList();          // returns null if disposed
         * ObjectDisposedException.ThrowIf(list is null, this);
         * @endcode
         *
         * @note **The tracking check comes FIRST, before the disposed check, and that is
         * observable**: a disposed instance built WITHOUT tracking reports
         * `InvalidOperationException`, not `ObjectDisposedException`. The order is .NET's and a
         * test pins it.
         *
         * @note Returns `std::vector<T>` by value -- a snapshot, as .NET's `GetValuesAsList`
         * builds a fresh `List<T>` on every call. .NET's declared return type is `IList<T>`,
         * which this port has no counterpart for; the vector is the closest faithful shape and
         * mutating it cannot affect the instance, which is also true of .NET's copy.
         */
        [[nodiscard]] std::vector<T> getValuesProperty() const {
            if (!trackAllValues_) {
                throw System::InvalidOperationException(
                    "The ThreadLocal object is not tracking values. To use the Values property, "
                    "use a ThreadLocal constructor that accepts the trackAllValues parameter and "
                    "set the parameter to true.");
            }
            ThrowIfDisposed();
            std::lock_guard<std::mutex> lk(trackedMutex_);
            std::vector<T> snapshot;
            snapshot.reserve(trackedValues_.size());
            for (const auto& v : trackedValues_) snapshot.push_back(*v);
            return snapshot;
        }

        /** Releases resources for the current thread's value. */
        void Dispose() override {
            disposed_.store(true, std::memory_order_release);
            storageMap().erase(id_);
            // .NET's Dispose unlinks every LinkedSlot, releasing the tracked values with it.
            std::lock_guard<std::mutex> lk(trackedMutex_);
            trackedValues_.clear();
        }
    };

} // namespace System::Threading
