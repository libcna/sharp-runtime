// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Globalization/CultureInfo.hpp"

namespace System::Globalization {

thread_local std::optional<CultureInfo> CultureInfo::currentCulture_{};
thread_local std::optional<CultureInfo> CultureInfo::currentUICulture_{};
thread_local std::shared_ptr<const CultureInfo> CultureInfo::currentCultureHold_{};
thread_local std::shared_ptr<const CultureInfo> CultureInfo::currentUICultureHold_{};

} // namespace System::Globalization
