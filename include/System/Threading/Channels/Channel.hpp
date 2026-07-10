// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/NotSupportedException.hpp"
#include "System/Threading/Channels/ChannelClosedException.hpp"
#include "System/Threading/Channels/ChannelOptions.hpp"
#include "System/Threading/Tasks/Task.hpp"

namespace System::Threading::Channels {

    /**
     * @brief Provides a base class for reading from a channel.
     *
     * C++ counterpart of .NET System.Threading.Channels.ChannelReader<T>.
     *
     * @note No `CancellationToken`/`IAsyncEnumerable<T>` support (`ReadAllAsync` isn't
     * reproduced) — this runtime's Task type has no async-enumerable idiom; drain a channel with
     * a `while (WaitToReadAsync) { while (TryRead) { ... } }` loop instead, matching
     * `ReadAllAsync`'s own documented implementation.
     */
    template<typename T>
    class ChannelReader {
    public:
        virtual ~ChannelReader() = default;

        /** @brief Attempts to read an item from the channel without blocking. */
        virtual bool TryRead(T& item) = 0;

        /** @brief Attempts to peek at the next item without removing it. Returns false if unsupported. */
        virtual bool TryPeek(T& /*item*/) { return false; }

        /** @return true if TryPeek() is supported by this instance. */
        [[nodiscard]] virtual bool getCanPeekProperty() const { return false; }

        /** @return true if getCountProperty() is supported by this instance. */
        [[nodiscard]] virtual bool getCanCountProperty() const { return false; }

        /** @return The current number of items available. @throws System::NotSupportedException if unsupported. */
        [[nodiscard]] virtual SharpRuntime::intcs getCountProperty() const {
            throw System::NotSupportedException("Counting is not supported on this channel reader.");
        }

        /** @brief Completes with true when data is available to read, or false when the channel is closed and drained. */
        virtual System::Threading::Tasks::TaskT<bool> WaitToReadAsync() = 0;

        /** @brief A Task that completes once the channel is closed and fully drained. */
        [[nodiscard]] virtual System::Threading::Tasks::Task getCompletionProperty() const = 0;

        /** @brief Reads an item, waiting for one to become available if necessary. */
        virtual System::Threading::Tasks::TaskT<T> ReadAsync() {
            return System::Threading::Tasks::TaskT<T>([this]() {
                T item{};
                while (true) {
                    if (TryRead(item)) return item;
                    if (!WaitToReadAsync().getResultProperty()) {
                        throw ChannelClosedException();
                    }
                }
            });
        }
    };

    /**
     * @brief Provides a base class for writing to a channel.
     *
     * C++ counterpart of .NET System.Threading.Channels.ChannelWriter<T>.
     */
    template<typename T>
    class ChannelWriter {
    public:
        virtual ~ChannelWriter() = default;

        /** @brief Attempts to write an item to the channel without blocking. */
        virtual bool TryWrite(const T& item) = 0;

        /** @brief Completes with true when space is available to write, or false when the channel will accept no more writes. */
        virtual System::Threading::Tasks::TaskT<bool> WaitToWriteAsync() = 0;

        /** @brief Attempts to mark the channel complete. @return false if already completed. */
        virtual bool TryComplete(std::exception_ptr error = nullptr) {
            (void)error;
            return false;
        }

        /** @brief Marks the channel complete. @throws ChannelClosedException if already completed. */
        void Complete(std::exception_ptr error = nullptr) {
            if (!TryComplete(error)) {
                throw ChannelClosedException("The channel has already been completed.");
            }
        }

        /** @brief Writes an item, waiting for space if necessary. */
        virtual System::Threading::Tasks::Task WriteAsync(T item) {
            return System::Threading::Tasks::Task([this, item = std::move(item)]() mutable {
                while (!TryWrite(item)) {
                    if (!WaitToWriteAsync().getResultProperty()) {
                        throw ChannelClosedException();
                    }
                }
            });
        }
    };

    namespace detail {

        template<typename T>
        struct ChannelState {
            std::mutex mutex;
            std::condition_variable notEmpty;
            std::condition_variable notFull;
            std::deque<T> queue;
            bool closed = false;
            std::exception_ptr closeError;
            SharpRuntime::intcs capacity = -1; // -1 == unbounded
            BoundedChannelFullMode fullMode = BoundedChannelFullMode::Wait;

            // Verified against BoundedChannel.cs's TryWrite: a configured capacity of 0 (a
            // documented legal "rendezvous channel") is handled there by a queue-transition-from-
            // 0-to-1 special case that still buffers one item when no reader is synchronously
            // blocked waiting -- i.e. a capacity-0 channel is observably equivalent to a
            // capacity-1 channel for every publicly-visible TryWrite/WaitToWriteAsync outcome (the
            // only difference is an internal direct-handoff-to-a-blocked-reader optimization that
            // doesn't change any return value this port's simpler mutex/condvar model produces).
            // Previously this port used `capacity` directly in its "is full" comparisons, so a
            // capacity-0 channel had queue.size() (0) >= capacity (0) true immediately -- every
            // write blocked forever, even with a reader concurrently waiting to receive.
            [[nodiscard]] SharpRuntime::intcs effectiveCapacity() const { return capacity == 0 ? 1 : capacity; }
        };

        template<typename T>
        class ChannelReaderImpl : public ChannelReader<T> {
            std::shared_ptr<ChannelState<T>> state_;

        public:
            explicit ChannelReaderImpl(std::shared_ptr<ChannelState<T>> state) : state_(std::move(state)) {}

            bool TryRead(T& item) override {
                std::lock_guard<std::mutex> lock(state_->mutex);
                if (state_->queue.empty()) return false;
                item = std::move(state_->queue.front());
                state_->queue.pop_front();
                state_->notFull.notify_one();
                // Also wake any Completion waiter: it's blocked on notEmpty until closed &&
                // queue.empty(), and draining the last item is exactly the transition it's
                // waiting for.
                state_->notEmpty.notify_all();
                return true;
            }

            [[nodiscard]] bool getCanCountProperty() const override { return true; }

            [[nodiscard]] SharpRuntime::intcs getCountProperty() const override {
                std::lock_guard<std::mutex> lock(state_->mutex);
                return static_cast<SharpRuntime::intcs>(state_->queue.size());
            }

            // Verified against UnboundedChannel.cs's WaitToReadAsync: when there are no items
            // left and writing is done, the returned task faults with the completion exception
            // (if any) rather than resolving to false. This previously always returned false
            // once closed, silently discarding a real completion error -- it was only
            // observable via the separate getCompletionProperty() task, not through the
            // primary WaitToReadAsync/ReadAsync read path.
            System::Threading::Tasks::TaskT<bool> WaitToReadAsync() override {
                auto state = state_;
                return System::Threading::Tasks::TaskT<bool>([state]() -> bool {
                    std::unique_lock<std::mutex> lock(state->mutex);
                    state->notEmpty.wait(lock, [&] { return !state->queue.empty() || state->closed; });
                    if (!state->queue.empty()) return true;
                    if (state->closeError) std::rethrow_exception(state->closeError);
                    return false;
                });
            }

            [[nodiscard]] System::Threading::Tasks::Task getCompletionProperty() const override {
                auto state = state_;
                return System::Threading::Tasks::Task([state]() {
                    std::unique_lock<std::mutex> lock(state->mutex);
                    state->notEmpty.wait(lock, [&] { return state->closed && state->queue.empty(); });
                    if (state->closeError) std::rethrow_exception(state->closeError);
                });
            }
        };

        template<typename T>
        class ChannelWriterImpl : public ChannelWriter<T> {
            std::shared_ptr<ChannelState<T>> state_;

        public:
            explicit ChannelWriterImpl(std::shared_ptr<ChannelState<T>> state) : state_(std::move(state)) {}

            bool TryWrite(const T& item) override {
                std::lock_guard<std::mutex> lock(state_->mutex);
                if (state_->closed) return false;

                if (state_->capacity >= 0 && static_cast<SharpRuntime::intcs>(state_->queue.size()) >= state_->effectiveCapacity()) {
                    switch (state_->fullMode) {
                        case BoundedChannelFullMode::Wait:
                            return false;
                        case BoundedChannelFullMode::DropWrite:
                            return true; // item is dropped, but the write is considered handled
                        case BoundedChannelFullMode::DropNewest:
                            if (!state_->queue.empty()) state_->queue.pop_back();
                            break;
                        case BoundedChannelFullMode::DropOldest:
                            if (!state_->queue.empty()) state_->queue.pop_front();
                            break;
                    }
                }

                state_->queue.push_back(item);
                state_->notEmpty.notify_one();
                return true;
            }

            // Verified against UnboundedChannel.cs's WaitToWriteAsync: once the channel is
            // closed, the returned task faults with the completion exception (if any) rather
            // than resolving to false. Same rationale as WaitToReadAsync's fix above.
            System::Threading::Tasks::TaskT<bool> WaitToWriteAsync() override {
                auto state = state_;
                return System::Threading::Tasks::TaskT<bool>([state]() -> bool {
                    std::unique_lock<std::mutex> lock(state->mutex);
                    state->notFull.wait(lock, [&] {
                        return state->closed || state->capacity < 0 ||
                               static_cast<SharpRuntime::intcs>(state->queue.size()) < state->effectiveCapacity() ||
                               state->fullMode != BoundedChannelFullMode::Wait;
                    });
                    if (!state->closed) return true;
                    if (state->closeError) std::rethrow_exception(state->closeError);
                    return false;
                });
            }

            bool TryComplete(std::exception_ptr error) override {
                std::lock_guard<std::mutex> lock(state_->mutex);
                if (state_->closed) return false;
                state_->closed = true;
                state_->closeError = error;
                state_->notEmpty.notify_all();
                state_->notFull.notify_all();
                return true;
            }
        };

    } // namespace detail

    /**
     * @brief Provides static methods for creating channels.
     *
     * C++ counterpart of .NET System.Threading.Channels.Channel (the static factory) — this
     * class's instances model .NET's `Channel<T>` (the Reader/Writer pair), since C++ has no
     * separate non-generic/generic overload of the same type name the way C# does.
     *
     * @note Backed by `std::mutex`/`std::condition_variable`, not a lock-free algorithm — simpler
     * and adequate for typical game producer/consumer workloads, but not tuned for extreme
     * contention the way .NET's real implementation is.
     */
    template<typename T>
    class Channel {
    public:
        std::shared_ptr<ChannelReader<T>> Reader;
        std::shared_ptr<ChannelWriter<T>> Writer;

        /** @brief Creates an unbounded channel. */
        [[nodiscard]] static Channel<T> CreateUnbounded(const UnboundedChannelOptions& = UnboundedChannelOptions()) {
            auto state = std::make_shared<detail::ChannelState<T>>();
            Channel<T> channel;
            channel.Reader = std::make_shared<detail::ChannelReaderImpl<T>>(state);
            channel.Writer = std::make_shared<detail::ChannelWriterImpl<T>>(state);
            return channel;
        }

        /** @brief Creates a bounded channel with the given capacity and BoundedChannelFullMode::Wait. */
        [[nodiscard]] static Channel<T> CreateBounded(SharpRuntime::intcs capacity) {
            return CreateBounded(BoundedChannelOptions(capacity));
        }

        /** @brief Creates a bounded channel with the given options. */
        [[nodiscard]] static Channel<T> CreateBounded(const BoundedChannelOptions& options) {
            auto state = std::make_shared<detail::ChannelState<T>>();
            state->capacity = options.getCapacityProperty();
            state->fullMode = options.FullMode;
            Channel<T> channel;
            channel.Reader = std::make_shared<detail::ChannelReaderImpl<T>>(state);
            channel.Writer = std::make_shared<detail::ChannelWriterImpl<T>>(state);
            return channel;
        }
    };

} // namespace System::Threading::Channels
