// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <type_traits>
#include <utility>

namespace System {

    namespace detail {
        /**
         * @brief True when @p T has constructors that @p Args should be forwarded to.
         *
         * Ticket #2266 (finding SR-AUD-109). `T{args...}` and `T(args...)` are not
         * interchangeable, and neither one is right for every type:
         *
         * - Braces prefer an `initializer_list` constructor over every other
         *   candidate, so `T{3, 7}` on a `std::vector<int>` builds the two-element
         *   sequence `{3, 7}` rather than the three sevens the caller asked for.
         * - Parentheses cannot express aggregate initialization with brace elision
         *   and cannot reach an `initializer_list` constructor at all, so they
         *   reject `std::array<int, 3>(1, 2, 3)`, nested-aggregate initialization
         *   and every `initializer_list` call outright.
         *
         * An aggregate has no constructors to forward to, so for one the braced
         * form is not merely acceptable but the only spelling that carries the
         * whole language feature — and it is also the spelling that still rejects
         * narrowing conversions. A non-aggregate class type constructible from the
         * arguments does have a constructor, and forwarding to it is exactly what
         * this class documents that it does. Non-class types keep braces so that
         * `CreateInstance<int>(2.5)` stays the compile error it has always been.
         */
        template<typename T, typename... Args>
        inline constexpr bool activatorForwardsToConstructor =
            std::is_class_v<T> && !std::is_aggregate_v<T> && std::is_constructible_v<T, Args...>;
    } // namespace detail

    /**
     * @brief Contains methods to create types of objects locally or obtain references to existing remote objects.
     *
     * C++ partial counterpart of .NET System.Activator.
     * Reflection-based overloads (CreateInstance(Type), CreateInstanceFrom, etc.) are not available
     * because sharp-runtime does not implement System.Reflection.
     * Template overloads cover all constructible types at compile time.
     */
    class Activator {
    public:
        /**
         * @brief Creates an instance of type @p T using its default constructor.
         *
         * C++ counterpart of .NET Activator.CreateInstance&lt;T&gt;().
         * @tparam T Type to instantiate. Must be default-constructible.
         * @return New value-constructed instance of @p T.
         */
        template<typename T>
        [[nodiscard]] static T CreateInstance() {
            return T{};
        }

        /**
         * @brief Creates an instance of type @p T forwarding the given constructor arguments.
         *
         * C++ counterpart of .NET Activator.CreateInstance(Type, object[]).
         * @tparam T    Type to instantiate.
         * @tparam Args Constructor argument types.
         * @param  args Arguments forwarded to @p T's constructor.
         * @return New instance of @p T constructed with @p args.
         *
         * Where @p T has a constructor taking @p Args, the arguments are
         * forwarded to it, so this agrees with CreateInstancePtr() wherever both
         * are well-formed. Where @p T has no such constructor — an aggregate, or
         * a type reached only through an `initializer_list` constructor — the
         * braced form is used, which is the only spelling that can express
         * aggregate initialization with brace elision or select an
         * `initializer_list` constructor. See detail::activatorForwardsToConstructor.
         */
        template<typename T, typename... Args>
        [[nodiscard]] static T CreateInstance(Args&&... args) {
            if constexpr (detail::activatorForwardsToConstructor<T, Args...>) {
                return T(std::forward<Args>(args)...);
            } else {
                return T{std::forward<Args>(args)...};
            }
        }

        /**
         * @brief Creates a heap-allocated instance of type @p T using its default constructor.
         *
         * C++ extension (no direct .NET equivalent).
         * @tparam T Type to instantiate. Must be default-constructible.
         * @return std::unique_ptr owning the new instance.
         */
        template<typename T>
        [[nodiscard]] static std::unique_ptr<T> CreateInstancePtr() {
            return std::make_unique<T>();
        }

        /**
         * @brief Creates a heap-allocated instance of type @p T forwarding the given constructor arguments.
         *
         * C++ extension (no direct .NET equivalent).
         * @tparam T    Type to instantiate.
         * @tparam Args Constructor argument types.
         * @param  args Arguments forwarded to @p T's constructor.
         * @return std::unique_ptr owning the new instance.
         */
        template<typename T, typename... Args>
        [[nodiscard]] static std::unique_ptr<T> CreateInstancePtr(Args&&... args) {
            return std::make_unique<T>(std::forward<Args>(args)...);
        }
    };

} // namespace System
