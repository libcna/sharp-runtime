// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
#include <gtest/gtest.h>
#include <string>
#include <optional>
#include <thread>
#include <atomic>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Globalization/TextInfo.hpp"
#include "System/Globalization/TextElementEnumerator.hpp"
#include "System/Globalization/SortKey.hpp"
#include "System/Globalization/CompareInfo.hpp"
#include "System/Globalization/CultureInfo.hpp"
#include "System/Globalization/CharUnicodeInfo.hpp"
#include "System/Globalization/DateTimeFormatInfo.hpp"
#include "System/Globalization/JulianCalendar.hpp"
#include "System/Globalization/ThaiBuddhistCalendar.hpp"
#include "System/Globalization/TaiwanCalendar.hpp"
#include "System/Globalization/PersianCalendar.hpp"

using namespace System::Globalization;

// --- TextInfo ---
TEST(TextInfoTests, ToUpper) {
    TextInfo ti;
    EXPECT_EQ(ti.ToUpper("hello"), "HELLO");
}
TEST(TextInfoTests, ToLower) {
    TextInfo ti;
    EXPECT_EQ(ti.ToLower("WORLD"), "world");
}
TEST(TextInfoTests, ToTitleCase) {
    TextInfo ti;
    EXPECT_EQ(ti.ToTitleCase("hello world"), "Hello World");
}

// --- TextElementEnumerator ---
TEST(TextElementEnumeratorTests, ASCIIIteration) {
    TextElementEnumerator e("abc");
    EXPECT_TRUE(e.MoveNext()); EXPECT_EQ(e.GetTextElement(), "a");
    EXPECT_TRUE(e.MoveNext()); EXPECT_EQ(e.GetTextElement(), "b");
    EXPECT_TRUE(e.MoveNext()); EXPECT_EQ(e.GetTextElement(), "c");
    EXPECT_FALSE(e.MoveNext());
}
TEST(TextElementEnumeratorTests, Reset) {
    TextElementEnumerator e("ab");
    e.MoveNext(); e.Reset();
    EXPECT_TRUE(e.MoveNext()); EXPECT_EQ(e.GetTextElement(), "a");
}

// --- SortKey ---
TEST(SortKeyTests, Compare) {
    SortKey k1{"a", {0x61}};
    SortKey k2{"b", {0x62}};
    EXPECT_LT(SortKey::Compare(k1,k2), 0);
    EXPECT_GT(SortKey::Compare(k2,k1), 0);
    EXPECT_EQ(SortKey::Compare(k1,k1), 0);
}

// --- CompareInfo ---
TEST(CompareInfoTests, Compare) {
    CompareInfo ci;
    EXPECT_EQ(ci.Compare("abc","abc"), 0);
    EXPECT_LT(ci.Compare("abc","abd"), 0);
}
TEST(CompareInfoTests, IgnoreCase) {
    CompareInfo ci;
    EXPECT_EQ(ci.Compare("Hello","hello", CompareOptions::IgnoreCase), 0);
}
TEST(CompareInfoTests, IsPrefix) {
    CompareInfo ci;
    EXPECT_TRUE(ci.IsPrefix("Hello World", "Hello"));
    EXPECT_FALSE(ci.IsPrefix("Hello World", "World"));
}
TEST(CompareInfoTests, IsSuffix) {
    CompareInfo ci;
    EXPECT_TRUE(ci.IsSuffix("Hello World", "World"));
    EXPECT_FALSE(ci.IsSuffix("Hello World", "Hello"));
}
TEST(CompareInfoTests, IndexOf) {
    CompareInfo ci;
    EXPECT_EQ(ci.IndexOf("Hello World", "World"), 6);
    EXPECT_EQ(ci.IndexOf("Hello World", "xyz"), -1);
}

// --- CharUnicodeInfo ---
TEST(CharUnicodeInfoTests, DecimalDigit) {
    EXPECT_EQ(CharUnicodeInfo::GetDecimalDigitValue(u'5'), 5);
    EXPECT_EQ(CharUnicodeInfo::GetDecimalDigitValue(u'a'), -1);
}
TEST(CharUnicodeInfoTests, NumericValue) {
    EXPECT_DOUBLE_EQ(CharUnicodeInfo::GetNumericValue(u'7'), 7.0);
    EXPECT_DOUBLE_EQ(CharUnicodeInfo::GetNumericValue(u'x'), -1.0);
}
TEST(CharUnicodeInfoTests, Category) {
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(u'A'), UnicodeCategory::UppercaseLetter);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(u'a'), UnicodeCategory::LowercaseLetter);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(u'5'), UnicodeCategory::DecimalDigitNumber);
}

// --- DateTimeFormatInfo ---
TEST(DateTimeFormatInfoTests, MonthNames) {
    DateTimeFormatInfo dtfi;
    EXPECT_EQ(dtfi.GetMonthName(1), "January");
    EXPECT_EQ(dtfi.GetMonthName(12), "December");
}
TEST(DateTimeFormatInfoTests, DayNames) {
    DateTimeFormatInfo dtfi;
    EXPECT_EQ(dtfi.GetDayName(System::DayOfWeek::Monday), "Monday");
}

// Indexing the internal day-names array with an invalid DayOfWeek was previously undefined
// behavior (std::array::operator[] does not bounds-check), not a thrown exception; real
// .NET's base implementation validates first (DateTimeFormatInfo.cs's
// `(uint)dow >= (uint)names.Length` check).
TEST(DateTimeFormatInfoTests, GetDayName_InvalidValue_Throws) {
    DateTimeFormatInfo dtfi;
    auto invalid = static_cast<System::DayOfWeek>(7);
    EXPECT_THROW(dtfi.GetDayName(invalid), System::ArgumentOutOfRangeException);
    EXPECT_THROW(dtfi.GetAbbreviatedDayName(invalid), System::ArgumentOutOfRangeException);
    EXPECT_THROW(dtfi.GetShortestDayName(invalid), System::ArgumentOutOfRangeException);
}
TEST(DateTimeFormatInfoTests, AbbreviatedMonthNames) {
    DateTimeFormatInfo dtfi;
    EXPECT_EQ(dtfi.GetAbbreviatedMonthName(1), "Jan");
}

// --- Calendars ---
TEST(JulianCalendarTests, LeapYear) {
    JulianCalendar cal;
    EXPECT_TRUE(cal.IsLeapYear(100));  // Julian: every 4th year
    EXPECT_TRUE(cal.IsLeapYear(400));
}
TEST(JulianCalendarTests, AddMonths_LargeValue_ThrowsInsteadOfOverflowing) {
    // JulianCalendar::AddMonths is a virtual override that replaces (not augments)
    // Calendar::AddMonths's own |months|>120000 check, and previously had none of its own --
    // `i = m - 1 + months` is real signed-integer-overflow UB in C++ for a months argument
    // as simple as INT_MAX.
    JulianCalendar cal;
    System::DateTime dt(2024, 1, 1);
    EXPECT_THROW(cal.AddMonths(dt, 1000000000), System::ArgumentOutOfRangeException);
    EXPECT_THROW(cal.AddMonths(dt, -1000000000), System::ArgumentOutOfRangeException);
}
TEST(JulianCalendarTests, AddYears_LargeValue_ThrowsInsteadOfOverflowing) {
    // `years * 12` computed directly with no upfront bounds check is real signed-integer-
    // overflow UB in C++ for a merely large years argument.
    JulianCalendar cal;
    System::DateTime dt(2024, 1, 1);
    EXPECT_THROW(cal.AddYears(dt, 200000000), System::ArgumentOutOfRangeException);
    EXPECT_THROW(cal.AddYears(dt, -200000000), System::ArgumentOutOfRangeException);
}
TEST(ThaiBuddhistCalendarTests, Year) {
    ThaiBuddhistCalendar cal;
    System::DateTime dt(2024,1,1);
    EXPECT_EQ(cal.GetYear(dt), 2567);
}
TEST(ThaiBuddhistCalendarTests, AddYears_LargeValue_ThrowsInsteadOfOverflowing) {
    // ThaiBuddhistCalendar doesn't override AddYears -- inherits Calendar::AddYears (the
    // base-class default). `years * 12` computed directly with no upfront bounds check was
    // real signed-integer-overflow UB in C++ for a merely large years argument; this exercises
    // the fix at the base-class level, which every non-overriding calendar subclass inherits.
    ThaiBuddhistCalendar cal;
    System::DateTime dt(2024, 1, 1);
    EXPECT_THROW(cal.AddYears(dt, 200000000), System::ArgumentOutOfRangeException);
    EXPECT_THROW(cal.AddYears(dt, -200000000), System::ArgumentOutOfRangeException);
}
TEST(ThaiBuddhistCalendarTests, AddWeeks_LargeValue_ThrowsInsteadOfOverflowing) {
    // `weeks * 7` computed directly with no upfront bounds check was real signed-integer-
    // overflow UB in C++ for a merely large weeks argument (Calendar::AddWeeks, base class).
    ThaiBuddhistCalendar cal;
    System::DateTime dt(2024, 1, 1);
    EXPECT_THROW(cal.AddWeeks(dt, 400000000), System::ArgumentOutOfRangeException);
    EXPECT_THROW(cal.AddWeeks(dt, -400000000), System::ArgumentOutOfRangeException);
}
TEST(ThaiBuddhistCalendarTests, Eras_ContainsThaiBuddhistEra) {
    ThaiBuddhistCalendar cal;
    auto eras = cal.getErasProperty();
    ASSERT_EQ(eras.size(), 1u);
    EXPECT_EQ(eras[0], ThaiBuddhistCalendar::ThaiBuddhistEra);
}
TEST(TaiwanCalendarTests, Year) {
    TaiwanCalendar cal;
    System::DateTime dt(2024,1,1);
    EXPECT_EQ(cal.GetYear(dt), 113);
}
TEST(PersianCalendarTests, LeapYear) {
    PersianCalendar cal;
    // 1403 is a Persian leap year
    EXPECT_TRUE(cal.IsLeapYear(1403));
    EXPECT_FALSE(cal.IsLeapYear(1402));
}
TEST(PersianCalendarTests, CurrentYear) {
    PersianCalendar cal;
    System::DateTime dt(2024,3,20);
    int y = cal.GetYear(dt);
    EXPECT_GE(y, 1402);
    EXPECT_LE(y, 1404);
}

// ===========================================================================================
// #2409 (SA-14 decision 2) -- CurrentCulture is per-THREAD, with .NET's process-wide fallback
//
// Until #2409 both current-culture members were plain `static`, so a set on one thread changed
// what every other thread read, and a concurrent get/set was an unsynchronised read/write of a
// non-atomic object. The type's own doc-comment already said "the current THREAD's culture".
//
// The repair is .NET's TWO-PROPERTY model, not a bare thread_local: making the members
// thread_local alone would have fixed the race and SILENTLY REMOVED the process-wide setting this
// port accidentally had. .NET's chain is
// `s_currentThreadCulture ?? s_DefaultThreadCurrentCulture ?? s_userDefaultCulture`
// (CultureInfo.cs:358-366), and DefaultThreadCurrentCulture is a real public property (:407-413).
//
// WHY EVERY THREAD-CULTURE SET BELOW HAPPENS ON A SPAWNED THREAD. A first cut set the culture on
// the TEST thread and never restored it, because the setter takes a value and there is no "unset"
// -- .NET's has none either. That leaked into the rest of the binary and made three unrelated
// pre-existing cases fail under mutation, which is a defect in the test rather than evidence about
// the code (#2352's rule). Setting on a thread that then exits leaves nothing behind, and it is
// also the only honest way to observe a per-thread property.
// ===========================================================================================

namespace {

/// Restores both process-wide defaults, so no case can leak one into another.
class CultureDefaultsFixture : public ::testing::Test {
protected:
    void SetUp() override { reset(); }
    void TearDown() override { reset(); }
    static void reset() {
        CultureInfo::setDefaultThreadCurrentCultureProperty(std::nullopt);
        CultureInfo::setDefaultThreadCurrentUICultureProperty(std::nullopt);
    }
    /// Runs @p body on a fresh thread and returns, so any thread culture it sets dies with it.
    template <typename F>
    static void onAFreshThread(F body) {
        std::thread worker(std::move(body));
        worker.join();
    }
};

} // namespace

TEST_F(CultureDefaultsFixture, ASetOnOneThreadIsInvisibleToAnother) {
    std::string setterSaw, otherSaw;
    onAFreshThread([&] {
        CultureInfo::setCurrentCultureProperty(CultureInfo("de-DE"));
        setterSaw = CultureInfo::getCurrentCultureProperty().getNameProperty();
        // A nested fresh thread, started AFTER the set, is the reader.
        onAFreshThread([&] { otherSaw = CultureInfo::getCurrentCultureProperty().getNameProperty(); });
    });

    EXPECT_EQ(setterSaw, "de-DE") << "the setting thread must see its own choice";
    EXPECT_NE(otherSaw, "de-DE")
        << "another thread saw that thread's culture, so the setting is still process-wide";
    EXPECT_EQ(otherSaw, "") << "with no default set, a fresh thread gets the invariant culture";
    // ...and the test thread, which set nothing, is untouched.
    EXPECT_EQ(CultureInfo::getCurrentCultureProperty().getNameProperty(), "");
}

// The half a bare thread_local would have destroyed. This is why the repair adds a property
// rather than only changing storage.
TEST_F(CultureDefaultsFixture, TheProcessWideDefaultIsWhatAThreadFallsBackTo) {
    CultureInfo::setDefaultThreadCurrentCultureProperty(CultureInfo("ja-JP"));

    std::string seenByOther;
    onAFreshThread([&] { seenByOther = CultureInfo::getCurrentCultureProperty().getNameProperty(); });
    EXPECT_EQ(seenByOther, "ja-JP")
        << "a thread with no setting of its own must fall back to the process-wide default";

    // .NET's chain is ??-ordered: a thread's OWN setting wins over the default.
    std::string seenByChooser;
    onAFreshThread([&] {
        CultureInfo::setCurrentCultureProperty(CultureInfo("fr-FR"));
        seenByChooser = CultureInfo::getCurrentCultureProperty().getNameProperty();
    });
    EXPECT_EQ(seenByChooser, "fr-FR");
}

// std::nullopt is .NET's null: "no process-wide default", which falls through to invariant.
TEST_F(CultureDefaultsFixture, TheDefaultRoundTripsAndAbsentMeansNoDefault) {
    EXPECT_FALSE(CultureInfo::getDefaultThreadCurrentCultureProperty().has_value());

    CultureInfo::setDefaultThreadCurrentCultureProperty(CultureInfo("pt-BR"));
    const auto readBack = CultureInfo::getDefaultThreadCurrentCultureProperty();
    ASSERT_TRUE(readBack.has_value());
    EXPECT_EQ(readBack->getNameProperty(), "pt-BR");

    CultureInfo::setDefaultThreadCurrentCultureProperty(std::nullopt);
    EXPECT_FALSE(CultureInfo::getDefaultThreadCurrentCultureProperty().has_value());
    EXPECT_EQ(CultureInfo::getCurrentCultureProperty().getNameProperty(), "")
        << "with the default cleared, a thread that chose nothing is back on invariant";
}

// The UI culture is a separate chain, not an alias. A repair that wired both to one storage would
// pass every case above.
TEST_F(CultureDefaultsFixture, TheUICultureIsItsOwnChain) {
    std::string uiAfterCultureSet;
    onAFreshThread([&] {
        CultureInfo::setCurrentCultureProperty(CultureInfo("nl-NL"));
        EXPECT_EQ(CultureInfo::getCurrentCultureProperty().getNameProperty(), "nl-NL");
        uiAfterCultureSet = CultureInfo::getCurrentUICultureProperty().getNameProperty();
    });
    EXPECT_EQ(uiAfterCultureSet, "") << "setting the culture must not move the UI culture";

    CultureInfo::setDefaultThreadCurrentUICultureProperty(CultureInfo("it-IT"));
    std::string uiSeen, cultureSeen;
    onAFreshThread([&] {
        uiSeen = CultureInfo::getCurrentUICultureProperty().getNameProperty();
        cultureSeen = CultureInfo::getCurrentCultureProperty().getNameProperty();
    });
    EXPECT_EQ(uiSeen, "it-IT");
    EXPECT_EQ(cultureSeen, "") << "the UI default must not become the culture default";
}

// THE REFERENCE MUST SURVIVE A CONCURRENT SWAP. getCurrentCultureProperty() returns
// `const CultureInfo&`, so a fallback that read a shared mutable object and returned a reference
// into it would have moved the race one level down rather than removing it. The reader parks the
// loaded shared_ptr in a thread_local holder for exactly this; here another thread replaces the
// default in a tight loop while this one holds and reads through a reference.
//
// HONEST LIMIT, RECORDED RATHER THAN IMPLIED: dropping the holder is a race WINDOW, so this case
// catches it only when the swap lands inside it. The instrument that catches it reliably is ASan,
// and the ticket records the run.
// THE HELD REFERENCE MUST STAY VALID WHEN THE DEFAULT IS REPLACED. `getCurrentCultureProperty()`
// returns `const CultureInfo&`, so a fallback that read the shared object and returned a reference
// into it would have moved the race one level down instead of removing it: the next store drops
// the last owner and frees the object the caller is still holding. The reader parks the loaded
// `shared_ptr` in a `thread_local` holder for exactly this.
//
// THIS IS DELIBERATELY DETERMINISTIC RATHER THAN A RACE. A first cut ran a churn thread and read
// in a loop, and NEITHER gtest NOR ASan ever caught the mutation that drops the holder -- the
// window is nanoseconds wide and it simply never landed, over 20,000 iterations and under the
// sanitizer. Replacing the default from the SAME thread removes the timing entirely: the store
// drops the last reference at a known point, so the defect is present on every run rather than on
// an unlucky one. A test that catches a defect only sometimes is not evidence (#2352).
TEST_F(CultureDefaultsFixture, AHeldReferenceSurvivesTheDefaultBeingReplaced) {
    CultureInfo::setDefaultThreadCurrentCultureProperty(CultureInfo("es-ES"));

    // A fresh thread, because a thread that has chosen its own culture never reaches the fallback
    // this case is about -- and other suites in this binary do set the test thread's culture.
    std::string readThroughHeld;
    bool nameWasEmpty = false;
    std::thread reader([&] {
        const CultureInfo& held = CultureInfo::getCurrentCultureProperty();
        ASSERT_EQ(held.getNameProperty(), "es-ES");

        // Replace the default. Without the holder this frees the object `held` refers to.
        CultureInfo::setDefaultThreadCurrentCultureProperty(CultureInfo("pt-BR"));

        readThroughHeld = held.getNameProperty();
        nameWasEmpty = held.getNameProperty().empty();
    });
    reader.join();

    EXPECT_FALSE(nameWasEmpty) << "the held reference points at freed storage";
    EXPECT_EQ(readThroughHeld, "es-ES")
        << "the reference must keep naming the culture it was taken from; it may be stale, but it "
           "must not be invalid";
}
