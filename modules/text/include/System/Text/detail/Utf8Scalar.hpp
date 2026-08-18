// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/detail/Utf8Scalar.hpp"

/**
 * @file
 * @brief Re-exports the one UTF-8 scalar decode, which lives in `Core.Base`.
 *
 * Ticket #2014 factored this rule out of five copies; ticket **#2106** moved the definition from
 * `modules/text` to `modules/core`, and ticket **#2354** collapsed the last three header-inline
 * copies (`UnicodeEncoding.hpp`, `UTF32Encoding.hpp`, `Rune.hpp`) onto it, because `System::BinaryData` needs the same decode and
 * `modules/io` does not depend on `Text`. The alternatives were a sixth copy or a new **public**
 * component edge from `io` to `Text`; moving the single definition to a component everything
 * already depends on costs neither.
 *
 * These aliases exist so that every caller written against `System::Text::detail::` is unchanged.
 * New code may use either spelling; `System::detail::` is the definition's own home.
 */
namespace System::Text::detail {

    using System::detail::DecodeUtf8Scalar;
    using System::detail::TryDecodeUtf8Scalar;
    using System::detail::AppendUtf8Scalar;

} // namespace System::Text::detail
