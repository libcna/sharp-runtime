// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include "System/Threading/LazyThreadSafetyMode.hpp"

namespace System {

    using Threading::LazyThreadSafetyMode;

    /**
     * @brief Provides support for lazy initialization.
     *
     * C++ counterpart of .NET System.Lazy<T>. The value is created on the first
     * access to getValueProperty() / Value(). Thread safety is controlled by the
     * constructor; the default is ExecutionAndPublication (exactly one thread
     * runs the factory; all others block until it completes).
     *
     * Note: Lazy<T> is not copyable or movable because it owns a std::once_flag.
     * Store it as a direct member or heap-allocate via std::unique_ptr.
     *
     * @tparam T The type of the lazily-initialized value.
     */
    template<typename T>
    class Lazy {
        mutable std::once_flag       onceflag_;
        mutable std::optional<T>     value_;
        std::function<T()>           factory_;
        mutable bool                 isValueCreated_ = false;
        LazyThreadSafetyMode         mode_;

        void initValue() const {
            value_         = factory_();
            isValueCreated_ = true;
        }

    public:
        // ---- Constructors ----

        /**
         * @brief Initializes a Lazy<T> that uses the default constructor T() as its factory.
         * Thread safety: ExecutionAndPublication.
         *
         * C++ counterpart of .NET Lazy<T>().
         */
        Lazy()
            : factory_([] { return T{}; }),
              mode_(LazyThreadSafetyMode::ExecutionAndPublication) {}

        /**
         * @brief Initializes a Lazy<T> with a pre-computed value.
         * IsValueCreated is true immediately; no factory is invoked.
         *
         * C++ counterpart of .NET Lazy<T>(T value).
         * @param value The pre-initialized value to store.
         */
        explicit Lazy(T value)
            : value_(std::move(value)),
              isValueCreated_(true),
              mode_(LazyThreadSafetyMode::ExecutionAndPublication) {}

        /**
         * @brief Initializes a Lazy<T> with the specified value factory function.
         * Thread safety: ExecutionAndPublication.
         *
         * C++ counterpart of .NET Lazy<T>(Func<T> valueFactory).
         * @param valueFactory Callable that produces the value on first access.
         */
        template<typename F>
            requires (std::is_invocable_r_v<T, std::decay_t<F>>
                   && !std::is_same_v<std::decay_t<F>, bool>)
        explicit Lazy(F&& valueFactory)
            : factory_(std::forward<F>(valueFactory)),
              mode_(LazyThreadSafetyMode::ExecutionAndPublication) {}

        /**
         * @brief Initializes a Lazy<T> using the default constructor T().
         *
         * C++ counterpart of .NET Lazy<T>(bool isThreadSafe).
         * @param isThreadSafe If true: ExecutionAndPublication mode; if false: None mode.
         */
        explicit Lazy(bool isThreadSafe)
            : factory_([] { return T{}; }),
              mode_(isThreadSafe ? LazyThreadSafetyMode::ExecutionAndPublication
                                 : LazyThreadSafetyMode::None) {}

        /**
         * @brief Initializes a Lazy<T> using the default constructor T() and the specified mode.
         *
         * C++ counterpart of .NET Lazy<T>(LazyThreadSafetyMode mode).
         * @param mode The thread-safety mode to use.
         */
        explicit Lazy(LazyThreadSafetyMode mode)
            : factory_([] { return T{}; }), mode_(mode) {}

        /**
         * @brief Initializes a Lazy<T> with the specified factory and thread-safety flag.
         *
         * C++ counterpart of .NET Lazy<T>(Func<T>, bool isThreadSafe).
         * @param valueFactory Callable that produces the value on first access.
         * @param isThreadSafe If true: ExecutionAndPublication mode; if false: None mode.
         */
        template<typename F>
            requires (std::is_invocable_r_v<T, std::decay_t<F>>
                   && !std::is_same_v<std::decay_t<F>, bool>)
        Lazy(F&& valueFactory, bool isThreadSafe)
            : factory_(std::forward<F>(valueFactory)),
              mode_(isThreadSafe ? LazyThreadSafetyMode::ExecutionAndPublication
                                 : LazyThreadSafetyMode::None) {}

        /**
         * @brief Initializes a Lazy<T> with the specified factory and thread-safety mode.
         *
         * C++ counterpart of .NET Lazy<T>(Func<T>, LazyThreadSafetyMode).
         * @param valueFactory Callable that produces the value on first access.
         * @param mode         The thread-safety mode to use.
         */
        template<typename F>
            requires (std::is_invocable_r_v<T, std::decay_t<F>>
                   && !std::is_same_v<std::decay_t<F>, bool>)
        Lazy(F&& valueFactory, LazyThreadSafetyMode mode)
            : factory_(std::forward<F>(valueFactory)), mode_(mode) {}

        // Lazy is not copyable or movable (std::once_flag constraint).
        Lazy(const Lazy&)            = delete;
        Lazy& operator=(const Lazy&) = delete;
        Lazy(Lazy&&)                 = delete;
        Lazy& operator=(Lazy&&)      = delete;

        ~Lazy() = default;

        // ---- Properties ----

        /**
         * @brief Gets the lazily-initialized value.
         * The factory is invoked at most once. All subsequent calls return
         * a reference to the cached value.
         *
         * C++ counterpart of .NET Lazy<T>.Value.
         * @return Const reference to the initialized value.
         * @throws Any exception thrown by the factory (propagated to the caller).
         */
        [[nodiscard]] const T& getValueProperty() const {
            if (mode_ == LazyThreadSafetyMode::ExecutionAndPublication) {
                std::call_once(onceflag_, [this] {
                    if (!isValueCreated_) initValue();
                });
            } else if (!isValueCreated_) {
                initValue();
            }
            return *value_;
        }

        /**
         * @brief Gets a value indicating whether the value has been created.
         *
         * C++ counterpart of .NET Lazy<T>.IsValueCreated.
         * @return true if getValueProperty() has been called and completed successfully.
         */
        [[nodiscard]] bool getIsValueCreatedProperty() const noexcept {
            return isValueCreated_;
        }

        /**
         * @brief Gets the thread-safety mode used by this instance.
         *
         * C++ counterpart of .NET Lazy<T>.Mode (debug-view property).
         * @return The LazyThreadSafetyMode specified at construction.
         */
        [[nodiscard]] LazyThreadSafetyMode getModeProperty() const noexcept {
            return mode_;
        }

        // ---- Methods ----

        /**
         * @brief Gets the lazily-initialized value; equivalent to getValueProperty().
         *
         * C++ counterpart of .NET Lazy<T>.Value (used as a method here for ergonomics).
         */
        [[nodiscard]] const T& Value() const { return getValueProperty(); }

        /**
         * @brief Returns a string representation of this Lazy<T> instance.
         *
         * C++ counterpart of .NET Lazy<T>.ToString(). Returns
         * "Value is not created." when the value has not yet been initialized,
         * or "Value is created." when it has.
         */
        [[nodiscard]] std::string ToString() const {
            return isValueCreated_ ? "Value is created." : "Value is not created.";
        }
    };

} // namespace System
