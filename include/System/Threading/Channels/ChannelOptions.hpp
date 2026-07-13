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
    };

    /**
     * @brief Options controlling the behavior of instances created by Channel::CreateBounded().
     *
     * C++ counterpart of .NET System.Threading.Channels.BoundedChannelOptions.
     */
    class BoundedChannelOptions : public ChannelOptions {
        SharpRuntime::intcs capacity_;

    public:
        BoundedChannelFullMode FullMode = BoundedChannelFullMode::Wait;

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
     *
     * @note Real .NET pairs this type with a `Channel.CreateUnboundedPrioritized<T>(options)`
     * factory that backs the channel with a priority queue (items dequeued by @p Comparer order,
     * not insertion order) instead of the plain FIFO `std::deque` this port's `Channel<T>` uses
     * internally. That factory -- and the priority-queue-backed reader/writer/state variant it
     * needs -- is NOT implemented in this port; this options type currently has no way to
     * actually be used to create a channel. Documented here as a known, disclosed gap rather than
     * silently pretending the type is functional -- implementing it properly needs a genuinely
     * different queueing backend, not a small mechanical addition, so it's deferred rather than
     * attempted in a single audit pass.
     */
    template<typename T>
    class UnboundedPrioritizedChannelOptions : public ChannelOptions {
    public:
        /** @brief The comparer used to prioritize elements, or nullptr to use operator&lt;. */
        std::function<bool(const T&, const T&)> Comparer;
    };

} // namespace System::Threading::Channels
