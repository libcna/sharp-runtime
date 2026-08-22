// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/TimeZone.hpp"
#include "System/DateTime.hpp"
#include "System/DateTimeOffset.hpp"
#include "System/TimeSpan.hpp"
#include "System/detail/ProcessTimeZoneState.hpp"

#include <string>

using System::TimeZone;
using System::DateTime;
using System::TimeSpan;

namespace {

class ConcreteTimeZone final : public TimeZone {
    std::string std_ = "TestStandard";
    std::string dst_ = "TestDaylight";
public:
    const std::string& getStandardNameProperty()  const override { return std_; }
    const std::string& getDaylightNameProperty()  const override { return dst_; }
    TimeSpan GetUtcOffset(const DateTime& /*time*/) const override {
        return TimeSpan::FromHours(2.0);
    }
    bool IsDaylightSavingTime(const DateTime& /*time*/) const override { return false; }
};

} // namespace

TEST(TimeZoneTest, StandardName) {
    ConcreteTimeZone tz;
    EXPECT_EQ(tz.getStandardNameProperty(), "TestStandard");
}

TEST(TimeZoneTest, DaylightName) {
    ConcreteTimeZone tz;
    EXPECT_EQ(tz.getDaylightNameProperty(), "TestDaylight");
}

TEST(TimeZoneTest, GetUtcOffset) {
    ConcreteTimeZone tz;
    DateTime now;
    EXPECT_EQ(tz.GetUtcOffset(now).getTotalHoursProperty(), 2.0);
}

TEST(TimeZoneTest, IsDaylightSavingTime) {
    ConcreteTimeZone tz;
    DateTime now;
    EXPECT_FALSE(tz.IsDaylightSavingTime(now));
}

TEST(TimeZoneTest, CurrentTimeZoneDoesNotThrow) {
    EXPECT_NO_THROW((void)TimeZone::CurrentTimeZone());
}

TEST(TimeZoneTest, CurrentTimeZoneHasStandardName) {
    const TimeZone& tz = TimeZone::CurrentTimeZone();
    EXPECT_FALSE(tz.getStandardNameProperty().empty());
}

TEST(TimeZoneTest, CurrentTimeZoneSameReference) {
    const TimeZone& a = TimeZone::CurrentTimeZone();
    const TimeZone& b = TimeZone::CurrentTimeZone();
    EXPECT_EQ(&a, &b);
}

TEST(TimeZoneTest, PolymorphicRef) {
    ConcreteTimeZone tz;
    TimeZone& ref = tz;
    EXPECT_EQ(ref.getStandardNameProperty(), "TestStandard");
}

// ---------------------------------------------------------------------------
// Ticket #2182 (SR-AUD-223): the CurrentTimeZone adapter snapshotted
// TimeZoneInfo::Local().BaseUtcOffset once at static construction and returned it
// from GetUtcOffset() for every date, and IsDaylightSavingTime() was hard-coded
// to false. Measured before the fix (build-probe/2176_probe1_surface.log) under
// TZ=America/New_York: -240 minutes and DST false for BOTH January and July,
// where the system database says -18000 s / isdst 0 and -14400 s / isdst 1, and
// the audit's current-.NET probe reports -5/-4 and False/True.
//
// Ticket #2184 (post-audit, no SR-AUD identifier): the TZ save/restore window in
// FindSystemTimeZoneById was straight-line code, so an exception left the
// process-global zone clobbered, and the restore branched on savedStr.empty(), so
// an empty-but-set TZ -- which POSIX defines as UTC, unlike an absent TZ -- was
// unsetenv()d rather than restored. Measured before the fix: TZ set to "" before
// the call, UNSET after.
//
// These tests mutate a process-global, so every one of them restores TZ through
// an RAII guard and none of them leaves the process in a different zone than it
// found it. TimeZoneInfo::Local() and CurrentTimeZone() both cache on first use,
// so nothing here asserts a *cached* value; the adapter resolves per call against
// whatever zone is selected when the call happens, which is what makes it testable.
// ---------------------------------------------------------------------------

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)

#include <cstdlib>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include "System/TimeZoneInfo.hpp"

namespace {

/** Selects a zone for the duration of a scope and restores the previous TZ exactly. */
class ScopedTestTz {
    std::string saved_;
    bool        wasSet_;

public:
    explicit ScopedTestTz(const char* zone) {
        std::lock_guard<std::mutex> lock(System::detail::processTimeZoneMutex());
        const char* current = getenv("TZ");
        wasSet_ = current != nullptr;
        if (wasSet_) saved_ = current;
        setenv("TZ", zone, 1);
        tzset();
    }
    ScopedTestTz(const ScopedTestTz&)            = delete;
    ScopedTestTz& operator=(const ScopedTestTz&) = delete;
    ~ScopedTestTz() {
        std::lock_guard<std::mutex> lock(System::detail::processTimeZoneMutex());
        if (wasSet_) setenv("TZ", saved_.c_str(), 1);
        else         unsetenv("TZ");
        tzset();
    }
};

bool zoneInstalled(const char* id) {
    std::shared_ptr<System::TimeZoneInfo> tz;
    return System::TimeZoneInfo::TryFindSystemTimeZoneById(id, tz);
}

long long offsetMinutesFor(const System::DateTime& when) {
    return System::TimeZone::CurrentTimeZone().GetUtcOffset(when).getTicksProperty() /
           System::TimeSpan::TicksPerMinute;
}

} // namespace

TEST(TimeZoneTest, CurrentTimeZone_OffsetFollowsTheDateAcrossADaylightBoundary) {
    if (!zoneInstalled("America/New_York")) GTEST_SKIP() << "America/New_York is not installed";
    ScopedTestTz tz("America/New_York");
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 1, 15, 12, 0, 0)), -300);
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 7, 15, 12, 0, 0)), -240);
}

TEST(TimeZoneTest, CurrentTimeZone_IsDaylightSavingTimeFollowsTheDate) {
    if (!zoneInstalled("America/New_York")) GTEST_SKIP() << "America/New_York is not installed";
    ScopedTestTz tz("America/New_York");
    const TimeZone& ctz = TimeZone::CurrentTimeZone();
    EXPECT_FALSE(ctz.IsDaylightSavingTime(DateTime(2025, 1, 15, 12, 0, 0)));
    EXPECT_TRUE(ctz.IsDaylightSavingTime(DateTime(2025, 7, 15, 12, 0, 0)));
}

TEST(TimeZoneTest, CurrentTimeZone_SpringForwardBoundary) {
    // 2025-03-09 02:00 local is the US spring-forward instant. 01:59 is still standard time;
    // 03:00 is daylight time. 02:30 does not exist -- mktime normalises it forward, which is
    // recorded in docs/SystemTimeZoneNamespaceReviewPlan.md section 12 as a deliberate
    // consequence of the legacy surface having no IsInvalidTime member to report it.
    if (!zoneInstalled("America/New_York")) GTEST_SKIP() << "America/New_York is not installed";
    ScopedTestTz tz("America/New_York");
    const TimeZone& ctz = TimeZone::CurrentTimeZone();
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 3, 9, 1, 59, 0)), -300);
    EXPECT_FALSE(ctz.IsDaylightSavingTime(DateTime(2025, 3, 9, 1, 59, 0)));
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 3, 9, 3, 0, 0)), -240);
    EXPECT_TRUE(ctz.IsDaylightSavingTime(DateTime(2025, 3, 9, 3, 0, 0)));
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 3, 9, 2, 30, 0)), -240);
}

TEST(TimeZoneTest, CurrentTimeZone_FallBackBoundary) {
    // 2025-11-02 02:00 local is the US fall-back instant. 00:59 is still daylight time and
    // 03:00 is standard time.
    if (!zoneInstalled("America/New_York")) GTEST_SKIP() << "America/New_York is not installed";
    ScopedTestTz tz("America/New_York");
    const TimeZone& ctz = TimeZone::CurrentTimeZone();
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 11, 2, 0, 59, 0)), -240);
    EXPECT_TRUE(ctz.IsDaylightSavingTime(DateTime(2025, 11, 2, 0, 59, 0)));
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 11, 2, 3, 0, 0)), -300);
    EXPECT_FALSE(ctz.IsDaylightSavingTime(DateTime(2025, 11, 2, 3, 0, 0)));
}

TEST(TimeZoneTest, CurrentTimeZone_AmbiguousHourIsDeterministicRegardlessOfCallOrder) {
    // 2025-11-02 01:30 occurs twice in America/New_York. Measured in
    // build-probe/2176_probe6_ambiguous.log, raw glibc mktime(tm_isdst = -1) answers -240 as
    // the first call in a process, -300 right after a standard-time conversion and -240 right
    // after a daylight one -- the same argument giving three different answers depending on
    // unrelated earlier calls. The adapter resolves the ambiguity explicitly, so the answer is
    // now a function of the argument alone.
    //
    // #2186 question 5, answered 2026-08-18: the STANDARD reading is .NET's, and #2182 chose
    // right. TimeZone.CalculateUtcOffset first decides isDst from the DST window, and then, for a
    // time INSIDE the ambiguous window, overrides it:
    //
    //     if (isDst && time >= ambiguousStart && time < ambiguousEnd)
    //         isDst = time.IsAmbiguousDaylightSavingTime();     -- TimeZone.cs:237-240
    //
    // That flag is set only by a prior UTC-to-local conversion, so a DateTime constructed from
    // its fields carries false and the answer is the STANDARD offset. Reading only the window
    // test above it -- which puts 01:30 inside daylight -- gives the opposite answer, and that is
    // the trap in this function: the override is what decides it, not the window.
    if (!zoneInstalled("America/New_York")) GTEST_SKIP() << "America/New_York is not installed";
    ScopedTestTz tz("America/New_York");
    const TimeZone& ctz = TimeZone::CurrentTimeZone();
    const DateTime ambiguous(2025, 11, 2, 1, 30, 0);

    // Drive the process into the daylight state, then ask.
    (void)ctz.GetUtcOffset(DateTime(2025, 7, 15, 12, 0, 0));
    const long long afterDaylight = offsetMinutesFor(ambiguous);
    const bool afterDaylightDst = ctz.IsDaylightSavingTime(ambiguous);

    // Drive it into the standard state, then ask the identical question.
    (void)ctz.GetUtcOffset(DateTime(2025, 1, 15, 12, 0, 0));
    const long long afterStandard = offsetMinutesFor(ambiguous);
    const bool afterStandardDst = ctz.IsDaylightSavingTime(ambiguous);

    EXPECT_EQ(afterDaylight, afterStandard);
    EXPECT_EQ(afterDaylightDst, afterStandardDst);
    EXPECT_EQ(afterStandard, -300);
    EXPECT_FALSE(afterStandardDst);
}

TEST(TimeZoneTest, CurrentTimeZone_NonExistentHourIsAlsoOrderIndependent) {
    // 02:30 on a spring-forward date does not exist. It was already stable under raw mktime;
    // this pins that the ambiguity handling did not make it order-dependent.
    if (!zoneInstalled("America/New_York")) GTEST_SKIP() << "America/New_York is not installed";
    ScopedTestTz tz("America/New_York");
    const TimeZone& ctz = TimeZone::CurrentTimeZone();
    const DateTime gap(2025, 3, 9, 2, 30, 0);
    (void)ctz.GetUtcOffset(DateTime(2025, 7, 15, 12, 0, 0));
    const long long afterDaylight = offsetMinutesFor(gap);
    (void)ctz.GetUtcOffset(DateTime(2025, 1, 15, 12, 0, 0));
    const long long afterStandard = offsetMinutesFor(gap);
    EXPECT_EQ(afterDaylight, afterStandard);
    EXPECT_EQ(afterStandard, -240);
}

TEST(TimeZoneTest, CurrentTimeZone_OrdinaryDaylightTimeSurvivesTheAmbiguityHandling) {
    // The standard re-ask must not steal an ordinary summer date: 12:00 in July is not
    // ambiguous, so asking for it as standard time normalises the fields and is rejected.
    if (!zoneInstalled("America/New_York")) GTEST_SKIP() << "America/New_York is not installed";
    ScopedTestTz tz("America/New_York");
    const TimeZone& ctz = TimeZone::CurrentTimeZone();
    for (int month : {4, 5, 6, 7, 8, 9, 10}) {
        DateTime summer(2025, month, 15, 12, 0, 0);
        EXPECT_EQ(offsetMinutesFor(summer), -240) << "month " << month;
        EXPECT_TRUE(ctz.IsDaylightSavingTime(summer)) << "month " << month;
    }
    for (int month : {1, 2, 12}) {
        DateTime winter(2025, month, 15, 12, 0, 0);
        EXPECT_EQ(offsetMinutesFor(winter), -300) << "month " << month;
        EXPECT_FALSE(ctz.IsDaylightSavingTime(winter)) << "month " << month;
    }
}

TEST(TimeZoneTest, CurrentTimeZone_YearBoundariesAreStandardTimeInNewYork) {
    if (!zoneInstalled("America/New_York")) GTEST_SKIP() << "America/New_York is not installed";
    ScopedTestTz tz("America/New_York");
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 1, 1, 0, 0, 0)), -300);
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 12, 31, 23, 59, 0)), -300);
}

TEST(TimeZoneTest, CurrentTimeZone_SouthernHemisphereDaylightIsInJanuary) {
    if (!zoneInstalled("Australia/Sydney")) GTEST_SKIP() << "Australia/Sydney is not installed";
    ScopedTestTz tz("Australia/Sydney");
    const TimeZone& ctz = TimeZone::CurrentTimeZone();
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 1, 15, 12, 0, 0)), 660);
    EXPECT_TRUE(ctz.IsDaylightSavingTime(DateTime(2025, 1, 15, 12, 0, 0)));
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 7, 15, 12, 0, 0)), 600);
    EXPECT_FALSE(ctz.IsDaylightSavingTime(DateTime(2025, 7, 15, 12, 0, 0)));
}

TEST(TimeZoneTest, CurrentTimeZone_ZoneWithoutDaylightTimeIsConstantAcrossTheYear) {
    if (!zoneInstalled("Asia/Kolkata")) GTEST_SKIP() << "Asia/Kolkata is not installed";
    ScopedTestTz tz("Asia/Kolkata");
    const TimeZone& ctz = TimeZone::CurrentTimeZone();
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 1, 15, 12, 0, 0)), 330);
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 7, 15, 12, 0, 0)), 330);
    EXPECT_FALSE(ctz.IsDaylightSavingTime(DateTime(2025, 7, 15, 12, 0, 0)));
}

TEST(TimeZoneTest, CurrentTimeZone_UtcHasNoDaylightTimeOnAnyDate) {
    ScopedTestTz tz("Etc/UTC");
    const TimeZone& ctz = TimeZone::CurrentTimeZone();
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 1, 15, 12, 0, 0)), 0);
    EXPECT_EQ(offsetMinutesFor(DateTime(2025, 7, 15, 12, 0, 0)), 0);
    EXPECT_FALSE(ctz.IsDaylightSavingTime(DateTime(2025, 7, 15, 12, 0, 0)));
}

TEST(TimeZoneTest, CurrentTimeZone_DateTimeRangeExtremesDoNotThrowOrAbort) {
    // Year 1 and year 9999 are far outside the range a 32-bit time_t could express; the
    // adapter must answer or fall back, never throw out of a legacy accessor.
    ScopedTestTz tz("Etc/UTC");
    const TimeZone& ctz = TimeZone::CurrentTimeZone();
    EXPECT_NO_THROW((void)ctz.GetUtcOffset(DateTime::MinValue));
    EXPECT_NO_THROW((void)ctz.GetUtcOffset(DateTime::MaxValue));
    EXPECT_NO_THROW((void)ctz.IsDaylightSavingTime(DateTime::MinValue));
    EXPECT_NO_THROW((void)ctz.IsDaylightSavingTime(DateTime::MaxValue));
    EXPECT_EQ(offsetMinutesFor(DateTime::MinValue), 0);
    EXPECT_EQ(offsetMinutesFor(DateTime::MaxValue), 0);
}

TEST(TimeZoneTest, CurrentTimeZone_RepeatedCallsForOneDateAgree) {
    if (!zoneInstalled("America/New_York")) GTEST_SKIP() << "America/New_York is not installed";
    ScopedTestTz tz("America/New_York");
    DateTime when(2025, 7, 15, 12, 0, 0);
    EXPECT_EQ(offsetMinutesFor(when), offsetMinutesFor(when));
    EXPECT_EQ(TimeZone::CurrentTimeZone().IsDaylightSavingTime(when),
              TimeZone::CurrentTimeZone().IsDaylightSavingTime(when));
}

TEST(TimeZoneTest, CurrentTimeZone_UtcKindUsesThePublicUtcSemantics) {
    if (!zoneInstalled("America/New_York")) GTEST_SKIP() << "America/New_York is not installed";
    ScopedTestTz tz("America/New_York");
    const TimeZone& ctz = TimeZone::CurrentTimeZone();

    const DateTime utc = DateTime::SpecifyKind(
        DateTime(2025, 7, 15, 12, 0, 0), System::DateTimeKind::Utc);
    EXPECT_EQ(ctz.GetUtcOffset(utc), TimeSpan::Zero);
    EXPECT_FALSE(ctz.IsDaylightSavingTime(utc));

    // Identical calendar fields with Local or Unspecified Kind are local wall-clock questions.
    const DateTime unspecified(2025, 7, 15, 12, 0, 0);
    const DateTime local = DateTime::SpecifyKind(unspecified, System::DateTimeKind::Local);
    EXPECT_EQ(offsetMinutesFor(unspecified), -240);
    EXPECT_EQ(offsetMinutesFor(local), -240);
    EXPECT_TRUE(ctz.IsDaylightSavingTime(unspecified));
    EXPECT_TRUE(ctz.IsDaylightSavingTime(local));
}

TEST(TimeZoneTest, ToLocalTime_SelectsOffsetByUtcInstantAcrossDstBoundaries) {
    if (!zoneInstalled("America/New_York")) GTEST_SKIP() << "America/New_York is not installed";
    ScopedTestTz tz("America/New_York");
    const TimeZone& ctz = TimeZone::CurrentTimeZone();

    const DateTime beforeSpring = DateTime::SpecifyKind(
        DateTime(2025, 3, 9, 6, 30, 0), System::DateTimeKind::Utc);
    const DateTime afterSpring = DateTime::SpecifyKind(
        DateTime(2025, 3, 9, 7, 30, 0), System::DateTimeKind::Utc);
    const DateTime beforeFall = DateTime::SpecifyKind(
        DateTime(2025, 11, 2, 5, 30, 0), System::DateTimeKind::Utc);
    const DateTime afterFall = DateTime::SpecifyKind(
        DateTime(2025, 11, 2, 6, 30, 0), System::DateTimeKind::Utc);

    const DateTime springStandard = beforeSpring.ToLocalTime(ctz);
    const DateTime springDaylight = afterSpring.ToLocalTime(ctz);
    const DateTime fallDaylight = beforeFall.ToLocalTime(ctz);
    const DateTime fallStandard = afterFall.ToLocalTime(ctz);

    EXPECT_EQ(ctz.GetUtcOffsetFromUniversalTime(beforeSpring).getTotalMinutesProperty(), -300);
    EXPECT_EQ(ctz.GetUtcOffsetFromUniversalTime(afterSpring).getTotalMinutesProperty(), -240);
    EXPECT_EQ(ctz.GetUtcOffsetFromUniversalTime(beforeFall).getTotalMinutesProperty(), -240);
    EXPECT_EQ(ctz.GetUtcOffsetFromUniversalTime(afterFall).getTotalMinutesProperty(), -300);

    EXPECT_EQ(springStandard, DateTime(2025, 3, 9, 1, 30, 0));
    EXPECT_EQ(springDaylight, DateTime(2025, 3, 9, 3, 30, 0));
    EXPECT_EQ(fallDaylight, DateTime(2025, 11, 2, 1, 30, 0));
    EXPECT_EQ(fallStandard, DateTime(2025, 11, 2, 1, 30, 0));
    EXPECT_EQ(springStandard.getKindProperty(), System::DateTimeKind::Local);
    EXPECT_EQ(springDaylight.getKindProperty(), System::DateTimeKind::Local);
    EXPECT_EQ(fallDaylight.getKindProperty(), System::DateTimeKind::Local);
    EXPECT_EQ(fallStandard.getKindProperty(), System::DateTimeKind::Local);

    // Neither spring value is ambiguous, so both directions must be exact. The second fall
    // occurrence is the standard reading selected for a field-only Local DateTime and is exact
    // too. The first fall occurrence needs .NET's hidden LocalAmbiguousDst marker, which this
    // practical subset still does not produce, so no false round-trip promise is made for it.
    EXPECT_EQ(springStandard.ToUniversalTime(ctz), beforeSpring);
    EXPECT_EQ(springDaylight.ToUniversalTime(ctz), afterSpring);
    EXPECT_EQ(fallStandard.ToUniversalTime(ctz), afterFall);
}

TEST(TimeZoneTest, ProcessTimeZoneMutexSerializesCoreReadersAndZoneSelection) {
    if (!zoneInstalled("Asia/Tokyo")) GTEST_SKIP() << "Asia/Tokyo is not installed";
    using namespace std::chrono_literals;

    std::promise<void> dateTimeStarted;
    std::promise<void> offsetStarted;
    std::promise<void> lookupStarted;
    auto dateTimeReady = dateTimeStarted.get_future();
    auto offsetReady = offsetStarted.get_future();
    auto lookupReady = lookupStarted.get_future();

    std::unique_lock<std::mutex> lock(System::detail::processTimeZoneMutex());
    auto dateTimeFuture = std::async(std::launch::async, [&dateTimeStarted] {
        dateTimeStarted.set_value();
        return DateTime::getNowProperty();
    });
    auto offsetFuture = std::async(std::launch::async, [&offsetStarted] {
        offsetStarted.set_value();
        return System::DateTimeOffset::getNowProperty();
    });
    auto lookupFuture = std::async(std::launch::async, [&lookupStarted] {
        lookupStarted.set_value();
        std::shared_ptr<System::TimeZoneInfo> zone;
        return System::TimeZoneInfo::TryFindSystemTimeZoneById("Asia/Tokyo", zone);
    });

    dateTimeReady.wait();
    offsetReady.wait();
    lookupReady.wait();
    EXPECT_EQ(dateTimeFuture.wait_for(100ms), std::future_status::timeout);
    EXPECT_EQ(offsetFuture.wait_for(100ms), std::future_status::timeout);
    EXPECT_EQ(lookupFuture.wait_for(100ms), std::future_status::timeout);

    lock.unlock();
    EXPECT_EQ(dateTimeFuture.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(offsetFuture.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(lookupFuture.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(dateTimeFuture.get().getKindProperty(), System::DateTimeKind::Local);
    EXPECT_EQ(offsetFuture.get().getDateTimeProperty().getKindProperty(),
              System::DateTimeKind::Unspecified);
    EXPECT_TRUE(lookupFuture.get());
}

TEST(TimeZoneTest, FindSystemTimeZoneById_RestoresAPreviouslySetTz) {
    ScopedTestTz tz("Europe/Prague");
    std::shared_ptr<System::TimeZoneInfo> found;
    (void)System::TimeZoneInfo::TryFindSystemTimeZoneById("Asia/Tokyo", found);
    const char* after = getenv("TZ");
    ASSERT_NE(after, nullptr);
    EXPECT_STREQ(after, "Europe/Prague");
}

TEST(TimeZoneTest, FindSystemTimeZoneById_RestoresAnEmptyButSetTz) {
    // POSIX: TZ="" selects UTC, an absent TZ selects the system default. The previous restore
    // branched on emptiness and therefore deleted the variable, silently moving the process
    // from "UTC" to "whatever /etc/localtime says".
    ScopedTestTz tz("");
    ASSERT_NE(getenv("TZ"), nullptr);
    std::shared_ptr<System::TimeZoneInfo> found;
    (void)System::TimeZoneInfo::TryFindSystemTimeZoneById("Asia/Tokyo", found);
    const char* after = getenv("TZ");
    ASSERT_NE(after, nullptr) << "an empty-but-set TZ was deleted rather than restored";
    EXPECT_STREQ(after, "");
}

TEST(TimeZoneTest, FindSystemTimeZoneById_RestoresTzAfterAFailedLookup) {
    // The rejecting paths must be as clean as the succeeding one.
    ScopedTestTz tz("Europe/Prague");
    std::shared_ptr<System::TimeZoneInfo> found;
    EXPECT_FALSE(System::TimeZoneInfo::TryFindSystemTimeZoneById("Mars/Olympus", found));
    EXPECT_FALSE(System::TimeZoneInfo::TryFindSystemTimeZoneById("zone.tab", found));
    EXPECT_FALSE(System::TimeZoneInfo::TryFindSystemTimeZoneById("America//New_York", found));
    const char* after = getenv("TZ");
    ASSERT_NE(after, nullptr);
    EXPECT_STREQ(after, "Europe/Prague");
}

TEST(TimeZoneTest, FindSystemTimeZoneById_LeavesTzUnsetWhenItWasUnset) {
    std::string saved;
    bool wasSet = false;
    {
        std::lock_guard<std::mutex> lock(System::detail::processTimeZoneMutex());
        const char* original = getenv("TZ");
        saved = original ? original : "";
        wasSet = original != nullptr;
        unsetenv("TZ");
        tzset();
    }
    std::shared_ptr<System::TimeZoneInfo> found;
    (void)System::TimeZoneInfo::TryFindSystemTimeZoneById("Asia/Tokyo", found);
    bool stillUnset = false;
    {
        std::lock_guard<std::mutex> lock(System::detail::processTimeZoneMutex());
        stillUnset = getenv("TZ") == nullptr;
        if (wasSet) setenv("TZ", saved.c_str(), 1);
        tzset();
    }
    EXPECT_TRUE(stillUnset);
}

#endif // !defined(_WIN32) && !defined(__EMSCRIPTEN__)
