// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/DateTime.hpp"
#include "System/DateTimeOffset.hpp"
#include "System/DayOfWeek.hpp"
#include "System/TimeSpan.hpp"

namespace System {

    /**
     * @brief Represents a time zone — a named region with a UTC offset.
     *
     * C++ counterpart of .NET System.TimeZoneInfo.
     *
     * Implements the subset of the .NET API needed for game-engine porting.
     * @c Local() reads the real system timezone via POSIX @c localtime_r().
     * @c FindSystemTimeZoneById() resolves IANA names from @c /usr/share/zoneinfo/.
     *
     * **Limitations (documented, not bugs):**
     * - DST transitions are not modelled *by this type*: GetUtcOffset() always returns the
     *   zone's standard offset, and IsDaylightSavingTime(), IsAmbiguousTime() and
     *   IsInvalidTime() always return false. The legacy @c System::TimeZone adapter returned
     *   by @c TimeZone::CurrentTimeZone() *is* date-sensitive -- its contract is per-date and
     *   it only ever describes the process-local zone -- so the two surfaces deliberately
     *   differ. See docs/SystemTimeZoneNamespaceReviewPlan.md section 12.
     * - GetAdjustmentRules() returns an empty array, and HasSameRules() therefore cannot
     *   distinguish two zones that share a base offset and a DST flag (SR-AUD-228, ticket
     *   #2185: the repair needs stored rules, an object-layout change).
     * - Serialisation (ToSerializedString / FromSerializedString) is not implemented.
     * - DisplayName is the raw identifier for a system zone; producing .NET's
     *   "(UTC+01:00) ..." text needs CLDR display data this repository does not carry.
     * - GetSystemTimeZones() returns UTC and Local rather than enumerating the database.
     * - POSIX-only: Local() and FindSystemTimeZoneById() use localtime_r and /usr/share/zoneinfo.
     */
    class TimeZoneInfo {
    public:
        // =====================================================================
        // Nested types
        // =====================================================================

        /**
         * @brief Represents the date and time when a time zone changes from standard
         * time to daylight saving time, or vice versa.
         *
         * C++ counterpart of .NET System.TimeZoneInfo.TransitionTime.
         * In this implementation all instances are stubs; DST transitions are not modelled.
         */
        struct TransitionTime {
            DateTime   timeOfDay_;
            intcs      month_          = 0;
            intcs      week_           = 0;
            intcs      day_            = 0;
            DayOfWeek  dayOfWeek_      = DayOfWeek::Sunday;
            bool       isFixedDateRule_ = false;

            /** @brief Gets the time of day at which the transition occurs. */
            [[nodiscard]] DateTime   getTimeOfDayProperty()     const { return timeOfDay_; }
            /** @brief Gets the month in which the transition occurs (1-12). */
            [[nodiscard]] intcs      getMonthProperty()         const { return month_; }
            /** @brief Gets the week of the month (1-5) in which the transition occurs. */
            [[nodiscard]] intcs      getWeekProperty()          const { return week_; }
            /** @brief Gets the day on which the transition occurs for a fixed-date rule. */
            [[nodiscard]] intcs      getDayProperty()           const { return day_; }
            /** @brief Gets the day of the week on which the transition occurs for a floating rule. */
            [[nodiscard]] DayOfWeek  getDayOfWeekProperty()     const { return dayOfWeek_; }
            /** @brief Gets a value indicating whether the transition is fixed-date or floating. */
            [[nodiscard]] bool       getIsFixedDateRuleProperty() const { return isFixedDateRule_; }

            /**
             * @brief Validates that @p timeOfDay represents only a time-of-day (no date
             * component beyond DateTime.MinValue's implicit date) at millisecond granularity.
             *
             * C++ counterpart of the timeOfDay portion of .NET
             * TimeZoneInfo.TransitionTime.ValidateTransitionTime -- real .NET also rejects a
             * timeOfDay.Kind other than Unspecified, which does not apply here since this
             * port's DateTime does not track DateTimeKind (see DateTime.hpp's documented
             * Kind limitation).
             * @throws ArgumentException if @p timeOfDay has a date component or sub-millisecond ticks.
             */
            static void validateTimeOfDay(const DateTime& timeOfDay) {
                longcs ticks = timeOfDay.getTicksProperty();
                if (ticks >= TimeSpan::TicksPerDay || ticks % TimeSpan::TicksPerMillisecond != 0)
                    throw System::ArgumentException(
                        "The supplied DateTime must have the Year, Month, and Day properties set to 1, "
                        "and the Millisecond, and Ticks properties set to 0.", "timeOfDay");
            }

            /**
             * @brief Creates a fixed-date transition rule.
             *
             * C++ counterpart of .NET TransitionTime.CreateFixedDateRule(DateTime, int, int).
             * @param timeOfDay Time of day when the transition occurs (must have no date component).
             * @param month     Month of the transition (1-12).
             * @param day       Day of the month (1-31).
             * @throws ArgumentOutOfRangeException if month or day is out of range.
             * @throws ArgumentException if timeOfDay has a date component or sub-millisecond ticks.
             */
            static TransitionTime CreateFixedDateRule(DateTime timeOfDay, intcs month, intcs day) {
                validateTimeOfDay(timeOfDay);
                if (month < 1 || month > 12)
                    throw ArgumentOutOfRangeException("month: Month must be between 1 and 12.");
                if (day < 1 || day > 31)
                    throw ArgumentOutOfRangeException("day: Day must be between 1 and 31.");
                TransitionTime t;
                t.timeOfDay_       = timeOfDay;
                t.month_           = month;
                t.day_             = day;
                t.week_            = 1;
                t.isFixedDateRule_ = true;
                return t;
            }

            /**
             * @brief Creates a floating-date transition rule.
             *
             * C++ counterpart of .NET TransitionTime.CreateFloatingDateRule(DateTime, int, int, DayOfWeek).
             * @param timeOfDay  Time of day when the transition occurs.
             * @param month      Month of the transition (1-12).
             * @param week       Week of the month (1-5).
             * @param dayOfWeek  Day of the week (Sunday=0 … Saturday=6).
             * @throws ArgumentOutOfRangeException if month, week, or dayOfWeek is out of range.
             * @throws ArgumentException if timeOfDay has a date component or sub-millisecond ticks.
             */
            static TransitionTime CreateFloatingDateRule(DateTime timeOfDay, intcs month,
                                                         intcs week, DayOfWeek dayOfWeek) {
                validateTimeOfDay(timeOfDay);
                if (month < 1 || month > 12)
                    throw ArgumentOutOfRangeException("month: Month must be between 1 and 12.");
                if (week < 1 || week > 5)
                    throw ArgumentOutOfRangeException("week: Week must be between 1 and 5.");
                if (static_cast<int>(dayOfWeek) < 0 || static_cast<int>(dayOfWeek) > 6)
                    throw ArgumentOutOfRangeException("dayOfWeek: DayOfWeek must be between 0 and 6.");
                TransitionTime t;
                t.timeOfDay_       = timeOfDay;
                t.month_           = month;
                t.week_            = week;
                t.day_             = 1;
                t.dayOfWeek_       = dayOfWeek;
                t.isFixedDateRule_ = false;
                return t;
            }

            /**
             * @brief Returns true if this TransitionTime is equal to @p other.
             *
             * C++ counterpart of .NET TransitionTime.Equals(TransitionTime).
             * For fixed-date rules only Day is compared; for floating rules Week and DayOfWeek
             * are compared instead, matching .NET's exact semantics.
             */
            [[nodiscard]] bool Equals(const TransitionTime& other) const {
                if (isFixedDateRule_ != other.isFixedDateRule_) return false;
                if (!(timeOfDay_ == other.timeOfDay_))          return false;
                if (month_ != other.month_)                     return false;
                return isFixedDateRule_
                    ? (day_ == other.day_)
                    : (week_ == other.week_ && dayOfWeek_ == other.dayOfWeek_);
            }

            /**
             * @brief Returns a hash code for this TransitionTime.
             *
             * C++ counterpart of .NET TransitionTime.GetHashCode().
             */
            [[nodiscard]] intcs GetHashCode() const noexcept {
                return month_ ^ (week_ << 8);
            }

            /**
             * @brief Returns true if both TransitionTime instances are equal.
             *
             * C++ counterpart of .NET TransitionTime operator==.
             */
            bool operator==(const TransitionTime& o) const { return Equals(o); }

            /**
             * @brief Returns true if both TransitionTime instances differ.
             *
             * C++ counterpart of .NET TransitionTime operator!=.
             */
            bool operator!=(const TransitionTime& o) const { return !Equals(o); }
        };

        /**
         * @brief Provides information about a time zone adjustment (DST rule).
         *
         * C++ counterpart of .NET System.TimeZoneInfo.AdjustmentRule.
         * In this implementation GetAdjustmentRules() always returns an empty vector;
         * these objects are exposed only for API completeness.
         */
        class AdjustmentRule {
            DateTime       dateStart_;
            DateTime       dateEnd_;
            TimeSpan       daylightDelta_;
            TransitionTime daylightTransitionStart_;
            TransitionTime daylightTransitionEnd_;
            TimeSpan       baseUtcOffsetDelta_;
            bool           noDaylightTransitions_ = false;

            AdjustmentRule() = default;
        public:
            /** @brief Gets the date when the adjustment rule begins. */
            [[nodiscard]] DateTime       getDateStartProperty()               const { return dateStart_; }
            /** @brief Gets the date when the adjustment rule ends. */
            [[nodiscard]] DateTime       getDateEndProperty()                 const { return dateEnd_; }
            /** @brief Gets the time difference between standard time and DST. */
            [[nodiscard]] TimeSpan       getDaylightDeltaProperty()           const { return daylightDelta_; }
            /** @brief Gets the start transition for DST. */
            [[nodiscard]] TransitionTime getDaylightTransitionStartProperty() const { return daylightTransitionStart_; }
            /** @brief Gets the end transition for DST. */
            [[nodiscard]] TransitionTime getDaylightTransitionEndProperty()   const { return daylightTransitionEnd_; }

            /**
             * @brief Gets the time difference with the base UTC offset for the time zone
             * during the adjustment-rule period.
             *
             * C++ counterpart of .NET AdjustmentRule.BaseUtcOffsetDelta.
             */
            [[nodiscard]] TimeSpan       getBaseUtcOffsetDeltaProperty()     const { return baseUtcOffsetDelta_; }

            /**
             * @brief Gets a value indicating that this rule fixes the time zone offset
             * from DateStart to DateEnd without any daylight transitions in between.
             *
             * C++ counterpart of .NET AdjustmentRule.NoDaylightTransitions (internal).
             */
            [[nodiscard]] bool           getNoDaylightTransitionsProperty()  const { return noDaylightTransitions_; }

            /**
             * @brief Gets a value indicating whether this rule involves a DST change.
             *
             * C++ counterpart of .NET AdjustmentRule.HasDaylightSaving (internal).
             * Returns true when DaylightDelta is non-zero or either transition time is
             * non-default.
             */
            [[nodiscard]] bool getHasDaylightSavingProperty() const {
                static const TransitionTime kDefault{};
                return daylightDelta_ != TimeSpan::Zero ||
                       !(daylightTransitionStart_ == kDefault) ||
                       !(daylightTransitionEnd_   == kDefault);
            }

            /**
             * @brief Returns a hash code for this AdjustmentRule.
             *
             * C++ counterpart of .NET AdjustmentRule.GetHashCode().
             */
            [[nodiscard]] intcs GetHashCode() const noexcept {
                auto ticks = dateStart_.getTicksProperty();
                return static_cast<intcs>(ticks ^ (ticks >> 32));
            }

            /**
             * @brief Returns true if this rule is equal to @p other.
             *
             * C++ counterpart of .NET AdjustmentRule.Equals(AdjustmentRule).
             * Two rules are equal when all fields (including BaseUtcOffsetDelta) match.
             */
            [[nodiscard]] bool Equals(const AdjustmentRule& other) const {
                return dateStart_            == other.dateStart_            &&
                       dateEnd_              == other.dateEnd_              &&
                       daylightDelta_        == other.daylightDelta_        &&
                       baseUtcOffsetDelta_   == other.baseUtcOffsetDelta_   &&
                       daylightTransitionStart_ == other.daylightTransitionStart_ &&
                       daylightTransitionEnd_   == other.daylightTransitionEnd_;
            }

            bool operator==(const AdjustmentRule& o) const { return Equals(o); }
            bool operator!=(const AdjustmentRule& o) const { return !Equals(o); }

            /**
             * @brief Validates the effective date range of a candidate adjustment rule.
             *
             * C++ counterpart of the date-range half of .NET
             * AdjustmentRule.CreateAdjustmentRule's argument validation: a rule whose
             * DateEnd precedes its DateStart describes an empty period and cannot be
             * applied to any instant, so .NET rejects it rather than storing it.
             * @param dateStart The first date the rule applies to.
             * @param dateEnd   The last date the rule applies to; must not precede @p dateStart.
             * @throws ArgumentException if @p dateEnd is earlier than @p dateStart.
             */
            static void validateDateRange(const DateTime& dateStart, const DateTime& dateEnd) {
                if (dateEnd.getTicksProperty() < dateStart.getTicksProperty())
                    throw System::ArgumentException(
                        "The DateStart property must come before the DateEnd property.",
                        "dateStart");
            }

            /**
             * @brief The three further validations .NET's ValidateAdjustmentRule performs.
             *
             * Ticket #2186 (2026-08-18). #2179 measured all three as accepted here and
             * deliberately did not repair them, because "the audit's managed probe covers only
             * the reversed date range, and inventing three more rejections on a recollection of
             * the .NET source is exactly what this review declines to do". The reference is
             * available now (`TimeZoneInfo.AdjustmentRule.cs:174-223`), and it **corrects the
             * ticket's own statement of two of the three**:
             *
             *   - the `daylightDelta` range is NOT +/-14 hours. It is `-23.0 .. 14.0`, and .NET
             *     explains why in a comment of its own: Samoa moved across the International Date
             *     Line, so describing its daylight delta needs -23. The MESSAGE still says "plus
             *     or minus 14.0 hours", because it is shared with `UtcOffsetOutOfRange`. That
             *     inconsistency is .NET's and is transcribed rather than tidied;
             *   - the seconds check is not "sub-minute". It is "not a whole number of minutes",
             *     so 1h30m30s fails as surely as 30s does;
             *   - the time-of-day check EXEMPTS `DateTime::MinValue` for `dateStart` and
             *     `MaxValue` for `dateEnd`, which is how a rule that spans all time is spelled.
             */
            static void validateAdjustmentRule(const DateTime& dateStart, const DateTime& dateEnd,
                                               const TimeSpan& daylightDelta) {
                validateDateRange(dateStart, dateEnd);

                // TimeZoneInfo.AdjustmentRule.cs:206-209.
                constexpr double kMinDaylightDeltaHours = -23.0;
                constexpr double kMaxDaylightDeltaHours = 14.0;
                if (daylightDelta.getTotalHoursProperty() < kMinDaylightDeltaHours ||
                    daylightDelta.getTotalHoursProperty() > kMaxDaylightDeltaHours) {
                    throw System::ArgumentOutOfRangeException(
                        "daylightDelta", daylightDelta.ToString(),
                        "The TimeSpan parameter must be within plus or minus 14.0 hours.");
                }

                // :211-214.
                if (daylightDelta.getTicksProperty() % TimeSpan::TicksPerMinute != 0) {
                    throw System::ArgumentException(
                        "The TimeSpan parameter cannot be specified more precisely than whole "
                        "minutes.",
                        "daylightDelta");
                }

                // :216-223. This port has no DateTimeKind (a permanent deviation), so the
                // `Kind == Unspecified` conjunct is not reproducible and is simply absent -- which
                // makes this port's check STRICTER than .NET's for a UTC-kinded argument, and
                // identical for every argument this port can express.
                if (dateStart != DateTime::MinValue &&
                    dateStart.getTimeOfDayProperty() != TimeSpan::Zero) {
                    throw System::ArgumentException(
                        "The supplied DateTime includes a TimeOfDay setting.   This is not "
                        "supported.",
                        "dateStart");
                }
                if (dateEnd != DateTime::MaxValue &&
                    dateEnd.getTimeOfDayProperty() != TimeSpan::Zero) {
                    throw System::ArgumentException(
                        "The supplied DateTime includes a TimeOfDay setting.   This is not "
                        "supported.",
                        "dateEnd");
                }
            }

            /**
             * @brief Creates an adjustment rule with zero BaseUtcOffsetDelta.
             *
             * C++ counterpart of .NET AdjustmentRule.CreateAdjustmentRule(DateTime, DateTime,
             * TimeSpan, TransitionTime, TransitionTime).
             * @throws ArgumentException if @p dateEnd is earlier than @p dateStart.
             */
            static std::shared_ptr<AdjustmentRule> CreateAdjustmentRule(
                DateTime dateStart, DateTime dateEnd, TimeSpan daylightDelta,
                TransitionTime daylightTransitionStart, TransitionTime daylightTransitionEnd)
            {
                validateAdjustmentRule(dateStart, dateEnd, daylightDelta);
                auto r = std::shared_ptr<AdjustmentRule>(new AdjustmentRule());
                r->dateStart_               = dateStart;
                r->dateEnd_                 = dateEnd;
                r->daylightDelta_           = daylightDelta;
                r->daylightTransitionStart_ = daylightTransitionStart;
                r->daylightTransitionEnd_   = daylightTransitionEnd;
                r->baseUtcOffsetDelta_      = TimeSpan::Zero;
                r->noDaylightTransitions_   = false;
                return r;
            }

            /**
             * @brief Creates an adjustment rule with an explicit BaseUtcOffsetDelta.
             *
             * C++ counterpart of .NET AdjustmentRule.CreateAdjustmentRule(DateTime, DateTime,
             * TimeSpan, TransitionTime, TransitionTime, TimeSpan).
             * @param baseUtcOffsetDelta The delta from the zone's default UTC offset that
             *                           applies during this rule's period.
             * @throws ArgumentException if @p dateEnd is earlier than @p dateStart.
             */
            static std::shared_ptr<AdjustmentRule> CreateAdjustmentRule(
                DateTime dateStart, DateTime dateEnd, TimeSpan daylightDelta,
                TransitionTime daylightTransitionStart, TransitionTime daylightTransitionEnd,
                TimeSpan baseUtcOffsetDelta)
            {
                validateAdjustmentRule(dateStart, dateEnd, daylightDelta);
                auto r = std::shared_ptr<AdjustmentRule>(new AdjustmentRule());
                r->dateStart_               = dateStart;
                r->dateEnd_                 = dateEnd;
                r->daylightDelta_           = daylightDelta;
                r->daylightTransitionStart_ = daylightTransitionStart;
                r->daylightTransitionEnd_   = daylightTransitionEnd;
                r->baseUtcOffsetDelta_      = baseUtcOffsetDelta;
                r->noDaylightTransitions_   = false;
                return r;
            }
        };

    private:
        std::string id_;
        std::string displayName_;
        std::string standardName_;
        std::string daylightName_;
        TimeSpan    baseUtcOffset_;
        bool        supportsDst_ = false;

        TimeZoneInfo(std::string id, TimeSpan baseUtcOffset,
                     std::string displayName, std::string standardName,
                     std::string daylightName, bool supportsDst)
            : id_(std::move(id)), displayName_(std::move(displayName)),
              standardName_(std::move(standardName)), daylightName_(std::move(daylightName)),
              baseUtcOffset_(baseUtcOffset), supportsDst_(supportsDst) {}

    public:
        // =====================================================================
        // Properties
        // =====================================================================

        /**
         * @brief Gets the IANA or well-known identifier of this time zone.
         *
         * C++ counterpart of .NET TimeZoneInfo.Id.
         */
        [[nodiscard]] const std::string& getIdProperty()           const { return id_; }

        /**
         * @brief Gets a human-readable display name for this time zone.
         *
         * C++ counterpart of .NET TimeZoneInfo.DisplayName.
         */
        [[nodiscard]] const std::string& getDisplayNameProperty()  const { return displayName_; }

        /**
         * @brief Gets the standard (non-DST) name.
         *
         * C++ counterpart of .NET TimeZoneInfo.StandardName. For a system zone this is the
         * abbreviation the zone uses during standard time (@c "EST" for America/New_York),
         * derived from the same year scan as BaseUtcOffset and likewise independent of the
         * current date.
         */
        [[nodiscard]] const std::string& getStandardNameProperty() const { return standardName_; }

        /**
         * @brief Gets the daylight-saving name.
         *
         * C++ counterpart of .NET TimeZoneInfo.DaylightName. For a system zone this is the
         * abbreviation the zone uses during daylight time (@c "EDT" for America/New_York).
         * Equals the standard name exactly when the zone does not observe daylight time.
         */
        [[nodiscard]] const std::string& getDaylightNameProperty() const { return daylightName_; }

        /**
         * @brief Gets the fixed UTC offset for this zone.
         *
         * C++ counterpart of .NET TimeZoneInfo.BaseUtcOffset.
         * DST transitions are not modelled; this is always the zone's **standard** offset --
         * the invariant one, not whichever offset happens to be in force today. For a system
         * zone it is derived by scanning a whole year of the tz database rather than by
         * reading the current instant, so the value does not depend on the month in which the
         * object was created (ticket #2181, SR-AUD-229).
         */
        [[nodiscard]] TimeSpan getBaseUtcOffsetProperty()          const { return baseUtcOffset_; }

        /**
         * @brief Gets a value indicating whether this zone ever observes DST.
         *
         * C++ counterpart of .NET TimeZoneInfo.SupportsDaylightSavingTime.
         */
        [[nodiscard]] bool getSupportsDaylightSavingTimeProperty() const { return supportsDst_; }

        /**
         * @brief Gets a value indicating whether the time zone ID has the IANA format.
         *
         * C++ counterpart of .NET TimeZoneInfo.HasIanaId.
         * Returns true when the ID contains a '/' (e.g. "Europe/Prague").
         */
        [[nodiscard]] bool getHasIanaIdProperty() const {
            return id_.find('/') != std::string::npos;
        }

        // =====================================================================
        // Instance methods
        // =====================================================================

        /**
         * @brief Returns false — DST transitions are not modelled.
         *
         * C++ counterpart of .NET TimeZoneInfo.IsDaylightSavingTime(DateTime).
         */
        [[nodiscard]] bool IsDaylightSavingTime(const DateTime& /*dt*/) const { return false; }

        /**
         * @brief Returns false — DST transitions are not modelled.
         *
         * C++ counterpart of .NET TimeZoneInfo.IsDaylightSavingTime(DateTimeOffset).
         */
        [[nodiscard]] bool IsDaylightSavingTime(const DateTimeOffset& /*dt*/) const { return false; }

        /**
         * @brief Returns the fixed UTC offset for any DateTime.
         *
         * C++ counterpart of .NET TimeZoneInfo.GetUtcOffset(DateTime).
         */
        [[nodiscard]] TimeSpan GetUtcOffset(const DateTime& /*dt*/)       const { return baseUtcOffset_; }

        /**
         * @brief Returns the fixed UTC offset for any DateTimeOffset.
         *
         * C++ counterpart of .NET TimeZoneInfo.GetUtcOffset(DateTimeOffset).
         */
        [[nodiscard]] TimeSpan GetUtcOffset(const DateTimeOffset& /*dt*/) const { return baseUtcOffset_; }

        /**
         * @brief Returns false — ambiguous times are not modelled.
         *
         * C++ counterpart of .NET TimeZoneInfo.IsAmbiguousTime(DateTime).
         */
        [[nodiscard]] bool IsAmbiguousTime(const DateTime& /*dt*/)        const { return false; }

        /**
         * @brief Returns false — ambiguous times are not modelled.
         *
         * C++ counterpart of .NET TimeZoneInfo.IsAmbiguousTime(DateTimeOffset).
         */
        [[nodiscard]] bool IsAmbiguousTime(const DateTimeOffset& /*dt*/)  const { return false; }

        /**
         * @brief Returns false — invalid times are not modelled.
         *
         * C++ counterpart of .NET TimeZoneInfo.IsInvalidTime(DateTime).
         */
        [[nodiscard]] bool IsInvalidTime(const DateTime& /*dt*/)          const { return false; }

        /**
         * @brief Returns an empty array — DST transitions are not modelled.
         *
         * C++ counterpart of .NET TimeZoneInfo.GetAmbiguousTimeOffsets(DateTime).
         */
        [[nodiscard]] std::vector<TimeSpan> GetAmbiguousTimeOffsets(const DateTime& /*dt*/) const {
            return {};
        }

        /**
         * @brief Returns an empty array — DST transitions are not modelled.
         *
         * C++ counterpart of .NET TimeZoneInfo.GetAmbiguousTimeOffsets(DateTimeOffset).
         */
        [[nodiscard]] std::vector<TimeSpan> GetAmbiguousTimeOffsets(const DateTimeOffset& /*dt*/) const {
            return {};
        }

        /**
         * @brief Returns an empty array — DST adjustment rules are not modelled.
         *
         * C++ counterpart of .NET TimeZoneInfo.GetAdjustmentRules().
         */
        [[nodiscard]] std::vector<std::shared_ptr<AdjustmentRule>> GetAdjustmentRules() const {
            return {};
        }

        /**
         * @brief Converts a DateTime to UTC by subtracting the zone's base UTC offset.
         *
         * C++ counterpart of the instance form of .NET TimeZoneInfo.ConvertTimeToUtc(DateTime).
         */
        [[nodiscard]] DateTime ConvertTimeToUtc(const DateTime& dt) const {
            return dt.Add(-baseUtcOffset_);
        }

        /**
         * @brief Returns true if this zone has the same base UTC offset and DST support as @p other.
         *
         * C++ counterpart of .NET TimeZoneInfo.HasSameRules(TimeZoneInfo), which additionally
         * compares the two zones' adjustment-rule arrays.
         *
         * **Known divergence (SR-AUD-228, ticket #2185).** This type stores no adjustment
         * rules, so it cannot return false where .NET does: America/New_York and
         * America/Havana share a standard offset and a DST flag but not their rules, and this
         * method reports them as same-rule zones. Repairing it means storing the rules, which
         * grows the object, so it is recorded and gated rather than guessed at.
         */
        [[nodiscard]] bool HasSameRules(const TimeZoneInfo& other) const {
            return baseUtcOffset_ == other.baseUtcOffset_ &&
                   supportsDst_   == other.supportsDst_;
        }

        /**
         * @brief Folds a zone identifier to its ordinal (locale-independent) lower case.
         *
         * Both Equals() and GetHashCode() answer the same question -- "are these two
         * identifiers the same zone id?" -- so both are computed from this one function.
         * Only the 26 ASCII letters are folded: unlike @c std::tolower this does not consult
         * the process @c LC_CTYPE, so two callers on different locales cannot disagree about
         * whether two ids are equal.
         *
         * @param id The identifier to fold.
         * @return @p id with A-Z mapped to a-z and every other byte left alone.
         */
        [[nodiscard]] static std::string foldZoneId(const std::string& id) {
            std::string folded = id;
            for (auto& c : folded)
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            return folded;
        }

        /**
         * @brief Returns true if this zone has the same ID as @p other, ignoring ASCII case.
         *
         * C++ counterpart of .NET TimeZoneInfo.Equals(TimeZoneInfo), which compares ids with
         * @c StringComparer.OrdinalIgnoreCase. Comparing case-sensitively here while
         * GetHashCode() folded case was also an equality/hash contract breach in its own
         * right: two zones could compare unequal and still hash equal.
         */
        [[nodiscard]] bool Equals(const TimeZoneInfo& other) const {
            return foldZoneId(id_) == foldZoneId(other.id_);
        }

        /**
         * @brief Returns a hash code based on the zone ID (case-insensitive).
         *
         * C++ counterpart of .NET TimeZoneInfo.GetHashCode(). Computed from the same fold
         * Equals() uses, so equal zones always hash equally.
         */
        [[nodiscard]] intcs GetHashCode() const {
            return static_cast<intcs>(std::hash<std::string>{}(foldZoneId(id_)));
        }

        /**
         * @brief Returns the display name of this time zone.
         *
         * C++ counterpart of .NET TimeZoneInfo.ToString().
         */
        [[nodiscard]] std::string ToString() const { return displayName_; }

        bool operator==(const TimeZoneInfo& other) const { return Equals(other); }
        bool operator!=(const TimeZoneInfo& other) const { return !Equals(other); }

        // =====================================================================
        // Static properties
        // =====================================================================

        /**
         * @brief Returns the UTC singleton (offset zero, no DST).
         *
         * C++ counterpart of .NET TimeZoneInfo.Utc.
         */
        static const TimeZoneInfo& Utc() {
            static TimeZoneInfo tz("UTC", TimeSpan::Zero,
                                   "Coordinated Universal Time",
                                   "Coordinated Universal Time",
                                   "Coordinated Universal Time", false);
            return tz;
        }

        /**
         * @brief Returns the local system time zone by reading the OS timezone via POSIX localtime_r().
         *
         * C++ counterpart of .NET TimeZoneInfo.Local.
         * The offset reflects the current wall-clock offset (including any active DST).
         * On Emscripten, returns UTC.
         */
        static const TimeZoneInfo& Local();

        // =====================================================================
        // Static methods
        // =====================================================================

        /**
         * @brief Clears the cached local time zone data.
         *
         * C++ counterpart of .NET TimeZoneInfo.ClearCachedData().
         * This implementation is a no-op because Local() uses a block-scope static.
         */
        static void ClearCachedData() { /* local static cannot be reset */ }

        /**
         * @brief Looks up a time zone by IANA ID (e.g. "Europe/Prague").
         *
         * C++ counterpart of .NET TimeZoneInfo.FindSystemTimeZoneById(string).
         * On Linux: requires @p id to be a relative, NUL-free path of non-empty, non-dot
         * segments naming a regular file under /usr/share/zoneinfo/ whose first four bytes are
         * the TZif magic, then derives the zone's metadata via setenv("TZ").
         * On Windows: uses the IANA→Windows CLDR mapping table.
         *
         * The identifier and magic checks were added by ticket #2183: /usr/share/zoneinfo also
         * ships plain-text data files (zone.tab, tzdata.zi, leapseconds and others), and
         * without the magic check every one of them resolved to a zone with offset zero.
         * @throws System::TimeZoneNotFoundException if the ID is malformed or not found.
         */
        static std::shared_ptr<TimeZoneInfo> FindSystemTimeZoneById(const std::string& id);

        /**
         * @brief Tries to find a time zone by ID; returns false instead of throwing.
         *
         * C++ counterpart of .NET TimeZoneInfo.TryFindSystemTimeZoneById(string, out TimeZoneInfo).
         *
         * On failure @p result is set to @c nullptr, mirroring .NET's assignment of @c null to the
         * @c out parameter. A caller that reuses one variable across several lookups therefore
         * cannot be handed the previous zone by a lookup that failed.
         *
         * @param id     The time zone identifier to look up.
         * @param result Receives the zone on success and @c nullptr on failure.
         * @return true if the zone was found.
         */
        static bool TryFindSystemTimeZoneById(const std::string& id,
                                              std::shared_ptr<TimeZoneInfo>& result) {
            try {
                result = FindSystemTimeZoneById(id);
                return true;
            } catch (...) {
                result.reset();
                return false;
            }
        }

        /**
         * @brief Returns all known system time zones (UTC + Local in this implementation).
         *
         * C++ counterpart of .NET TimeZoneInfo.GetSystemTimeZones().
         */
        static std::vector<std::shared_ptr<TimeZoneInfo>> GetSystemTimeZones();

        /**
         * @brief Returns all known system time zones; skipSorting is ignored.
         *
         * C++ counterpart of .NET TimeZoneInfo.GetSystemTimeZones(bool skipSorting).
         */
        static std::vector<std::shared_ptr<TimeZoneInfo>> GetSystemTimeZones(bool /*skipSorting*/) {
            return GetSystemTimeZones();
        }

        /**
         * @brief The largest UTC offset a time zone may declare, in ticks (+14 hours).
         *
         * C++ counterpart of the bound .NET's TimeZoneInfo.UtcOffsetOutOfRange enforces.
         */
        static constexpr longcs MaxUtcOffsetTicks = 14LL * 60LL * 60LL * 10000000LL;

        /**
         * @brief Validates a candidate base UTC offset the way .NET's ValidateTimeZoneInfo does.
         *
         * @param utcOffset The offset to check.
         * @throws ArgumentOutOfRangeException if @p utcOffset is outside ±14 hours.
         * @throws ArgumentException if @p utcOffset is not a whole number of minutes.
         */
        static void validateUtcOffset(const TimeSpan& utcOffset) {
            longcs ticks = utcOffset.getTicksProperty();
            if (ticks > MaxUtcOffsetTicks || ticks < -MaxUtcOffsetTicks)
                throw ArgumentOutOfRangeException(
                    "baseUtcOffset: The TimeSpan parameter must be within plus or minus 14.0 hours.");
            if (ticks % TimeSpan::TicksPerMinute != 0)
                throw ArgumentException(
                    "The TimeSpan parameter cannot be specified more precisely than whole minutes.",
                    "baseUtcOffset");
        }

        /**
         * @brief Creates a fixed-offset custom time zone.
         *
         * C++ counterpart of .NET TimeZoneInfo.CreateCustomTimeZone(string, TimeSpan, string, string).
         *
         * The inputs are validated the way .NET's TimeZoneInfo.ValidateTimeZoneInfo validates
         * them, so this factory can no longer mint a zone .NET could not represent: an identifier
         * is required, and the offset must be a whole number of minutes within ±14 hours. The
         * offset bound also closes a door that reached TimeSpan's negation guard — a zone built
         * with TimeSpan::MinValue made ConvertTimeToUtc report a two's-complement diagnostic
         * instead of the argument being rejected where it was supplied.
         *
         * @param id           The zone identifier; must not be empty.
         * @param utcOffset    The zone's fixed offset from UTC; whole minutes, within ±14 hours.
         * @param displayName  The display name.
         * @param standardName The standard-time name, also used as the daylight name.
         * @throws ArgumentException if @p id is empty or @p utcOffset is finer than whole minutes.
         * @throws ArgumentOutOfRangeException if @p utcOffset is outside ±14 hours.
         */
        static std::shared_ptr<TimeZoneInfo> CreateCustomTimeZone(
            const std::string& id, const TimeSpan& utcOffset,
            const std::string& displayName, const std::string& standardName)
        {
            if (id.empty())
                throw ArgumentException("The specified ID parameter is not a valid time zone ID.",
                                        "id");
            validateUtcOffset(utcOffset);
            return std::shared_ptr<TimeZoneInfo>(
                new TimeZoneInfo(id, utcOffset, displayName, standardName, standardName, false));
        }

        /**
         * @brief Converts @p dt (assumed UTC) to the zone identified by @p destinationTimeZoneId.
         *
         * C++ counterpart of .NET TimeZoneInfo.ConvertTimeBySystemTimeZoneId(DateTime, string).
         */
        static DateTime ConvertTimeBySystemTimeZoneId(const DateTime& dt,
                                                      const std::string& destinationTimeZoneId) {
            auto tz = FindSystemTimeZoneById(destinationTimeZoneId);
            return dt.Add(tz->baseUtcOffset_);
        }

        /**
         * @brief Converts @p dt (assumed UTC) to the zone identified by @p destinationTimeZoneId.
         *
         * C++ counterpart of .NET TimeZoneInfo.ConvertTimeBySystemTimeZoneId(DateTime, string, string).
         */
        static DateTime ConvertTimeBySystemTimeZoneId(const DateTime& dt,
                                                      const std::string& sourceTimeZoneId,
                                                      const std::string& destinationTimeZoneId) {
            auto src = FindSystemTimeZoneById(sourceTimeZoneId);
            auto dst = FindSystemTimeZoneById(destinationTimeZoneId);
            DateTime utc = dt.Add(-src->baseUtcOffset_);
            return utc.Add(dst->baseUtcOffset_);
        }

        /**
         * @brief Converts a DateTimeOffset to the specified destination time zone.
         *
         * C++ counterpart of .NET TimeZoneInfo.ConvertTime(DateTimeOffset, TimeZoneInfo).
         */
        static DateTimeOffset ConvertTime(const DateTimeOffset& dto,
                                          const TimeZoneInfo& destinationTimeZone) {
            return DateTimeOffset(dto.getUtcDateTimeProperty().Add(destinationTimeZone.baseUtcOffset_),
                                  destinationTimeZone.baseUtcOffset_);
        }

        /**
         * @brief Converts @p dt (assumed UTC) to the specified destination time zone.
         *
         * C++ counterpart of .NET TimeZoneInfo.ConvertTime(DateTime, TimeZoneInfo).
         */
        static DateTime ConvertTime(const DateTime& dt, const TimeZoneInfo& destinationTimeZone) {
            return dt.Add(destinationTimeZone.baseUtcOffset_);
        }

        /**
         * @brief Converts @p dt from @p sourceTimeZone to @p destinationTimeZone.
         *
         * C++ counterpart of .NET TimeZoneInfo.ConvertTime(DateTime, TimeZoneInfo, TimeZoneInfo).
         */
        /**
         * @brief `DateTime` from a tick count, clamped to the representable range.
         *
         * Ticket #2186 (2026-08-18) answered question 1. The three conversion doors used
         * `DateTime::Add`, whose overflow is an `ArgumentOutOfRangeException`; .NET **clamps**:
         *
         * @code
         * private static DateTime SafeCreateDateTimeFromTicks(long ticks, DateTimeKind kind = …)
         *     => (ulong)ticks <= DateTime.MaxTicks ? new DateTime(ticks, kind)
         *                                          : (ticks < 0 ? DateTime.MinValue : DateTime.MaxValue);
         * @endcode
         * (`TimeZoneInfo.Cache.cs:340-342`), and `ConvertTime` builds its result through it
         * (`TimeZoneInfo.cs:685`).
         *
         * **The cast to `ulong` is the whole trick and is reproduced deliberately**: a negative
         * tick count wraps to something enormous, so one unsigned comparison rejects both ends of
         * the range at once. Spelling it as two signed comparisons would be equivalent, and this
         * spelling is kept because it is the reference's.
         *
         * .NET does NOT clamp everywhere. Its invalid-time compatibility path builds a raw
         * `new DateTime(...)` and lets it throw, with a comment saying so explicitly
         * (`TimeZoneInfo.cs:661-667`) — that path needs `TimeZoneInfoOptions` and adjustment
         * rules this port's `TimeZoneInfo` does not model, so it is not reachable here.
         */
        [[nodiscard]] static DateTime safeFromTicks(SharpRuntime::longcs ticks) {
            const auto unsignedTicks = static_cast<unsigned long long>(ticks);
            if (unsignedTicks <= static_cast<unsigned long long>(DateTime::MaxTicks))
                return DateTime(ticks);
            return ticks < 0 ? DateTime::MinValue : DateTime::MaxValue;
        }

        static DateTime ConvertTime(const DateTime& dt,
                                    const TimeZoneInfo& sourceTimeZone,
                                    const TimeZoneInfo& destinationTimeZone) {
            // #2186: the intermediate UTC ticks may leave the range while the final local ticks
            // land back inside it, which is why .NET computes the result "from raw ticks to avoid
            // precision loss from double-clamping" (TimeZoneInfo.cs:683-685) and clamps only once,
            // at the end.
            const SharpRuntime::longcs utcTicks =
                dt.getTicksProperty() - sourceTimeZone.baseUtcOffset_.getTicksProperty();
            return safeFromTicks(utcTicks + destinationTimeZone.baseUtcOffset_.getTicksProperty());
        }

        /**
         * @brief Converts a UTC DateTime to the specified destination time zone.
         *
         * C++ counterpart of .NET TimeZoneInfo.ConvertTimeFromUtc(DateTime, TimeZoneInfo).
         */
        static DateTime ConvertTimeFromUtc(const DateTime& dt,
                                           const TimeZoneInfo& destinationTimeZone) {
            return safeFromTicks(dt.getTicksProperty() +
                                 destinationTimeZone.baseUtcOffset_.getTicksProperty());   // #2186
        }

        /**
         * @brief Converts a DateTime to UTC using the specified source time zone.
         *
         * C++ counterpart of .NET TimeZoneInfo.ConvertTimeToUtc(DateTime, TimeZoneInfo).
         */
        static DateTime ConvertTimeToUtc(const DateTime& dt, const TimeZoneInfo& sourceTimeZone) {
            return safeFromTicks(dt.getTicksProperty() -
                                 sourceTimeZone.baseUtcOffset_.getTicksProperty());   // #2186
        }

        /**
         * @brief Converts a DateTimeOffset to the zone identified by @p destinationTimeZoneId.
         *
         * C++ counterpart of .NET TimeZoneInfo.ConvertTimeBySystemTimeZoneId(DateTimeOffset, string).
         */
        static DateTimeOffset ConvertTimeBySystemTimeZoneId(const DateTimeOffset& dto,
                                                            const std::string& destinationTimeZoneId) {
            auto tz = FindSystemTimeZoneById(destinationTimeZoneId);
            return ConvertTime(dto, *tz);
        }

        /**
         * @brief Tries to map an IANA timezone ID to a Windows timezone ID.
         *
         * C++ counterpart of .NET TimeZoneInfo.TryConvertIanaIdToWindowsId(string, out string).
         * Uses the CLDR-derived IANA→Windows mapping table.
         * @param ianaId    The IANA timezone identifier (e.g. "Europe/Prague").
         * @param windowsId Receives the Windows timezone name on success.
         * @return true if a mapping was found; false otherwise.
         */
        static bool TryConvertIanaIdToWindowsId(const std::string& ianaId, std::string& windowsId);

        /**
         * @brief Tries to map a Windows timezone ID to an IANA timezone ID.
         *
         * C++ counterpart of .NET TimeZoneInfo.TryConvertWindowsIdToIanaId(string, out string).
         * Returns the first IANA ID that maps to @p windowsId in the CLDR table.
         * @param windowsId The Windows timezone identifier (e.g. "Central Europe Standard Time").
         * @param ianaId    Receives the IANA timezone name on success.
         * @return true if a mapping was found; false otherwise.
         */
        static bool TryConvertWindowsIdToIanaId(const std::string& windowsId, std::string& ianaId);

    };

} // namespace System
