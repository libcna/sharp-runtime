// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Concurrent/ConcurrentQueue.hpp"
#include "System/Collections/Concurrent/IProducerConsumerCollection.hpp"
#include "System/Collections/Generic/IEnumerator.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/IDisposable.hpp"
#include "System/NotSupportedException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Threading/CancellationToken.hpp"
#include "System/Threading/CancellationTokenRegistration.hpp"
#include "System/Threading/Timeout.hpp"
#include "System/TimeSpan.hpp"

namespace System::Collections::Concurrent {

    using SharpRuntime::intcs;

    /**
     * Provides blocking and bounding capabilities for a thread-safe collection implementing
     * IProducerConsumerCollection<T>.
     *
     * C++ counterpart of .NET System.Collections.Concurrent.BlockingCollection<T>.
     *
     * @note As in .NET, the underlying collection must not be modified directly after it has
     * been supplied to BlockingCollection. Direct mutation bypasses the wrapper's condition
     * variables and capacity accounting.
     * @note The C# array-taking *ToAny methods map to std::vector<BlockingCollection<T>*>.
     * C++ condition variables do not offer WaitAny, so their blocking path polls all collections
     * at one-millisecond intervals while retaining timeout and cancellation semantics.
     */
    template<typename T>
    class BlockingCollection : public System::Collections::Generic::IEnumerable<T>, public System::IDisposable {
        static constexpr intcs NonBounded = -1;

        std::shared_ptr<IProducerConsumerCollection<T>> collection_;
        intcs boundedCapacity_ = NonBounded;
        mutable std::mutex mutex_;
        std::condition_variable itemsAvailable_;
        std::condition_variable spaceAvailable_;
        bool addingCompleted_ = false;
        bool disposed_ = false;

        class SnapshotEnumerator final : public System::Collections::Generic::IEnumerator<T> {
            std::vector<T> items_;
            intcs index_ = -1;

        public:
            explicit SnapshotEnumerator(std::vector<T> items) : items_(std::move(items)) {}

            bool MoveNext() override { return ++index_ < static_cast<intcs>(items_.size()); }
            void Reset() override { index_ = -1; }
            [[nodiscard]] const T& Current() const override { return items_[static_cast<std::size_t>(index_)]; }
        };

        class CancellationNotification final {
            System::Threading::CancellationTokenRegistration registration_;

        public:
            CancellationNotification(System::Threading::CancellationToken token,
                                     std::condition_variable& itemsAvailable,
                                     std::condition_variable& spaceAvailable)
                : registration_(token.Register([&itemsAvailable, &spaceAvailable]() {
                    itemsAvailable.notify_all();
                    spaceAvailable.notify_all();
                })) {}

            ~CancellationNotification() { registration_.Dispose(); }

            CancellationNotification(const CancellationNotification&) = delete;
            CancellationNotification& operator=(const CancellationNotification&) = delete;
        };

        void checkDisposedLocked() const {
            System::ObjectDisposedException::ThrowIf(disposed_, "BlockingCollection");
        }

        void checkDisposed() const {
            std::lock_guard<std::mutex> lock(mutex_);
            checkDisposedLocked();
        }

        static void validateMillisecondsTimeout(intcs millisecondsTimeout) {
            if (millisecondsTimeout < System::Threading::Timeout::Infinite) {
                throw System::ArgumentOutOfRangeException(
                    "millisecondsTimeout", "The timeout must be a value between -1 and Int32.MaxValue, inclusive.");
            }
        }

        static intcs validateTimeout(const System::TimeSpan& timeout) {
            if (timeout.getTicksProperty() == System::Threading::Timeout::InfiniteTimeSpan) {
                return System::Threading::Timeout::Infinite;
            }
            const double milliseconds = timeout.getTotalMillisecondsProperty();
            if (timeout.getTicksProperty() < System::Threading::Timeout::InfiniteTimeSpan ||
                milliseconds > static_cast<double>(std::numeric_limits<intcs>::max())) {
                throw System::ArgumentOutOfRangeException("timeout");
            }
            return static_cast<intcs>(milliseconds);
        }

        void initialize(std::shared_ptr<IProducerConsumerCollection<T>> collection, intcs boundedCapacity) {
            if (!collection) {
                throw System::ArgumentNullException("collection");
            }
            if (boundedCapacity != NonBounded && boundedCapacity <= 0) {
                throw System::ArgumentOutOfRangeException("boundedCapacity");
            }
            if (boundedCapacity != NonBounded && collection->getCountProperty() > boundedCapacity) {
                throw System::ArgumentException("The supplied collection contains more values than are permitted by boundedCapacity.", "collection");
            }
            collection_ = std::move(collection);
            boundedCapacity_ = boundedCapacity;
        }

        bool tryAddCore(const T& item, intcs millisecondsTimeout,
                        const System::Threading::CancellationToken& cancellationToken) {
            validateMillisecondsTimeout(millisecondsTimeout);
            cancellationToken.ThrowIfCancellationRequested();
            CancellationNotification cancellationNotification(cancellationToken, itemsAvailable_, spaceAvailable_);
            std::unique_lock<std::mutex> lock(mutex_);
            checkDisposedLocked();
            if (addingCompleted_) {
                throw System::InvalidOperationException("The collection has been marked as complete with regards to additions.");
            }

            const auto readyForAdd = [this, &cancellationToken]() {
                return disposed_ || addingCompleted_ || cancellationToken.getIsCancellationRequestedProperty() ||
                       boundedCapacity_ == NonBounded || collection_->getCountProperty() < boundedCapacity_;
            };

            bool ready = true;
            if (boundedCapacity_ != NonBounded && collection_->getCountProperty() >= boundedCapacity_) {
                if (millisecondsTimeout == System::Threading::Timeout::Infinite) {
                    spaceAvailable_.wait(lock, readyForAdd);
                } else {
                    ready = spaceAvailable_.wait_for(lock, std::chrono::milliseconds(millisecondsTimeout), readyForAdd);
                }
            }

            cancellationToken.ThrowIfCancellationRequested();
            checkDisposedLocked();
            if (addingCompleted_) {
                throw System::InvalidOperationException("The collection has been marked as complete with regards to additions.");
            }
            if (!ready || (boundedCapacity_ != NonBounded && collection_->getCountProperty() >= boundedCapacity_)) {
                return false;
            }
            if (!collection_->TryAdd(item)) {
                throw System::InvalidOperationException("The underlying collection did not accept the item.");
            }
            lock.unlock();
            itemsAvailable_.notify_one();
            return true;
        }

        bool tryTakeCore(T& item, intcs millisecondsTimeout,
                         const System::Threading::CancellationToken& cancellationToken) {
            validateMillisecondsTimeout(millisecondsTimeout);
            cancellationToken.ThrowIfCancellationRequested();
            CancellationNotification cancellationNotification(cancellationToken, itemsAvailable_, spaceAvailable_);
            std::unique_lock<std::mutex> lock(mutex_);
            checkDisposedLocked();

            const auto canTake = [this, &cancellationToken]() {
                return disposed_ || cancellationToken.getIsCancellationRequestedProperty() ||
                       collection_->getCountProperty() > 0 || addingCompleted_;
            };

            const auto deadline = millisecondsTimeout == System::Threading::Timeout::Infinite
                ? std::chrono::steady_clock::time_point::max()
                : std::chrono::steady_clock::now() + std::chrono::milliseconds(millisecondsTimeout);

            while (true) {
                cancellationToken.ThrowIfCancellationRequested();
                checkDisposedLocked();
                if (collection_->TryTake(item)) {
                    lock.unlock();
                    spaceAvailable_.notify_one();
                    return true;
                }
                if (addingCompleted_) {
                    return false;
                }
                if (millisecondsTimeout == 0) {
                    return false;
                }

                bool awakened;
                if (millisecondsTimeout == System::Threading::Timeout::Infinite) {
                    itemsAvailable_.wait(lock, canTake);
                    awakened = true;
                } else {
                    awakened = itemsAvailable_.wait_until(lock, deadline, canTake);
                }
                if (!awakened) {
                    return false;
                }
            }
        }

        static void validateCollections(const std::vector<BlockingCollection<T>*>& collections, bool adding) {
            if (collections.empty()) {
                throw System::ArgumentException("The collections vector must contain at least one collection.", "collections");
            }
            for (BlockingCollection<T>* collection : collections) {
                if (collection == nullptr) {
                    throw System::ArgumentNullException("collections");
                }
                collection->checkDisposed();
                if (adding && collection->getIsAddingCompletedProperty()) {
                    throw System::ArgumentException("A collection has been marked as complete with regards to additions.", "collections");
                }
            }
        }

        static bool allCompleted(const std::vector<BlockingCollection<T>*>& collections) {
            return std::all_of(collections.begin(), collections.end(),
                               [](const BlockingCollection<T>* collection) { return collection->getIsCompletedProperty(); });
        }

        static bool timeoutExpired(std::chrono::steady_clock::time_point deadline, intcs millisecondsTimeout) {
            return millisecondsTimeout != System::Threading::Timeout::Infinite &&
                   std::chrono::steady_clock::now() >= deadline;
        }

    public:
        class ConsumingEnumerable;

        /** Initializes an unbounded collection backed by ConcurrentQueue<T>. */
        BlockingCollection() {
            initialize(std::make_shared<ConcurrentQueue<T>>(), NonBounded);
        }

        /** Initializes a bounded collection backed by ConcurrentQueue<T>. */
        explicit BlockingCollection(intcs boundedCapacity) {
            initialize(std::make_shared<ConcurrentQueue<T>>(), boundedCapacity);
        }

        /** Initializes an unbounded collection using a non-owning reference to @p collection. */
        explicit BlockingCollection(IProducerConsumerCollection<T>& collection) {
            initialize(std::shared_ptr<IProducerConsumerCollection<T>>(&collection, [](IProducerConsumerCollection<T>*) {}), NonBounded);
        }

        /** Initializes a bounded collection using a non-owning reference to @p collection. */
        BlockingCollection(IProducerConsumerCollection<T>& collection, intcs boundedCapacity) {
            initialize(std::shared_ptr<IProducerConsumerCollection<T>>(&collection, [](IProducerConsumerCollection<T>*) {}), boundedCapacity);
        }

        /** Initializes an unbounded collection taking shared ownership of @p collection. */
        explicit BlockingCollection(std::shared_ptr<IProducerConsumerCollection<T>> collection) {
            initialize(std::move(collection), NonBounded);
        }

        /** Initializes a bounded collection taking shared ownership of @p collection. */
        BlockingCollection(std::shared_ptr<IProducerConsumerCollection<T>> collection, intcs boundedCapacity) {
            initialize(std::move(collection), boundedCapacity);
        }

        /** Disposes the collection when it leaves scope. */
        ~BlockingCollection() override { Dispose(); }

        /** BlockingCollection<T> is not copyable because it owns synchronization state. */
        BlockingCollection(const BlockingCollection&) = delete;
        /** BlockingCollection<T> is not copy-assignable because it owns synchronization state. */
        BlockingCollection& operator=(const BlockingCollection&) = delete;
        /** Moving a collection with active waiters is unsupported. */
        BlockingCollection(BlockingCollection&&) = delete;
        /** Moving a collection with active waiters is unsupported. */
        BlockingCollection& operator=(BlockingCollection&&) = delete;

        /** Returns the bounded capacity, or -1 for an unbounded collection. */
        [[nodiscard]] intcs getBoundedCapacityProperty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            checkDisposedLocked();
            return boundedCapacity_;
        }

        /** Returns the current number of items. */
        [[nodiscard]] intcs getCountProperty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            checkDisposedLocked();
            return collection_->getCountProperty();
        }

        /** Returns whether no more items may be added. */
        [[nodiscard]] bool getIsAddingCompletedProperty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            checkDisposedLocked();
            return addingCompleted_;
        }

        /** Returns whether adding is complete and the collection is empty. */
        [[nodiscard]] bool getIsCompletedProperty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            checkDisposedLocked();
            return addingCompleted_ && collection_->getCountProperty() == 0;
        }

        /** Adds @p item, blocking until a slot is available for bounded collections. */
        void Add(const T& item) { (void)tryAddCore(item, System::Threading::Timeout::Infinite, System::Threading::CancellationToken::None()); }

        /** Adds @p item while observing @p cancellationToken. */
        void Add(const T& item, const System::Threading::CancellationToken& cancellationToken) {
            (void)tryAddCore(item, System::Threading::Timeout::Infinite, cancellationToken);
        }

        /** Attempts to add @p item without waiting. */
        bool TryAdd(const T& item) { return tryAddCore(item, 0, System::Threading::CancellationToken::None()); }

        /** Attempts to add @p item within @p timeout. */
        bool TryAdd(const T& item, const System::TimeSpan& timeout) {
            return tryAddCore(item, validateTimeout(timeout), System::Threading::CancellationToken::None());
        }

        /** Attempts to add @p item within @p millisecondsTimeout milliseconds (-1 is infinite). */
        bool TryAdd(const T& item, intcs millisecondsTimeout) {
            return tryAddCore(item, millisecondsTimeout, System::Threading::CancellationToken::None());
        }

        /** Attempts to add @p item within a timeout while observing @p cancellationToken. */
        bool TryAdd(const T& item, intcs millisecondsTimeout,
                    const System::Threading::CancellationToken& cancellationToken) {
            return tryAddCore(item, millisecondsTimeout, cancellationToken);
        }

        /** Marks this collection as not accepting further additions and wakes all waiters. */
        void CompleteAdding() {
            std::lock_guard<std::mutex> lock(mutex_);
            checkDisposedLocked();
            addingCompleted_ = true;
            itemsAvailable_.notify_all();
            spaceAvailable_.notify_all();
        }

        /** Copies a point-in-time snapshot to @p array starting at @p index. */
        void CopyTo(std::vector<T>& array, intcs index) {
            checkDisposed();
            collection_->CopyTo(array, index);
        }

        /** Returns a point-in-time snapshot of the underlying collection. */
        [[nodiscard]] std::vector<T> ToArray() const {
            std::lock_guard<std::mutex> lock(mutex_);
            checkDisposedLocked();
            return collection_->ToArray();
        }

        /** Returns a non-consuming point-in-time snapshot enumerator. */
        [[nodiscard]] System::Collections::Generic::IEnumerator<T>* GetEnumerator() override {
            return new SnapshotEnumerator(ToArray());
        }

        /** Removes and returns an item, blocking until one is available or the collection completes. */
        T Take() {
            T item{};
            if (!tryTakeCore(item, System::Threading::Timeout::Infinite, System::Threading::CancellationToken::None())) {
                throw System::InvalidOperationException("The collection is empty and has been marked as complete with regards to additions.");
            }
            return item;
        }

        /** Removes and returns an item while observing @p cancellationToken. */
        T Take(const System::Threading::CancellationToken& cancellationToken) {
            T item{};
            if (!tryTakeCore(item, System::Threading::Timeout::Infinite, cancellationToken)) {
                throw System::InvalidOperationException("The collection is empty and has been marked as complete with regards to additions.");
            }
            return item;
        }

        /** Attempts to remove an item without waiting. */
        bool TryTake(T& item) { return tryTakeCore(item, 0, System::Threading::CancellationToken::None()); }

        /** Attempts to remove an item within @p timeout. */
        bool TryTake(T& item, const System::TimeSpan& timeout) {
            return tryTakeCore(item, validateTimeout(timeout), System::Threading::CancellationToken::None());
        }

        /** Attempts to remove an item within @p millisecondsTimeout milliseconds (-1 is infinite). */
        bool TryTake(T& item, intcs millisecondsTimeout) {
            return tryTakeCore(item, millisecondsTimeout, System::Threading::CancellationToken::None());
        }

        /** Attempts to remove an item within a timeout while observing @p cancellationToken. */
        bool TryTake(T& item, intcs millisecondsTimeout,
                     const System::Threading::CancellationToken& cancellationToken) {
            return tryTakeCore(item, millisecondsTimeout, cancellationToken);
        }

        /** Returns an enumerable whose enumerator consumes items until adding is complete. */
        [[nodiscard]] ConsumingEnumerable GetConsumingEnumerable();

        /** Returns a consuming enumerable whose enumerator observes @p cancellationToken. */
        [[nodiscard]] ConsumingEnumerable GetConsumingEnumerable(const System::Threading::CancellationToken& cancellationToken);

        /** Disposes this collection and wakes all currently blocked producers and consumers. */
        void Dispose() override {
            std::lock_guard<std::mutex> lock(mutex_);
            if (disposed_) {
                return;
            }
            disposed_ = true;
            addingCompleted_ = true;
            itemsAvailable_.notify_all();
            spaceAvailable_.notify_all();
        }

        /** Adds @p item to one of @p collections, blocking until successful. */
        static intcs AddToAny(const std::vector<BlockingCollection<T>*>& collections, const T& item) {
            const intcs index = TryAddToAny(collections, item, System::Threading::Timeout::Infinite,
                                            System::Threading::CancellationToken::None());
            if (index < 0) {
                throw System::ArgumentException("No collection can accept an item.", "collections");
            }
            return index;
        }

        /** Adds @p item to one of @p collections while observing @p cancellationToken. */
        static intcs AddToAny(const std::vector<BlockingCollection<T>*>& collections, const T& item,
                              const System::Threading::CancellationToken& cancellationToken) {
            const intcs index = TryAddToAny(collections, item, System::Threading::Timeout::Infinite, cancellationToken);
            if (index < 0) {
                throw System::ArgumentException("No collection can accept an item.", "collections");
            }
            return index;
        }

        /** Attempts to add @p item to any collection without waiting. */
        static intcs TryAddToAny(const std::vector<BlockingCollection<T>*>& collections, const T& item) {
            return TryAddToAny(collections, item, 0, System::Threading::CancellationToken::None());
        }

        /** Attempts to add @p item to any collection within @p timeout. */
        static intcs TryAddToAny(const std::vector<BlockingCollection<T>*>& collections, const T& item,
                                 const System::TimeSpan& timeout) {
            return TryAddToAny(collections, item, validateTimeout(timeout), System::Threading::CancellationToken::None());
        }

        /** Attempts to add @p item to any collection within @p millisecondsTimeout milliseconds. */
        static intcs TryAddToAny(const std::vector<BlockingCollection<T>*>& collections, const T& item,
                                 intcs millisecondsTimeout) {
            return TryAddToAny(collections, item, millisecondsTimeout, System::Threading::CancellationToken::None());
        }

        /** Attempts to add @p item to any collection while observing @p cancellationToken. */
        static intcs TryAddToAny(const std::vector<BlockingCollection<T>*>& collections, const T& item,
                                 intcs millisecondsTimeout,
                                 const System::Threading::CancellationToken& cancellationToken) {
            validateMillisecondsTimeout(millisecondsTimeout);
            validateCollections(collections, true);
            cancellationToken.ThrowIfCancellationRequested();
            const auto deadline = millisecondsTimeout == System::Threading::Timeout::Infinite
                ? std::chrono::steady_clock::time_point::max()
                : std::chrono::steady_clock::now() + std::chrono::milliseconds(millisecondsTimeout);
            do {
                cancellationToken.ThrowIfCancellationRequested();
                for (std::size_t index = 0; index < collections.size(); ++index) {
                    if (collections[index]->TryAdd(item, 0, cancellationToken)) {
                        return static_cast<intcs>(index);
                    }
                }
                if (timeoutExpired(deadline, millisecondsTimeout)) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } while (true);
            return -1;
        }

        /** Removes and returns an item from any collection, blocking until successful. */
        static intcs TakeFromAny(const std::vector<BlockingCollection<T>*>& collections, T& item) {
            const intcs index = TryTakeFromAny(collections, item, System::Threading::Timeout::Infinite,
                                               System::Threading::CancellationToken::None());
            if (index < 0) {
                throw System::ArgumentException("All collections are completed.", "collections");
            }
            return index;
        }

        /** Removes and returns an item from any collection while observing @p cancellationToken. */
        static intcs TakeFromAny(const std::vector<BlockingCollection<T>*>& collections, T& item,
                                 const System::Threading::CancellationToken& cancellationToken) {
            const intcs index = TryTakeFromAny(collections, item, System::Threading::Timeout::Infinite, cancellationToken);
            if (index < 0) {
                throw System::ArgumentException("All collections are completed.", "collections");
            }
            return index;
        }

        /** Attempts to remove an item from any collection without waiting. */
        static intcs TryTakeFromAny(const std::vector<BlockingCollection<T>*>& collections, T& item) {
            return TryTakeFromAny(collections, item, 0, System::Threading::CancellationToken::None());
        }

        /** Attempts to remove an item from any collection within @p timeout. */
        static intcs TryTakeFromAny(const std::vector<BlockingCollection<T>*>& collections, T& item,
                                    const System::TimeSpan& timeout) {
            return TryTakeFromAny(collections, item, validateTimeout(timeout), System::Threading::CancellationToken::None());
        }

        /** Attempts to remove an item from any collection within @p millisecondsTimeout milliseconds. */
        static intcs TryTakeFromAny(const std::vector<BlockingCollection<T>*>& collections, T& item,
                                    intcs millisecondsTimeout) {
            return TryTakeFromAny(collections, item, millisecondsTimeout, System::Threading::CancellationToken::None());
        }

        /** Attempts to remove an item from any collection while observing @p cancellationToken. */
        static intcs TryTakeFromAny(const std::vector<BlockingCollection<T>*>& collections, T& item,
                                    intcs millisecondsTimeout,
                                    const System::Threading::CancellationToken& cancellationToken) {
            validateMillisecondsTimeout(millisecondsTimeout);
            validateCollections(collections, false);
            cancellationToken.ThrowIfCancellationRequested();
            const auto deadline = millisecondsTimeout == System::Threading::Timeout::Infinite
                ? std::chrono::steady_clock::time_point::max()
                : std::chrono::steady_clock::now() + std::chrono::milliseconds(millisecondsTimeout);
            do {
                cancellationToken.ThrowIfCancellationRequested();
                for (std::size_t index = 0; index < collections.size(); ++index) {
                    if (collections[index]->TryTake(item, 0, cancellationToken)) {
                        return static_cast<intcs>(index);
                    }
                }
                if (allCompleted(collections) || timeoutExpired(deadline, millisecondsTimeout)) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } while (true);
            return -1;
        }
    };

    /** A consuming enumerable returned by BlockingCollection<T>::GetConsumingEnumerable(). */
    template<typename T>
    class BlockingCollection<T>::ConsumingEnumerable final : public System::Collections::Generic::IEnumerable<T> {
        BlockingCollection<T>* owner_;
        System::Threading::CancellationToken cancellationToken_;

        class Enumerator final : public System::Collections::Generic::IEnumerator<T> {
            BlockingCollection<T>* owner_;
            System::Threading::CancellationToken cancellationToken_;
            T current_{};
            bool hasCurrent_ = false;

        public:
            Enumerator(BlockingCollection<T>* owner, System::Threading::CancellationToken cancellationToken)
                : owner_(owner), cancellationToken_(std::move(cancellationToken)) {}

            bool MoveNext() override {
                hasCurrent_ = owner_->TryTake(current_, System::Threading::Timeout::Infinite, cancellationToken_);
                return hasCurrent_;
            }

            void Reset() override {
                throw System::NotSupportedException("Reset is not supported for a consuming enumerator.");
            }

            [[nodiscard]] const T& Current() const override { return current_; }
        };

    public:
        /** Constructs a consuming enumerable for @p owner that observes @p cancellationToken. */
        ConsumingEnumerable(BlockingCollection<T>* owner, System::Threading::CancellationToken cancellationToken)
            : owner_(owner), cancellationToken_(std::move(cancellationToken)) {}

        /** Returns an enumerator that removes one item on each successful MoveNext(). */
        [[nodiscard]] System::Collections::Generic::IEnumerator<T>* GetEnumerator() override {
            return new Enumerator(owner_, cancellationToken_);
        }
    };

    template<typename T>
    typename BlockingCollection<T>::ConsumingEnumerable BlockingCollection<T>::GetConsumingEnumerable() {
        checkDisposed();
        return ConsumingEnumerable(this, System::Threading::CancellationToken::None());
    }

    template<typename T>
    typename BlockingCollection<T>::ConsumingEnumerable
    BlockingCollection<T>::GetConsumingEnumerable(const System::Threading::CancellationToken& cancellationToken) {
        checkDisposed();
        cancellationToken.ThrowIfCancellationRequested();
        return ConsumingEnumerable(this, cancellationToken);
    }

} // namespace System::Collections::Concurrent
