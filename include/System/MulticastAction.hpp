// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace System {

    /**
     * @brief A multicast delegate returning void, matching a .NET System.Action<Args...> used as a
     *        field or event (which are multicast via `+=` in C#).
     *
     * Unlike @ref System::ActionT (a single-subscriber `std::function` alias), this holds a list of
     * subscribed handlers, keeping the same `void(Args...)` signature:
     *  - `operator+=` adds a subscriber (C# `+=`);
     *  - `operator=` replaces the whole list with a single handler (C# `field = handler;`) or clears
     *    it (`field = null;` via `= nullptr`);
     *  - invocation calls every subscriber in subscription order over a snapshot of the list, so a
     *    handler that subscribes/unsubscribes during invocation only affects the next invocation
     *    (matching .NET multicast-delegate semantics; also makes invocation re-entrancy safe).
     *
     * @tparam Args The delegate parameter types.
     */
    template<typename... Args>
    class MulticastAction {
    public:
        /** @brief The stored handler type: `void(Args...)`. */
        using HandlerType = std::function<void(Args...)>;

        /** @brief Creates an empty multicast action (no subscribers). */
        MulticastAction() = default;

        /**
         * @brief Replaces all subscribers with a single handler (C# `field = handler;`).
         * @param handler The handler to set; an empty handler clears the list.
         * @return A reference to this action.
         */
        MulticastAction& operator=(HandlerType handler) {
            handlers_.clear();
            if (handler) {
                handlers_.push_back(std::move(handler));
            }
            return *this;
        }

        /**
         * @brief Removes all subscribers (C# `field = null;`).
         * @return A reference to this action.
         */
        MulticastAction& operator=(std::nullptr_t) {
            handlers_.clear();
            return *this;
        }

        /**
         * @brief Adds a subscriber to the invocation list (C# `+=`).
         * @param handler The handler to add; an empty handler is ignored.
         * @return A reference to this action.
         */
        MulticastAction& operator+=(HandlerType handler) {
            if (handler) {
                handlers_.push_back(std::move(handler));
            }
            return *this;
        }

        /**
         * @brief Invokes every subscriber in subscription order. Does nothing when empty.
         * @param args The arguments forwarded to each handler.
         */
        void operator()(Args... args) const {
            const std::vector<HandlerType> snapshot = handlers_;
            for (const auto& handler : snapshot) {
                handler(args...);
            }
        }

        /**
         * @brief Invokes every subscriber; alias of operator().
         * @param args The arguments forwarded to each handler.
         */
        void Invoke(Args... args) const {
            (*this)(args...);
        }

        /**
         * @brief Tests whether there is at least one subscriber (C# `field != null`).
         * @return True if the invocation list is non-empty.
         */
        explicit operator bool() const {
            return !handlers_.empty();
        }

        /**
         * @brief Gets whether the invocation list is empty.
         * @return True if there are no subscribers.
         */
        [[nodiscard]] bool Empty() const {
            return handlers_.empty();
        }

        /** @brief Removes all subscribers. */
        void Clear() {
            handlers_.clear();
        }

    private:
        std::vector<HandlerType> handlers_;
    };

} // namespace System
