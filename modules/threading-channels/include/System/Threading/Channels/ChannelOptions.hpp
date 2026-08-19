// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <memory>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Threading/Channels/BoundedChannelFullMode.hpp"

namespace System::Threading::Channels {

    /**
     * @brief Provides options that control the behavior of channel instances.
     *
     * C++ counterpart of .NET System.Threading.Channels.ChannelOptions.
     */
    class ChannelOptions {
    public:
        virtual ~ChannelOptions() = default;

        /** @brief true if writers to the channel guarantee at most one concurrent write operation. */
        bool SingleWriter = false;
        /** @brief true if readers from the channel guarantee at most one concurrent read operation. */
        bool SingleReader = false;
        /** @brief true if continuations may be invoked synchronously on the thread completing an operation. */
        bool AllowSynchronousContinuations = false;

        // DELIBERATELY still public data members, and the boundary is pinned by a test rather
        // than left to look like an oversight. .NET's three are plain auto-properties with no
        // validation at all (ChannelOptions.cs:17,27,39) -- `{ get; set; }` and nothing else --
        // so a public field is observationally identical to the reference. SA-8 reaches a
        // representation .NET keeps private, readonly or absent; it does not reach one .NET
        // publishes as freely as this. Converting them would be a source break that buys no
        // behaviour, which is the opposite of #1969's case: there, the shape was the only thing
        // preventing a check that .NET performs.
    };

    /**
     * @brief Options controlling the behavior of instances created by Channel::CreateBounded().
     *
     * C++ counterpart of .NET System.Threading.Channels.BoundedChannelOptions.
     */
    class BoundedChannelOptions : public ChannelOptions {
        SharpRuntime::intcs    capacity_;
        // PRIVATE, matching .NET's `private BoundedChannelFullMode _mode`
        // (ChannelOptions.cs:47). It was a bare public data member, and the obstacle was the
        // field's SHAPE rather than any missing logic: a data member has nowhere to put a
        // check, so an undeclared value could be assigned and then defeat the channel's
        // bounded-memory contract outright -- with capacity 1, `static_cast<
        // BoundedChannelFullMode>(99)` made the writer take the drop path (the mode is not
        // Wait) and then match no arm of the drop switch, so nothing was dropped and Count
        // reached 2. Ticket #1969, under docs/StandingApprovals.md SA-8.
        BoundedChannelFullMode fullMode_ = BoundedChannelFullMode::Wait;

    public:
        /** @throws System::ArgumentOutOfRangeException if @p capacity is negative. */
        explicit BoundedChannelOptions(SharpRuntime::intcs capacity) : capacity_(capacity) {
            System::ArgumentOutOfRangeException::ThrowIfNegative(capacity, "capacity");
        }

        /** @return The maximum number of items the bounded channel may store. */
        [[nodiscard]] SharpRuntime::intcs getCapacityProperty() const { return capacity_; }
        /** @brief Sets the maximum number of items the bounded channel may store. */
        void setCapacityProperty(SharpRuntime::intcs value) {
            System::ArgumentOutOfRangeException::ThrowIfNegative(value, "value");
            capacity_ = value;
        }

        /** @return The behavior incurred by write operations when the channel is full. */
        [[nodiscard]] BoundedChannelFullMode getFullModeProperty() const noexcept { return fullMode_; }

        /**
         * @brief Sets the behavior incurred by write operations when the channel is full.
         * @param value One of the four declared BoundedChannelFullMode values.
         * @throws System::ArgumentOutOfRangeException if @p value is not a declared enumerator.
         *
         * Transcribed from .NET's `FullMode` setter (ChannelOptions.cs:80-97): a four-arm
         * switch with a `default` that throws `ArgumentOutOfRangeException(nameof(value))` --
         * so the parameter name is **"value"**, not "FullMode", which is what a caller reading
         * the message will see.
         */
        void setFullModeProperty(BoundedChannelFullMode value) {
            switch (value) {
                case BoundedChannelFullMode::Wait:
                case BoundedChannelFullMode::DropNewest:
                case BoundedChannelFullMode::DropOldest:
                case BoundedChannelFullMode::DropWrite:
                    fullMode_ = value;
                    break;
                default:
                    throw System::ArgumentOutOfRangeException("value");
            }
        }
    };

    /**
     * @brief Options controlling the behavior of instances created by Channel::CreateUnbounded().
     *
     * C++ counterpart of .NET System.Threading.Channels.UnboundedChannelOptions.
     */
    class UnboundedChannelOptions : public ChannelOptions {};

    /**
     * @brief Options controlling the behavior of a priority-ordered unbounded channel.
     *
     * C++ counterpart of .NET System.Threading.Channels.UnboundedPrioritizedChannelOptions<T>.
     * Paired with `Channel<T>::CreateUnboundedPrioritized(options)`, which backs the channel with
     * a `std::multiset<T, Comparer>` (items dequeued in ascending @p Comparer order -- the
     * smallest item first, not insertion order) instead of the plain FIFO `std::deque` this
     * port's ordinary `Channel<T>` uses internally. Verified against real .NET's
     * `UnboundedPrioritizedChannel<T>` (backed by `PriorityQueue<bool, T>`), including its
     * `TryPeek`/`Count` support and its "unbounded writes always succeed until closed" contract.
     */
    template<typename T>
    class UnboundedPrioritizedChannelOptions : public ChannelOptions {
    public:
        /** @brief The comparer used to prioritize elements, or nullptr to use operator&lt;. */
        std::function<bool(const T&, const T&)> Comparer;
    };

} // namespace System::Threading::Channels
