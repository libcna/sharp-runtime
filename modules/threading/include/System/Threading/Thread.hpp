// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <any>
#include <map>
#include <mutex>
#include "System/ArgumentException.hpp"
#include "System/LocalDataStoreSlot.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Threading/ApartmentState.hpp"
#include "System/Threading/ThreadPriority.hpp"
#include "System/Threading/ThreadState.hpp"
#include "System/Threading/ThreadStateException.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /**
     * @brief Creates and controls a thread, sets its priority, and gets its status.
     *
     * C++ counterpart of .NET System.Threading.Thread.
     * Wraps std::thread. The thread does NOT start at construction — call Start()
     * exactly once. Calling Start() a second time throws System::Threading::ThreadStateException.
     *
     * Obsolete .NET APIs (Abort, Suspend, Resume, VolatileRead/Write) are omitted.
     * Apartment-state and compressed-stack APIs are stubs.
     */
    class Thread {
        // Managed thread IDs start at 2, matching .NET's convention that the main/first
        // thread is ID 1 (see currentThreadState_'s default below) — the first Thread object
        // constructed by user code must not collide with that.
        inline static std::atomic<intcs> nextManagedId_{2};

        // Verified: the spawned-thread lambda previously captured `this` by raw pointer and
        // wrote into `finished_`/read `currentThread_->...` after ~Thread() (which detach()es,
        // not join()s -- a deliberate design letting the OS thread outlive the wrapper). A
        // Thread object destroyed before its OS thread finishes left the lambda touching freed
        // memory: a genuine use-after-free, the same bug class fixed elsewhere this session
        // (Channel::ReadAsync/WriteAsync's raw-`this` capture, RegisteredWaitHandle::
        // Unregister()'s detach-without-join). Fixed by moving everything the spawned thread
        // touches into a heap-allocated RunState the lambda captures by shared_ptr, so it stays
        // alive for the OS thread's full lifetime regardless of the owning Thread object's.
        struct RunState {
            std::atomic<bool> finished{false};
            std::atomic<bool> isBackground{false};
            intcs             managedThreadId = 0;
        };

        // Tracks which running thread's RunState (if any) belongs to the calling OS thread, so
        // CurrentThread() can report the correct ManagedThreadId/IsBackground instead of an
        // unrelated hash of the OS thread handle. Threads not started via this class (the
        // main thread, or any other externally-created thread) see nullptr and report the
        // .NET-convention main-thread ID of 1. Holding the shared_ptr here (rather than a raw
        // Thread*) means CurrentThreadProxy never dereferences the (possibly already-destroyed)
        // Thread object itself.
        inline static thread_local std::shared_ptr<RunState> currentThreadState_;

        // #1958 / SR-AUD-193. A thread NOT started through this class used to report managed id
        // 1 -- the main thread's -- so every external thread collided with it and with each
        // other, erasing the uniqueness .NET guarantees. Each OS thread now takes an id from the
        // same counter the wrapper uses, so no two threads can report the same one.
        //
        // The main thread keeps 1 by IDENTITY rather than by arriving first: its OS id is
        // captured during static initialisation, which runs on it. Assigning "1 to whoever asks
        // first" would hand it to a worker whenever a worker happens to ask before main does.
        inline static const std::thread::id mainThreadOsId_ = std::this_thread::get_id();
        inline static thread_local intcs    currentExternalId_ = 0;

        /** @brief The managed id of the calling thread, assigned on first use. */
        static intcs currentManagedThreadId() {
            if (currentThreadState_) return currentThreadState_->managedThreadId;
            if (currentExternalId_ == 0) {
                currentExternalId_ = (std::this_thread::get_id() == mainThreadOsId_)
                                         ? 1
                                         : nextManagedId_.fetch_add(1);
            }
            return currentExternalId_;
        }

        std::shared_ptr<RunState> state_ = std::make_shared<RunState>();
        std::function<void()>  fn_;
        // Ticket #1958 / SR-AUD-194. Exactly ONE of these two is ever set, and which one records
        // the delegate shape -- so no separate flag is needed. `fn_` non-empty means the thread
        // was built with the parameterless shape, which is precisely .NET's
        // `startHelper._start is ThreadStart` test.
        std::function<void(void*)> paramFn_;
        std::thread            thread_;
        std::string            name_;
        bool                   isThreadPoolThread_ = false;
        ThreadPriority         priority_          = ThreadPriority::Normal;
        std::atomic<bool>      started_{false};

    public:
        /**
         * @brief Constructs a Thread with the given parameterless start function.
         * @param start Function to execute on the new thread.
         * @throws System::ArgumentNullException if @p start is an empty std::function.
         *
         * .NET's `Thread(ThreadStart start)` opens with
         * `ArgumentNullException.ThrowIfNull(start)`, and this port must, because the
         * consequence of deferring it is not a catchable exception: `Start()` used to
         * hand the empty function to a new OS thread, whose call to it raised
         * `std::bad_function_call` with no handler on that thread, so `std::terminate`
         * killed the whole process (exit 134) at a point where no caller could observe,
         * let alone catch, the mistake (SR-AUD-192). Ticket #1951 / CCF-011; see
         * docs/EmptyCallableBoundaryPlan.md and docs/ThreadingNamespaceReviewPlan.md
         * cause T-B. Rejecting at construction also means no managed thread id is
         * consumed and no OS thread exists for a Thread that can never run.
         */
        explicit Thread(std::function<void()> start)
            : fn_(std::move(start))
        {
            if (!fn_) throw System::ArgumentNullException("start");
            state_->managedThreadId = nextManagedId_.fetch_add(1);
        }

        /**
         * @brief Constructs a Thread whose body ACCEPTS the parameter `Start(void*)` supplies.
         * @param start Function to execute on the new thread, receiving `Start`'s argument.
         * @throws System::ArgumentNullException if @p start is an empty std::function.
         *
         * C++ counterpart of .NET's `Thread(ParameterizedThreadStart start)`
         * (`Thread.cs:152`). Ticket #1958 / SR-AUD-194.
         *
         * **This constructor is what makes `Start(void*)` mean anything.** Before it, the only
         * accepted callback shape had no parameter slot, so `Start(void*)` captured its argument
         * and then discarded it with a literal `(void)parameter;` while its own doc-comment said
         * the value was "forwarded to the thread function". There was no way for a caller to
         * receive it and no diagnostic saying so.
         *
         * The same `ArgumentNullException` guard applies for the same reason as the parameterless
         * constructor's (SR-AUD-192): deferring it means `std::bad_function_call` on a thread with
         * no handler, i.e. `std::terminate`.
         */
        explicit Thread(std::function<void(void*)> start)
            : paramFn_(std::move(start))
        {
            if (!paramFn_) throw System::ArgumentNullException("start");
            state_->managedThreadId = nextManagedId_.fetch_add(1);
        }

        ~Thread() {
            if (thread_.joinable()) thread_.detach();
        }

        Thread(const Thread&)            = delete;
        Thread& operator=(const Thread&) = delete;

        // -----------------------------------------------------------------------
        // Control
        // -----------------------------------------------------------------------

        /**
         * @brief Starts the thread.
         * @throws System::Threading::ThreadStateException if Start() has already been called.
         */
        void Start() {
            if (started_.exchange(true))
                throw System::Threading::ThreadStateException("Thread is running or terminated; it cannot restart.");
            // A PARAMETERIZED thread started this way runs with a null argument and NO exception.
            // That asymmetry is .NET's: its private Start(bool) sets `startHelper._startArg = null`
            // and performs no delegate-shape check at all (Thread.cs:239-253) -- only
            // Start(parameter) guards. Pinned by a test, because "reject it for symmetry" is the
            // plausible wrong answer.
            thread_ = std::thread([state = state_, fn = std::move(fn_),
                                   paramFn = std::move(paramFn_)]() mutable {
                currentThreadState_ = state;
                if (fn) fn(); else paramFn(nullptr);
                state->finished.store(true);
            });
        }

        /**
         * @brief Starts the thread, passing @p parameter to a ParameterizedThreadStart function.
         * @param parameter Argument forwarded to the thread function (stored as void*).
         * @throws System::Threading::ThreadStateException if Start() has already been called.
         */
        void Start(void* parameter) {
            // THE SHAPE CHECK COMES FIRST -- but only while the thread has NOT been started, and
            // that qualification is .NET's rather than an accident here. Its private
            // Start(object, bool) wraps the whole check in `if (startHelper != null)`
            // (Thread.cs:204-214), and the comment two lines above says why: "In the case of a
            // null startHelper (second call to start on same thread) StartCore method will take
            // care of the error reporting."
            //
            // So a SECOND Start(void*) reports the RESTART error, not the wrong-shape error. This
            // port gets the same rule from the same fact: fn_ is MOVED FROM into the thread body
            // on the first successful start, so it is empty afterwards and the guard falls
            // through -- exactly as .NET's startHelper becomes null. A pin asserts both halves,
            // and it was written asserting the opposite first: the test failed, and the reference
            // showed the test was wrong rather than the code.
            if (fn_) {
                throw System::InvalidOperationException(
                    "The thread was created with a ThreadStart delegate that does not accept a "
                    "parameter.");
            }
            if (started_.exchange(true))
                throw System::Threading::ThreadStateException("Thread is running or terminated; it cannot restart.");
            thread_ = std::thread([state = state_, paramFn = std::move(paramFn_), parameter]() mutable {
                currentThreadState_ = state;
                paramFn(parameter);
                state->finished.store(true);
            });
        }

        /**
         * @brief Blocks the calling thread until this thread terminates.
         * @throws System::Threading::ThreadStateException if this thread has not been started.
         */
        void Join() {
            if (!started_.load())
                throw System::Threading::ThreadStateException("Thread has not been started.");
            if (thread_.joinable()) thread_.join();
        }

        /**
         * @brief Blocks the calling thread until this thread terminates or
         * @p millisecondsTimeout elapses.
         * @param millisecondsTimeout Maximum wait time in milliseconds, or -1 (Timeout.Infinite)
         *        to block until the thread terminates.
         * @return true if the thread terminated; false if the timeout elapsed.
         * @throws System::ArgumentOutOfRangeException if @p millisecondsTimeout is less than -1.
         * @throws System::Threading::ThreadStateException if this thread has not been started.
         */
        bool Join(intcs millisecondsTimeout) {
            if (millisecondsTimeout < -1)
                throw System::ArgumentOutOfRangeException("millisecondsTimeout",
                    "Number must be either non-negative and less than or equal to Int32.MaxValue or -1.");
            if (!started_.load())
                throw System::Threading::ThreadStateException("Thread has not been started.");
            if (!thread_.joinable()) return true;
            if (millisecondsTimeout == -1) {
                thread_.join();
                return true;
            }
            auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(millisecondsTimeout);
            while (!state_->finished.load()) {
                if (std::chrono::steady_clock::now() >= deadline) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (thread_.joinable()) thread_.join();
            return true;
        }

        /** @brief No-op stub — Interrupt() is not supported in this port. */
        void Interrupt() noexcept {}

        // -----------------------------------------------------------------------
        // Properties
        // -----------------------------------------------------------------------

        /**
         * @brief Returns true while the OS thread is live (started and not yet joined).
         * @return true if the thread is running.
         */
        [[nodiscard]] bool getIsAliveProperty() const { return thread_.joinable() && !state_->finished.load(); }

        /**
         * @brief Returns the unique managed thread ID assigned at construction.
         * @return Managed thread ID.
         */
        [[nodiscard]] intcs getManagedThreadIdProperty() const { return state_->managedThreadId; }

        /** @brief Returns true if this is a background thread. */
        [[nodiscard]] bool getIsBackgroundProperty() const { return state_->isBackground.load(); }
        /** @brief Sets the background status of this thread. */
        void setIsBackgroundProperty(bool v) { state_->isBackground.store(v); }

        /** @brief Returns true if this thread was created by the thread pool. Always false for user-created threads. */
        [[nodiscard]] bool getIsThreadPoolThreadProperty() const { return isThreadPoolThread_; }

        /** @brief Returns the name of this thread. */
        [[nodiscard]] const std::string& getNameProperty() const { return name_; }
        /** @brief Sets the name of this thread. */
        void setNameProperty(const std::string& name) { name_ = name; }

        /** @brief Returns the scheduling priority of this thread. */
        [[nodiscard]] ThreadPriority getPriorityProperty() const { return priority_; }
        /** @brief Sets the scheduling priority (stored but not applied to the OS thread). */
        void setPriorityProperty(ThreadPriority p) { priority_ = p; }

        /**
         * @brief Returns the current execution state of this thread.
         * @return ThreadState reflecting started/running/stopped flags.
         */
        [[nodiscard]] ThreadState getThreadStateProperty() const {
            if (!started_.load())          return ThreadState::Unstarted;
            if (state_->finished.load())   return ThreadState::Stopped;
            if (!thread_.joinable())       return ThreadState::Stopped;
            return state_->isBackground.load() ? (ThreadState::Running | ThreadState::Background)
                                                : ThreadState::Running;
        }

        // -----------------------------------------------------------------------
        // Apartment state (stub — COM apartments are not meaningful in C++)
        // -----------------------------------------------------------------------

        /** @brief Returns ApartmentState::Unknown (COM apartments not supported in C++). */
        [[nodiscard]] ApartmentState GetApartmentState() const noexcept {
            return ApartmentState::Unknown;
        }
        /** @brief No-op stub — COM apartment state cannot be set. */
        void SetApartmentState(ApartmentState) noexcept {}
        /** @brief Always returns false — COM apartment state cannot be set. */
        [[nodiscard]] bool TrySetApartmentState(ApartmentState) noexcept { return false; }

        // -----------------------------------------------------------------------
        // Static helpers
        // -----------------------------------------------------------------------

        /**
         * @brief Suspends the current thread for @p milliseconds.
         * @param milliseconds Duration in milliseconds (0 yields the scheduler), or -1
         *        (Timeout.Infinite) to sleep until the process ends (Interrupt() is a
         *        no-op stub in this port, so an infinite sleep cannot be woken early).
         * @throws System::ArgumentOutOfRangeException if @p milliseconds is less than -1.
         */
        static void Sleep(intcs milliseconds) {
            if (milliseconds < -1)
                throw System::ArgumentOutOfRangeException("millisecondsTimeout",
                    "Number must be either non-negative and less than or equal to Int32.MaxValue or -1.");
            if (milliseconds == -1) {
                std::this_thread::sleep_until(std::chrono::steady_clock::time_point::max());
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }

        /**
         * @brief Causes the operating system to change the state of the current
         * instance to WaitSleepJoin for the specified duration.
         * @param timeout Time span specifying the sleep duration.
         */
        static void Sleep(std::chrono::milliseconds timeout) {
            std::this_thread::sleep_for(timeout);
        }

        /**
         * @brief Spins for @p iterations tight-loop iterations.
         * @param iterations Number of spin iterations.
         */
        static void SpinWait(intcs iterations) {
            for (intcs i = 0; i < iterations; ++i)
                std::atomic_signal_fence(std::memory_order_seq_cst);
        }

        /** @brief Issues a full memory fence (sequentially consistent). */
        static void MemoryBarrier() {
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }

        /**
         * @brief Causes the current thread to yield execution to another thread.
         * @return true if the operating system switched to another thread.
         */
        static bool Yield() {
            std::this_thread::yield();
            return true;
        }

        /**
         * @brief Returns the ID of the processor on which the current thread is running.
         * @return Processor ID (best-effort; may be stale immediately).
         */
        [[nodiscard]] static intcs GetCurrentProcessorId();

        // -----------------------------------------------------------------------
        // CurrentThread proxy
        // -----------------------------------------------------------------------

        /**
         * @brief Lightweight proxy representing the calling thread.
         *
         * Returned by Thread::CurrentThread(). Provides read-only access to
         * properties of the calling thread without owning its lifetime.
         */
        struct CurrentThreadProxy {
            /**
             * @brief Returns the managed thread ID of the calling thread.
             *
             * Resolves to the same ManagedThreadId the owning Thread object reports when the
             * calling thread was started via Thread::Start(). Ticket #1958 / SR-AUD-193: a
             * thread NOT created through this class used to return 1 unconditionally -- the main
             * thread's id -- so every external thread collided with it and with every other
             * external thread. Each now takes a distinct id from the same counter, which is the
             * uniqueness .NET's ManagedThreadId guarantees.
             *
             * The main thread still reports 1, and now does so by identity rather than by
             * arriving first: its OS thread id is captured during static initialisation.
             *
             * The id is assigned on FIRST USE, so a thread that never asks costs nothing.
             */
            [[nodiscard]] intcs getManagedThreadIdProperty() const {
                return currentManagedThreadId();
            }
            /** @brief Returns whether the calling thread's owning Thread object is marked background. */
            [[nodiscard]] bool getIsBackgroundProperty() const {
                return currentThreadState_ && currentThreadState_->isBackground.load();
            }
            /** @brief Suspends the calling thread for @p ms milliseconds. */
            static void Sleep(intcs ms) {
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            }
        };

        /**
         * @brief Returns a proxy for the calling thread.
         * @return CurrentThreadProxy for the thread calling this method.
         */
        static CurrentThreadProxy CurrentThread() { return CurrentThreadProxy{}; }

        // -------------------------------------------------------------------------
        // Thread-local data slots — ticket #2298 (SR-AUD-129)
        // -------------------------------------------------------------------------
        //
        // C++ counterparts of .NET's `Thread.AllocateDataSlot`, `AllocateNamedDataSlot`,
        // `GetNamedDataSlot`, `FreeNamedDataSlot`, `GetData` and `SetData`
        // (`Thread.cs:502-507`, backed by `Thread.LocalDataStore` at `:660-729`).
        //
        // THIS SURFACE DID NOT EXIST HERE AT ALL before #2298, which is what made
        // `System::LocalDataStoreSlot` a project-owned type wearing a .NET name: it had a public
        // constructor and a getData/setData pair, and the single `std::any` it held was ONE VALUE
        // SHARED BY EVERY THREAD. A write from any thread replaced what every other thread read.
        //
        // `docs/StandingApprovals.md` SA-9 authorised this new public surface explicitly, as the
        // only way `LocalDataStoreSlot` could reach .NET's shape.
        //
        // THE STORAGE LIVES HERE RATHER THAN IN THE SLOT, and that is a module-graph decision
        // rather than a design preference: `LocalDataStoreSlot` is in `Core.Base`, this class is
        // in `modules/threading` which depends on it, so a slot holding a thread-local would
        // invert the graph. The slot carries an opaque id; the per-thread map is below. The
        // observable contract is .NET's either way.

        /**
         * @brief Allocates an unnamed thread-local data slot.
         *
         * C++ counterpart of .NET `Thread.AllocateDataSlot()`.
         */
        [[nodiscard]] static System::LocalDataStoreSlot AllocateDataSlot() {
            return System::LocalDataStoreSlot(nextSlotId());
        }

        /**
         * @brief Allocates a named thread-local data slot.
         *
         * C++ counterpart of .NET `Thread.AllocateNamedDataSlot(string)`, which adds to the map
         * with `Dictionary.Add` and therefore **throws when the name already exists**
         * (`Thread.cs:679-688`). `GetNamedDataSlot` is the get-or-create door; this one is not.
         *
         * @throws System::ArgumentException if @p name is already allocated.
         */
        [[nodiscard]] static System::LocalDataStoreSlot AllocateNamedDataSlot(const std::string& name) {
            std::lock_guard<std::mutex> lock(namedSlotMutex());
            auto& map = namedSlots();
            if (map.find(name) != map.end()) {
                throw System::ArgumentException(
                    "Named data slot already exists for this name.", "name");
            }
            const System::LocalDataStoreSlot slot(nextSlotId());
            map.emplace(name, slot);
            return slot;
        }

        /**
         * @brief Looks up a named slot, allocating it if it does not exist.
         *
         * C++ counterpart of .NET `Thread.GetNamedDataSlot(string)` (`Thread.cs:690-702`), which
         * is get-or-create and never throws for an unknown name.
         */
        [[nodiscard]] static System::LocalDataStoreSlot GetNamedDataSlot(const std::string& name) {
            std::lock_guard<std::mutex> lock(namedSlotMutex());
            auto& map = namedSlots();
            auto it = map.find(name);
            if (it != map.end()) return it->second;
            const System::LocalDataStoreSlot slot(nextSlotId());
            map.emplace(name, slot);
            return slot;
        }

        /**
         * @brief Removes a name from the named-slot map.
         *
         * C++ counterpart of .NET `Thread.FreeNamedDataSlot(string)` (`Thread.cs:704-711`).
         *
         * @note It **does not destroy a slot a caller still holds**, and .NET's does not either —
         *       there the map drops its reference and the garbage collector decides the rest.
         *       Here the slot stays valid and keeps its per-thread values; only the *name*
         *       stops resolving to it. Removing an unknown name is a no-op, as in .NET.
         */
        static void FreeNamedDataSlot(const std::string& name) {
            std::lock_guard<std::mutex> lock(namedSlotMutex());
            namedSlots().erase(name);
        }

        /**
         * @brief Reads the CALLING THREAD's value for @p slot.
         *
         * C++ counterpart of .NET `Thread.GetData(LocalDataStoreSlot)`.
         * @return The stored value, or an empty `std::any` if this thread never set one.
         */
        [[nodiscard]] static std::any GetData(const System::LocalDataStoreSlot& slot) {
            auto& store = threadSlotStore();
            auto it = store.find(slot.id());
            return it == store.end() ? std::any{} : it->second;
        }

        /**
         * @brief Writes the CALLING THREAD's value for @p slot.
         *
         * C++ counterpart of .NET `Thread.SetData(LocalDataStoreSlot, object?)`. No other thread
         * observes the write — which is the whole point, and the opposite of what this type did
         * before #2298.
         */
        static void SetData(const System::LocalDataStoreSlot& slot, std::any data) {
            threadSlotStore()[slot.id()] = std::move(data);
        }

    private:
        /// Monotonic slot ids. Shared across threads, so atomic; never reused, so a freed name
        /// cannot alias a live slot.
        static std::size_t nextSlotId() {
            static std::atomic<std::size_t> counter{1};
            return counter.fetch_add(1, std::memory_order_relaxed);
        }

        /// The named-slot map and its lock. Process-wide, exactly as .NET's static map is.
        static std::map<std::string, System::LocalDataStoreSlot>& namedSlots() {
            static std::map<std::string, System::LocalDataStoreSlot> map;
            return map;
        }
        static std::mutex& namedSlotMutex() {
            static std::mutex m;
            return m;
        }

        /// THE PER-THREAD STORAGE. `thread_local` is what makes GetData/SetData thread-local at
        /// all; the pre-#2298 type had one shared value instead.
        static std::map<std::size_t, std::any>& threadSlotStore() {
            static thread_local std::map<std::size_t, std::any> store;
            return store;
        }

    public:

    };

} // namespace System::Threading
