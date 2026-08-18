// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <functional>
#include <type_traits>

namespace System {

    /**
     * @name Func aliases
     * @{
     *
     * @note **Arity is part of the spelling here.** .NET has one overloaded
     * generic name, `Func<...>`; C++ alias templates cannot overload on
     * parameter count, so the arity appears in the name — `Func<R>`,
     * `FuncT<T, R>`, `FuncT2<T1, T2, R>` … `FuncT16<…>`. Ported code changes
     * spelling with arity, and the result type is always **last**, as in .NET.
     *
     * @par `R` is constrained to a non-void type since ticket #2299
     * `Func<void>` used to compile, and it was **the same type as** `Action` — not convertible
     * to it, the same type, because an alias template introduces no new type. `Converter<T,
     * void>` and `ActionT<T>` coincided the same way. .NET cannot express any of this: `void` is
     * not a permitted C# generic argument, so `Func<void>` does not exist there and the port was
     * merging two delegate categories under incompatible public names.
     *
     * `NonVoidResult` makes the spelling ill-formed, which is as far as an alias can go. **The
     * finding's second prescription is structurally impossible and is not attempted**: with
     * aliases there is only ONE type, so no declaration can accept `Action` and reject
     * `Func<void>`. Preserving the category would mean replacing every alias with a distinct
     * class type — a whole-API break this repository has not asked for.
     *
     * @warning **Historical note.** Before #2299, `R` was unconstrained, so `Func<void>` was the very
     * same type as `Action`** — not merely convertible to it, the same type,
     * because both are aliases of `std::function<void()>`. `Converter<T, void>`
     * and `ActionT<T>` coincide the same way. .NET keeps the two categories
     * apart and can do so structurally: `void` is not a permitted C# generic
     * argument at all, so `Func<void>` does not exist there. **No alias-based
     * design can restore that distinction** — an alias introduces no new type,
     * so nothing declared in terms of these names can accept `Action` while
     * rejecting `Func<void>`, or the reverse. That is SR-AUD-126, and it is
     * **not** repaired by this note. Constraining `R` to non-`void` is
     * available (it is a `static_assert` or a constraint away) but it is a
     * compile-domain public source break — anything downstream spelling
     * `Func<void>` stops compiling — so it is a decision, ticket #2299, not
     * something to assume. Until then: prefer `Action`/`ActionT<…>` when you
     * mean "returns nothing", and do not rely on any API distinguishing the two.
     */

    /**
     * @brief Encapsulates a method that has no parameters and returns a value
     * of type R.
     *
     * C++ counterpart of the .NET System.Func<TResult> delegate.
     *
     * An empty `Func` is callable in the sense that the call compiles; invoking
     * it throws `std::bad_function_call`, which is `std::function`'s own
     * diagnostic and not a `System::Exception`.
     */
    /**
     * @brief The constraint every `Func`/`FuncT*` result type must satisfy (ticket #2299).
     *
     * `void` is not a permitted generic argument in C#, so .NET has no `Func<void>`. Spelling one
     * here produced `Action` — the same type, not merely a convertible one — which merged two
     * delegate categories under incompatible public names.
     */
    template<typename R>
    concept NonVoidResult = !std::is_void_v<R>;

    template<NonVoidResult R>
    using Func = std::function<R()>;

    /**
     * @brief Encapsulates a method that has one parameter and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T, TResult> delegate.
     */
    template<typename T, NonVoidResult R>
    using FuncT = std::function<R(T)>;

    /**
     * @brief Encapsulates a method that has two parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1, T2, TResult> delegate.
     */
    template<typename T1, typename T2, NonVoidResult R>
    using FuncT2 = std::function<R(T1, T2)>;

    /**
     * @brief Encapsulates a method that has three parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1, T2, T3, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, NonVoidResult R>
    using FuncT3 = std::function<R(T1, T2, T3)>;

    /**
     * @brief Encapsulates a method that has four parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1, T2, T3, T4, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, typename T4, NonVoidResult R>
    using FuncT4 = std::function<R(T1, T2, T3, T4)>;

    /**
     * @brief Encapsulates a method that has five parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1, T2, T3, T4, T5, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, typename T4, typename T5, NonVoidResult R>
    using FuncT5 = std::function<R(T1, T2, T3, T4, T5)>;

    /**
     * @brief Encapsulates a method that has six parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1..T6, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
             NonVoidResult R>
    using FuncT6 = std::function<R(T1, T2, T3, T4, T5, T6)>;

    /**
     * @brief Encapsulates a method that has seven parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1..T7, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
             typename T7, NonVoidResult R>
    using FuncT7 = std::function<R(T1, T2, T3, T4, T5, T6, T7)>;

    /**
     * @brief Encapsulates a method that has eight parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1..T8, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
             typename T7, typename T8, NonVoidResult R>
    using FuncT8 = std::function<R(T1, T2, T3, T4, T5, T6, T7, T8)>;

    /**
     * @brief Encapsulates a method that has nine parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1..T9, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
             typename T7, typename T8, typename T9, NonVoidResult R>
    using FuncT9 = std::function<R(T1, T2, T3, T4, T5, T6, T7, T8, T9)>;

    /**
     * @brief Encapsulates a method that has ten parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1..T10, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
             typename T7, typename T8, typename T9, typename T10, NonVoidResult R>
    using FuncT10 = std::function<R(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10)>;

    /**
     * @brief Encapsulates a method that has eleven parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1..T11, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
             typename T7, typename T8, typename T9, typename T10, typename T11, NonVoidResult R>
    using FuncT11 = std::function<R(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11)>;

    /**
     * @brief Encapsulates a method that has twelve parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1..T12, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
             typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
             NonVoidResult R>
    using FuncT12 = std::function<R(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12)>;

    /**
     * @brief Encapsulates a method that has thirteen parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1..T13, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
             typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
             typename T13, NonVoidResult R>
    using FuncT13 = std::function<R(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13)>;

    /**
     * @brief Encapsulates a method that has fourteen parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1..T14, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
             typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
             typename T13, typename T14, NonVoidResult R>
    using FuncT14 = std::function<R(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13,
                                     T14)>;

    /**
     * @brief Encapsulates a method that has fifteen parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1..T15, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
             typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
             typename T13, typename T14, typename T15, NonVoidResult R>
    using FuncT15 = std::function<R(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13,
                                     T14, T15)>;

    /**
     * @brief Encapsulates a method that has sixteen parameters and returns a value.
     *
     * C++ counterpart of the .NET System.Func<T1..T16, TResult> delegate.
     */
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
             typename T7, typename T8, typename T9, typename T10, typename T11, typename T12,
             typename T13, typename T14, typename T15, typename T16, NonVoidResult R>
    using FuncT16 = std::function<R(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13,
                                     T14, T15, T16)>;

    /** @} */

} // namespace System
