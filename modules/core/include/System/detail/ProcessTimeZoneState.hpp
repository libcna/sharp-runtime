// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <mutex>

namespace System::detail {

    /**
     * @brief Serializes access to the C library's process-global timezone state.
     *
     * POSIX `TZ`/`tzset()` select one timezone for the whole process. A timezone lookup may
     * temporarily change that state, so every `localtime_r()` reader must share the same lock
     * with every writer. Keeping the function in Core.Base lets DateTime and DateTimeOffset
     * participate without introducing the forbidden Core.Base -> TimeZone dependency.
     *
     * A function-local static avoids cross-translation-unit initialization-order concerns.
     */
    inline std::mutex& processTimeZoneMutex() {
        static std::mutex mutex;
        return mutex;
    }

} // namespace System::detail
