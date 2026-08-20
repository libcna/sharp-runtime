// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/DateTime.hpp"
#include "System/TimeSpan.hpp"

namespace System {

    /**
     * @brief The one thing `DateTime` needs from a time zone: a **date-sensitive** UTC offset.
     *
     * Added by #1941 phase 2 under **SA-15.1**, which chose *an abstraction in `Core.Base` that the
     * timezone module implements* over moving `TimeZoneInfo` here.
     *
     * @note **Why an interface rather than naming a zone type.** `DateTime` lives in `Core.Base`
     * and every timezone type lives in `TimeZone`, which declares `PUBLIC_DEPENDENCIES Core.Base` --
     * so naming one from here is a cycle the boundary validator rejects. It is #1940's obstacle
     * exactly, and #1940's answer applies: declare what is needed here, let the other side
     * implement it. Moving `TimeZoneInfo` was measured and rejected as the dearer shape -- it is
     * not header-only, carries two exception types and a 270-line private POSIX header, and would
     * put tzdata reading under **every** consumer of `Core.Base`.
     *
     * @note **This is deliberately two members, not a zone.** It is not `TimeZoneInfo`'s API in
     * miniature and must not grow into one: identifiers, adjustment rules, ambiguity and
     * serialisation all stay on the real types. What `ToLocalTime`/`ToUniversalTime` need is an
     * offset for an instant, and that is all this promises.
     *
     * @note **A DATE-SENSITIVE implementation exists and that is why phase 2 could land.** #1941's
     * record blocked the conversion phase on *"a date-sensitive timezone/DST model"* and looked at
     * `TimeZoneInfo`, whose `GetUtcOffset(DateTime)` **ignores its argument** and whose
     * `IsDaylightSavingTime` is always `false` -- both documented as limitations on that type.
     * `System::TimeZone::CurrentTimeZone()` is the one that is per-date: on POSIX it resolves the
     * offset **and the DST flag** for the instant given. It describes only the process-local zone,
     * which is precisely the zone `ToLocalTime` and `ToUniversalTime` convert against, so the model
     * the record wanted was present all along -- on the other type.
     */
    class ILocalTimeZone {
    public:
        virtual ~ILocalTimeZone() = default;

        /** @return This zone's offset from UTC **at @p time**, DST included where modelled. */
        [[nodiscard]] virtual TimeSpan GetUtcOffset(const DateTime& time) const = 0;

        /** @return true if @p time falls in this zone's daylight saving period. */
        [[nodiscard]] virtual bool IsDaylightSavingTime(const DateTime& time) const = 0;
    };

} // namespace System
