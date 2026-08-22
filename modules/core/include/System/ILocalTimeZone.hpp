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
     * @note **This is deliberately a conversion boundary, not a zone.** It is not
     * `TimeZoneInfo`'s API in miniature and must not grow into one: identifiers, adjustment
     * rules, ambiguity and serialisation all stay on the real types. The two offset queries are
     * distinct because a local wall clock and a UTC instant require different DST resolution at
     * transition boundaries. The UTC query has a compatibility default for fixed test zones.
     *
     * @note **DATE-SENSITIVE implementations exist and that is why phase 2 could land.** #1941's
     * record blocked the conversion phase on *"a date-sensitive timezone/DST model"* and looked at
     * `TimeZoneInfo`, whose `GetUtcOffset(DateTime)` **ignores its argument** and whose
     * `IsDaylightSavingTime` is always `false` -- both documented as limitations on that type.
     * `System::TimeZone::CurrentTimeZone()` is the one that is per-date: on POSIX and Windows it
     * resolves the offset **and the DST flag** for the supplied wall clock / UTC instant. It
     * describes only the process-local zone, which is precisely the zone `ToLocalTime` and
     * `ToUniversalTime` convert against. Emscripten has no timezone database and explicitly uses
     * a distinct zero-offset Local model.
     */
    class ILocalTimeZone {
    public:
        virtual ~ILocalTimeZone() = default;

        /** @return This zone's offset from UTC **at @p time**, DST included where modelled. */
        [[nodiscard]] virtual TimeSpan GetUtcOffset(const DateTime& time) const = 0;

        /**
         * @brief Returns the offset selected by the UTC instant @p time.
         *
         * `DateTime::ToLocalTime` uses this member. A date-sensitive implementation must resolve
         * DST from the UTC instant rather than reinterpret its fields as a local wall clock.
         * Fixed-offset implementations need not override it.
         */
        [[nodiscard]] virtual TimeSpan GetUtcOffsetFromUniversalTime(
            const DateTime& time) const {
            return GetUtcOffset(time);
        }

        /** @return true if @p time falls in this zone's daylight saving period. */
        [[nodiscard]] virtual bool IsDaylightSavingTime(const DateTime& time) const = 0;
    };

} // namespace System
