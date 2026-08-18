// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <concepts>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/InvalidOperationException.hpp"
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
     * PublicationOnly matches .NET as of ticket #2238, and BOTH of the deviations this
     * doc-comment used to record are gone. They are described here in the past tense because
     * the change REVERSED a guarantee this class advertised for its whole life, and a reader
     * who remembers the old contract needs to be told, not left to discover it.
     *
     * WAS: PublicationOnly serialized the factory behind a mutex, so it ran at most once, and a
     * factory that re-entered Value() on the same instance got InvalidOperationException in all
     * three modes. NOW: the factory runs with no lock held, exactly as .NET's
     * PublicationOnlyViaFactory does (`Lazy.cs:351-378`), the first thread to publish wins, and
     * every loser DISCARDS the value it computed.
     *
     * Two consequences follow, and both are the price of parity rather than oversights:
     *
     *  - **A PublicationOnly factory may now run concurrently on several threads**, and may run
     *    more than once. That is what PublicationOnly MEANS in .NET. If your factory has side
     *    effects, or is expensive, use ExecutionAndPublication -- which is still the default and
     *    still runs the factory exactly once.
     *  - **A recursive PublicationOnly factory now recurses.** It no longer receives a clean
     *    catchable InvalidOperationException; an unconditionally recursive factory will exhaust
     *    the stack, precisely as it does in .NET. The guard remains in force for None and
     *    ExecutionAndPublication, where .NET also raises it and where the alternative would be
     *    undefined behaviour (recursive std::call_once on one flag from one thread).
     *
     * Recursion into a *different* Lazy<T> instance is unaffected and always legal.
     *
     * Note: Lazy<T> is not copyable or movable because it owns synchronization
     * primitives (std::once_flag, std::mutex). Store it as a direct member or
     * heap-allocate via std::unique_ptr.
     *
     * @tparam T The type of the lazily-initialized value.
     */
    template<typename T>
    class Lazy {
        mutable std::once_flag             onceflag_;
        mutable std::mutex                 publicationOnlyMutex_;
        mutable std::optional<T>           value_;
        mutable std::exception_ptr         cachedException_;
        mutable std::atomic<std::thread::id> creatingThreadId_{};
        std::function<T()>                 factory_;
        // atomic, not plain bool: getIsValueCreatedProperty() reads this with no lock/
        // call_once involvement, so it must be safely readable while getValueProperty() is
        // concurrently writing it from another thread (ExecutionAndPublication/PublicationOnly
        // modes are meant to support exactly this multi-thread-observer pattern, matching real
        // .NET's Lazy<T>.IsValueCreated). Confirmed via a standalone ThreadSanitizer repro
        // before this fix: one thread computing the value while another polled
        // getIsValueCreatedProperty() raced on this field.
        mutable std::atomic<bool>          isValueCreated_{false};
        LazyThreadSafetyMode               mode_;

        // Rejects an *empty* std::function factory at construction, the way .NET rejects a
        // null Func<T> (`Lazy.cs`: `ArgumentNullException.ThrowIfNull(valueFactory)`).
        // The `requires` clauses on the three factory constructors only check that the
        // callable *type* is invocable; std::function<T()> satisfies that in its empty
        // state too, so an empty one used to be stored and then invoked by initValue(),
        // raising std::bad_function_call at the first Value() -- a native exception that
        // does not derive from System::Exception, so ported `catch (const Exception&)`
        // code cannot see it, arbitrarily far from the constructor that was at fault.
        // Ticket #1867 / SR-AUD-065 / CCF-011; see docs/EmptyCallableBoundaryPlan.md.
        void requireFactory() const {
            if (!factory_) throw ArgumentNullException("valueFactory");
        }

        // Rejects a LazyThreadSafetyMode value outside the three defined enumerators. .NET
        // routes every mode-taking constructor through LazyHelper.Create, whose switch has
        // exactly these three cases and a `default:` that throws
        // ArgumentOutOfRangeException(nameof(mode), SR.Lazy_ctor_ModeInvalid) -- so an
        // invalid mode fails at the constructor, before any lazy state exists. Without this
        // check the value was stored, stayed observable through getModeProperty(), and was
        // then routed by the `default:` label of getValueProperty()'s switch into the
        // PublicationOnly arm: the instance did not merely tolerate the invalid value, it
        // silently acquired another mode's fault-caching contract. Measured before the fix
        // (build-probe/2235_probe1_before.log): Lazy<int>(static_cast<LazyThreadSafetyMode>(99))
        // constructed, reported mode 99, and invoked a throwing factory twice.
        // Ticket #2236 / SR-AUD-064; see docs/CoreLazyThreadSafetyModeFamilyPlan.md.
        static void requireValidMode(LazyThreadSafetyMode mode) {
            switch (mode) {
                case LazyThreadSafetyMode::None:
                case LazyThreadSafetyMode::PublicationOnly:
                case LazyThreadSafetyMode::ExecutionAndPublication:
                    return;
                default:
                    throw ArgumentOutOfRangeException(
                        "mode", "The mode argument specifies an invalid value.");
            }
        }

        // Rejects a factory that re-enters Value() on this same instance from this same
        // thread. See the class doc-comment's deviation (2/2): this fires in all three
        // modes, where .NET fires it for None and ExecutionAndPublication only, because
        // the ExecutionAndPublication path would otherwise deadlock (recursive
        // std::call_once on the same flag from the same thread is undefined behavior) and
        // the PublicationOnly path would otherwise re-lock a non-recursive std::mutex
        // (likewise undefined). Must run *before* dispatching into std::call_once / the
        // lock, since the undefined behaviour is at that layer, not inside the factory call
        // itself. Ticket #2237 / SR-AUD-066; reopening path is ticket #2238.
        void checkNotReentrant() const {
            if (creatingThreadId_.load(std::memory_order_acquire) == std::this_thread::get_id()) {
                throw InvalidOperationException("ValueFactory attempted to access the Value property of this instance.");
            }
        }

        // Runs the factory and either publishes the value or (for modes that cache
        // faults - None and ExecutionAndPublication) captures the exception so every
        // subsequent access rethrows the same one, matching .NET's LazyHelper contract.
        void initValue(bool cacheFaults) const {
            creatingThreadId_.store(std::this_thread::get_id(), std::memory_order_release);
            try {
                value_ = factory_();
                isValueCreated_ = true;
                creatingThreadId_.store(std::thread::id{}, std::memory_order_release);
            } catch (...) {
                creatingThreadId_.store(std::thread::id{}, std::memory_order_release);
                if (cacheFaults) cachedException_ = std::current_exception();
                throw;
            }
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
         * @throws ArgumentNullException if @p valueFactory is an empty std::function,
         *         matching .NET's null-Func rejection at the constructor boundary.
         */
        template<typename F>
            requires (std::is_invocable_r_v<T, std::decay_t<F>>
                   && !std::is_same_v<std::decay_t<F>, bool>)
        explicit Lazy(F&& valueFactory)
            : factory_(std::forward<F>(valueFactory)),
              mode_(LazyThreadSafetyMode::ExecutionAndPublication) { requireFactory(); }

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
         * @throws ArgumentOutOfRangeException if @p mode is not one of the three defined
         *         LazyThreadSafetyMode enumerators, matching .NET's LazyHelper.Create.
         */
        explicit Lazy(LazyThreadSafetyMode mode)
            : factory_([] { return T{}; }), mode_(mode) { requireValidMode(mode_); }

        /**
         * @brief Initializes a Lazy<T> with the specified factory and thread-safety flag.
         *
         * C++ counterpart of .NET Lazy<T>(Func<T>, bool isThreadSafe).
         * @param valueFactory Callable that produces the value on first access.
         * @param isThreadSafe If true: ExecutionAndPublication mode; if false: None mode.
         * @throws ArgumentNullException if @p valueFactory is an empty std::function,
         *         matching .NET's null-Func rejection at the constructor boundary.
         */
        template<typename F>
            requires (std::is_invocable_r_v<T, std::decay_t<F>>
                   && !std::is_same_v<std::decay_t<F>, bool>)
        Lazy(F&& valueFactory, bool isThreadSafe)
            : factory_(std::forward<F>(valueFactory)),
              mode_(isThreadSafe ? LazyThreadSafetyMode::ExecutionAndPublication
                                 : LazyThreadSafetyMode::None) { requireFactory(); }

        /**
         * @brief Initializes a Lazy<T> with the specified factory and thread-safety mode.
         *
         * C++ counterpart of .NET Lazy<T>(Func<T>, LazyThreadSafetyMode).
         * @param valueFactory Callable that produces the value on first access.
         * @param mode         The thread-safety mode to use.
         * @throws ArgumentNullException if @p valueFactory is an empty std::function,
         *         matching .NET's null-Func rejection at the constructor boundary.
         * @throws ArgumentOutOfRangeException if @p mode is not one of the three defined
         *         LazyThreadSafetyMode enumerators, matching .NET's LazyHelper.Create.
         *         The factory is checked first, as .NET's
         *         Lazy(Func<T>, LazyThreadSafetyMode) does.
         */
        template<typename F>
            requires (std::is_invocable_r_v<T, std::decay_t<F>>
                   && !std::is_same_v<std::decay_t<F>, bool>)
        Lazy(F&& valueFactory, LazyThreadSafetyMode mode)
            : factory_(std::forward<F>(valueFactory)), mode_(mode)
        { requireFactory(); requireValidMode(mode_); }

        // Lazy is not copyable or movable (std::once_flag constraint).
        Lazy(const Lazy&)            = delete;
        Lazy& operator=(const Lazy&) = delete;
        Lazy(Lazy&&)                 = delete;
        Lazy& operator=(Lazy&&)      = delete;

        ~Lazy() = default;

        // ---- Properties ----

        /**
         * @brief Gets the lazily-initialized value.
         * The factory is invoked at most once (except in PublicationOnly mode, where a
         * failed attempt may be retried). All subsequent calls return a reference to the
         * cached value.
         *
         * C++ counterpart of .NET Lazy<T>.Value. Matches .NET's fault-caching contract:
         * in None and ExecutionAndPublication modes, if the factory throws, the same
         * exception is rethrown on every later access rather than retrying the factory;
         * in PublicationOnly mode, a failed attempt does not poison the instance and the
         * next access retries.
         * @return Const reference to the initialized value.
         * @throws Any exception thrown by the factory (propagated to the caller), or
         *         InvalidOperationException if the factory recursively accesses this
         *         same Value() while it is already running, in None or ExecutionAndPublication
         *         mode. **PublicationOnly does not raise it** and never did in .NET: since
         *         ticket #2238 a recursive PublicationOnly factory simply runs again, and an
         *         unconditionally recursive one exhausts the stack. SR-AUD-066.
         */
        [[nodiscard]] const T& getValueProperty() const {
            // #2238: the reentrancy guard applies to the two modes .NET applies it to, and NOT
            // to PublicationOnly, whose factory now runs unlocked and therefore has no lock to
            // re-enter. `requireValidMode` guarantees `mode_` is one of the three enumerators,
            // so this test names PublicationOnly exactly.
            //
            // THIS CONDITION IS REDUNDANT TODAY, AND SAYS SO RATHER THAN LOOKING LOAD-BEARING.
            // `creatingThreadId_` is written only by `initValue`, which the PublicationOnly arm
            // no longer calls, so `checkNotReentrant()` could not fire on that path even if it
            // ran. Mutation M1 of #2238 -- deleting this test -- is therefore NOT CAUGHT by any
            // test, and the ticket records that instead of claiming coverage. It is kept because
            // it states the contract at the place the contract is decided, and because it keeps
            // the guard correct if a future change gives PublicationOnly a reason to record the
            // creating thread again.
            if (mode_ != LazyThreadSafetyMode::PublicationOnly) checkNotReentrant();
            switch (mode_) {
                case LazyThreadSafetyMode::ExecutionAndPublication: {
                    std::call_once(onceflag_, [this] {
                        if (!isValueCreated_ && !cachedException_) {
                            try { initValue(/*cacheFaults=*/true); }
                            catch (...) { /* cached inside initValue; swallow so the flag still completes */ }
                        }
                    });
                    if (cachedException_) std::rethrow_exception(cachedException_);
                    return *value_;
                }
                case LazyThreadSafetyMode::None: {
                    if (cachedException_) std::rethrow_exception(cachedException_);
                    if (!isValueCreated_) initValue(/*cacheFaults=*/true);
                    return *value_;
                }
                // The two labels stay fused: since ticket #2236 every constructor validates
                // its mode and Lazy<T> has neither a mode setter nor a copy/move assignment,
                // so mode_ is always one of the three defined values and `default:` is
                // unreachable. It used to be the door through which an invalid mode silently
                // acquired PublicationOnly's contract (SR-AUD-064). Splitting it now would
                // mean inventing a behaviour for a state construction forbids.
                case LazyThreadSafetyMode::PublicationOnly:
                default: {
                    // .NET's PublicationOnlyViaFactory / PublicationOnly
                    // (`Lazy.cs:351-378`), transcribed. THE FACTORY RUNS WITH NO LOCK HELD.
                    // .NET calls `factory()` and only then attempts an
                    // `Interlocked.CompareExchange` on the state; a thread that loses the
                    // exchange DISCARDS the value it computed.
                    if (isValueCreated_.load(std::memory_order_acquire)) return *value_;

                    // Deliberately outside the lock, and deliberately not fault-cached: in
                    // PublicationOnly a failed attempt does not poison the instance, so the next
                    // access simply tries again.
                    T candidate = factory_();

                    {
                        std::lock_guard<std::mutex> lock(publicationOnlyMutex_);
                        if (!isValueCreated_.load(std::memory_order_relaxed)) {
                            value_ = std::move(candidate);
                            // Release, paired with the acquire above, so a thread that observes
                            // the flag also observes the value. `value_` is written exactly once
                            // -- the guard makes every later winner a no-op -- so the reference
                            // returned below can never be rewritten under a reader.
                            isValueCreated_.store(true, std::memory_order_release);
                        }
                        // ...and `candidate` is destroyed here if we lost. That discard IS the
                        // contract, not a leak.
                    }
                    return *value_;
                }
            }
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
         * C++ counterpart of .NET Lazy<T>.ToString(). Matches .NET: if the value has
         * already been created, returns @c Value().ToString() (or the equivalent for
         * built-in numeric types, via std::to_string) - NOT a fixed placeholder string.
         * Only when the value has not yet been created does this return the fixed
         * "Value is not created." message, without triggering creation.
         */
        [[nodiscard]] std::string ToString() const {
            if (!isValueCreated_) return "Value is not created.";
            if constexpr (requires (const T& t) { { t.ToString() } -> std::convertible_to<std::string>; }) {
                return Value().ToString();
            } else if constexpr (requires (const T& t) { { std::to_string(t) } -> std::convertible_to<std::string>; }) {
                return std::to_string(Value());
            } else {
                // No .ToString() member and not std::to_string-able: this port has no
                // universal object-to-string fallback (no runtime reflection), unlike
                // .NET's Object.ToString(). Falls back to the type name only.
                return "Value is created.";
            }
        }
    };

} // namespace System
