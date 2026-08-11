// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>

namespace System {

/**
 * @brief Represents a method that converts an object from one type to another type.
 *
 * C++ counterpart of .NET System.Converter&lt;TInput, TOutput&gt; delegate.
 * Maps to std::function&lt;TOutput(TInput)&gt;.
 *
 * @tparam TInput  The type of object that is to be converted.
 * @tparam TOutput The type the input object is to be converted to.
 *
 * @warning **`TOutput` is unconstrained, so `Converter<T, void>` compiles and
 * is the very same type as `ActionT<T>`**, an alias introducing no new type.
 * .NET cannot express that at all — `void` is not a permitted C# generic
 * argument — so the port silently merges two .NET delegate categories under
 * incompatible public names. That is SR-AUD-126, shared with `Func`; see
 * `System/Func.hpp` for the full note and ticket #2299 for the decision, which
 * is a compile-domain public source break either way. This alias is declared
 * identically in `System/Action.hpp`; both spellings mean the same type.
 */
template<typename TInput, typename TOutput>
using Converter = std::function<TOutput(TInput)>;

} // namespace System
