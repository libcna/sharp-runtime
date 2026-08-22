// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2047 -- the mandatory disclosure-and-pins ticket of
// docs/SystemNetNamespaceReviewPlan.md §13 item 7 and §15 completion criterion 2.
//
// This file began as pins for four approval-gated System::Net decisions. The decisions have since
// landed (or, for #2043, are pinned in the named companion file), so these cases now assert the
// resulting contracts rather than describing open defects. Keeping that history here records why
// each boundary has direct regression coverage.
//
//   #2040 (SR-AUD-305 + SR-AUD-306, cause N-E) -- explicit cookie domains are validated against
//         the origin and constructor-supplied path/domain values remain explicit.
//   #2042 (SR-AUD-308, cause N-F) -- storage enforces .NET's total, per-domain, value-size,
//         expiration and oldest-first eviction rules.
//   #2043 (SR-AUD-304's wildcard half, cause N-C) -- lives in
//         DnsLiteralAndDuplicateTests.cpp under #2039.
//   #2044 (SR-AUD-309, cause N-G) -- HtmlEncode performs the selected HTML-safe Unicode escaping
//         while HtmlDecode retains its broader accepted input grammar.
//
// The original pins were inverted as their repairs landed; the individual sections retain the
// mutation evidence and reference rationale.
#include <gtest/gtest.h>
#include <string>
#include "System/Net/Cookie.hpp"
#include "System/Net/CookieCollection.hpp"
#include "System/Net/CookieContainer.hpp"
#include "System/Net/CookieException.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/DateTimeOffset.hpp"
#include "System/Net/WebUtility.hpp"
#include "System/Uri.hpp"
#include "System/detail/ProcessTimeZoneState.hpp"

using System::Net::Cookie;
using System::Net::CookieCollection;
using System::Net::CookieContainer;
using System::Net::WebUtility;
using System::Uri;

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
namespace {

// POSIX exposes one TZ selection for the whole process. Tests use the same lock as the runtime
// readers and the TimeZone component's temporary selectors, so this deterministic non-UTC probe
// cannot observe (or expose to another thread) a half-switched C-library timezone.
class ScopedProcessTimeZone final {
public:
    explicit ScopedProcessTimeZone(const char* zone) {
        std::lock_guard<std::mutex> lock(System::detail::processTimeZoneMutex());
        const char* current = std::getenv("TZ");
        wasSet_ = current != nullptr;
        if (wasSet_) saved_ = current;
        (void)::setenv("TZ", zone, 1);
        ::tzset();
    }

    ~ScopedProcessTimeZone() {
        std::lock_guard<std::mutex> lock(System::detail::processTimeZoneMutex());
        if (wasSet_) (void)::setenv("TZ", saved_.c_str(), 1);
        else         (void)::unsetenv("TZ");
        ::tzset();
    }

    ScopedProcessTimeZone(const ScopedProcessTimeZone&) = delete;
    ScopedProcessTimeZone& operator=(const ScopedProcessTimeZone&) = delete;

private:
    std::string saved_;
    bool wasSet_ = false;
};

} // namespace
#endif

// ===========================================================================
// #2040 -- the cookie origin policy. Approval sentence: plan §14.1.
// ===========================================================================

// #2040 LANDED 2026-08-17. All four pins below are INVERTED, which is what the ticket said
// would happen if the rule were ever settled. It was, against the reference tree:
// Cookie.VerifyAndSetDefaults (Cookie.cs:358-424) validates an explicitly supplied Domain
// against the request URI's host and throws CookieException when it does not domain-match, and
// .NET's constructors assign through the properties (Cookie.cs:108-118), so a caller-supplied
// path or domain is never marked implicit.

TEST(NetGatedBehaviourPinTests, Pin2040_CrossOriginExplicitDomainIsREJECTED) {
    // THE DEFECT, now repaired. Measured before (build-probe/2034_probe1_net_before.log): a
    // cookie handed to the container from origin.invalid carrying Domain=.unrelated.invalid was
    // stored and handed back out for unrelated.invalid.
    CookieContainer container;
    Cookie cookie("session", "isolated");
    cookie.setDomainProperty(".unrelated.invalid");

    EXPECT_THROW(container.Add(Uri("http://origin.invalid/"), cookie),
                 System::Net::CookieException);
    EXPECT_EQ(container.getCountProperty(), 0) << "a rejected cookie must not be stored";
    EXPECT_EQ(container.GetCookieHeader(Uri("http://unrelated.invalid/")), "");
}

TEST(NetGatedBehaviourPinTests, Pin2040_TheRejectionCarriesDotNetsMessage) {
    CookieContainer container;
    Cookie cookie("session", "isolated");
    cookie.setDomainProperty(".unrelated.invalid");
    try {
        container.Add(Uri("http://origin.invalid/"), cookie);
        FAIL() << "expected CookieException";
    } catch (const System::Net::CookieException& e) {
        // SR.net_cookie_attribute is "The '{0}'='{1}' part of the cookie is invalid."
        // (System.Net.Primitives/src/Resources/Strings.resx:93).
        EXPECT_EQ(std::string(e.what()),
                  "The 'Domain'='.unrelated.invalid' part of the cookie is invalid.");
    }
}

TEST(NetGatedBehaviourPinTests, Pin2040_AnExplicitDomainTHATDoesMatchIsStillAccepted) {
    // The repair must not reject the legitimate case it exists to permit: a cookie scoped to a
    // parent domain of its own origin. This is the assertion that would catch a guard that
    // simply rejected every explicit domain.
    CookieContainer container;
    Cookie cookie("session", "shared");
    cookie.setDomainProperty(".example.invalid");
    container.Add(Uri("http://www.example.invalid/"), cookie);

    EXPECT_EQ(container.getCountProperty(), 1);
    EXPECT_EQ(container.GetCookieHeader(Uri("http://api.example.invalid/")), "session=shared");
    EXPECT_EQ(container.GetCookieHeader(Uri("http://www.example.invalid/")), "session=shared");
}

TEST(NetGatedBehaviourPinTests, Pin2040_SingleLabelDomainsAndIpHostsNeedAnExactMatch) {
    // The two compatibility conditions .NET adds on top of RFC 6265 (Cookie.cs:347-349):
    // the domain must itself contain a dot, and the host must not be an IP literal. Without
    // them, Domain=invalid would attach to every *.invalid host and Domain=1.2.3 to 1.2.3.4.
    {
        CookieContainer container;
        Cookie tooBroad("n", "v");
        tooBroad.setDomainProperty("invalid");
        EXPECT_THROW(container.Add(Uri("http://origin.invalid/"), tooBroad),
                     System::Net::CookieException);
    }
    {
        CookieContainer container;
        Cookie ipSuffix("n", "v");
        ipSuffix.setDomainProperty("2.3.4");
        EXPECT_THROW(container.Add(Uri("http://1.2.3.4/"), ipSuffix),
                     System::Net::CookieException);
    }
    {
        // ...and the exact match those two conditions still permit.
        CookieContainer container;
        Cookie exact("n", "v");
        exact.setDomainProperty("1.2.3.4");
        EXPECT_NO_THROW(container.Add(Uri("http://1.2.3.4/"), exact));
        EXPECT_EQ(container.GetCookieHeader(Uri("http://1.2.3.4/")), "n=v");
    }
}

TEST(NetGatedBehaviourPinTests, Pin2040_ConstructorSuppliedPathIsEXPLICIT) {
    // The constructor and the setter now agree about the same value. That flag is the INPUT to
    // the domain rule above, which is why SR-AUD-305 and SR-AUD-306 were one decision.
    const Cookie viaConstructor("n", "v", "/explicit");
    EXPECT_EQ(viaConstructor.getPathProperty(), "/explicit");
    EXPECT_FALSE(viaConstructor.getPathImplicitProperty());

    Cookie viaSetter("n", "v");
    viaSetter.setPathProperty("/explicit");
    EXPECT_EQ(viaSetter.getPathProperty(), "/explicit");
    EXPECT_FALSE(viaSetter.getPathImplicitProperty());
}

TEST(NetGatedBehaviourPinTests, Pin2040_ConstructorSuppliedDomainIsEXPLICIT) {
    const Cookie viaConstructor("n", "v", "/p", ".d.invalid");
    EXPECT_EQ(viaConstructor.getDomainProperty(), ".d.invalid");
    EXPECT_FALSE(viaConstructor.getDomainImplicitProperty());

    Cookie viaSetter("n", "v");
    viaSetter.setDomainProperty(".d.invalid");
    EXPECT_EQ(viaSetter.getDomainProperty(), ".d.invalid");
    EXPECT_FALSE(viaSetter.getDomainImplicitProperty());
}

TEST(NetGatedBehaviourPinTests, Pin2040_ContainerKEEPSConstructorSuppliedPathAndDomain) {
    // The consequence of the two cases above, and the user-visible half of the repair: because
    // the flags are now cleared, Add() no longer replaces the caller's own values with the
    // request URI's. The domain is chosen to domain-match the origin so the new validation
    // accepts it -- ".d.invalid" from the original pin would now be rejected, which is the point.
    CookieContainer container;
    container.Add(Uri("http://sub.origin.invalid/some/where"),
                  Cookie("n", "v", "/explicit", ".origin.invalid"));

    const CookieCollection stored = container.GetCookies(Uri("http://sub.origin.invalid/explicit"));
    ASSERT_EQ(stored.getCountProperty(), 1);
    EXPECT_EQ(stored[0].getPathProperty(), "/explicit");
    EXPECT_EQ(stored[0].getDomainProperty(), ".origin.invalid");
}

TEST(NetGatedBehaviourPinTests, Pin2040_AnImplicitDomainStaysImplicitAfterBeingDefaulted) {
    // .NET's SetDomainAndKey writes the domain and leaves m_domain_implicit alone
    // (Cookie.cs:310-314): the flag records where the value CAME FROM, so defaulting it from the
    // request URI must not make the cookie claim the caller chose it.
    CookieContainer container;
    container.Add(Uri("http://origin.invalid/a/b"), Cookie("n", "v"));

    const CookieCollection stored = container.GetCookies(Uri("http://origin.invalid/a/b"));
    ASSERT_EQ(stored.getCountProperty(), 1);
    EXPECT_EQ(stored[0].getDomainProperty(), "origin.invalid");
    EXPECT_TRUE(stored[0].getDomainImplicitProperty());
    EXPECT_TRUE(stored[0].getPathImplicitProperty());
}

// ===========================================================================
// #2042 RESOLVED -- the storage bound, and the numbers were never anybody's to choose.
//
// The ticket was gated twice: "every bound is a number somebody must choose" AND ".NET's exact
// default capacities cannot be established here". The reference establishes them, which
// dissolves both halves at once -- DefaultCookieLimit = 300, DefaultPerDomainCookieLimit = 20,
// DefaultCookieLengthLimit = 4096 (CookieContainer.cs:69-71).
// ===========================================================================

TEST(NetGatedBehaviourPinTests, Fix2042_TheDefaultsAreDotNetsOwnConstants) {
    CookieContainer container;
    EXPECT_EQ(container.getCapacityProperty(), 300);
    EXPECT_EQ(container.getPerDomainCapacityProperty(), 20);
    EXPECT_EQ(container.getMaxCookieSizeProperty(), 4096);
    EXPECT_EQ(CookieContainer::DefaultCookieLimit, 300);
    EXPECT_EQ(CookieContainer::DefaultPerDomainCookieLimit, 20);
    EXPECT_EQ(CookieContainer::DefaultCookieLengthLimit, 4096);
}

TEST(NetGatedBehaviourPinTests, Fix2042_StorageIsBoundedPerDomainAndInTotal) {
    // INVERTED. 2,000 cookies from one origin used to be all retained; the per-domain limit now
    // binds first, and it binds at min(perDomain, capacity) - 1 = 19 rather than at 20, because
    // aging must FREE a slot for the cookie being added (CookieContainer.cs:441).
    CookieContainer container;
    const Uri origin("http://origin.invalid/");
    for (int i = 0; i < 2000; ++i)
        container.Add(origin, Cookie("n" + std::to_string(i), "v" + std::to_string(i)));
    EXPECT_EQ(container.getCountProperty(), 20);

    // Oldest-first: the survivors are the LAST twenty added.
    const std::string header = container.GetCookieHeader(origin);
    EXPECT_NE(header.find("n1999=v1999"), std::string::npos) << header;
    EXPECT_EQ(header.find("n0=v0"), std::string::npos) << header;
}

TEST(NetGatedBehaviourPinTests, Fix2042_TheTotalCapacityBindsAcrossDomains) {
    // The per-domain limit alone would allow 20 per domain without end, so the total is a
    // separate bound and needs its own case. 40 domains x 20 would be 800; the total is 300.
    CookieContainer container;
    for (int d = 0; d < 40; ++d) {
        const Uri origin("http://d" + std::to_string(d) + ".invalid/");
        for (int i = 0; i < 20; ++i)
            container.Add(origin, Cookie("n" + std::to_string(i), "v"));
    }
    EXPECT_LE(container.getCountProperty(), 300);
    EXPECT_GT(container.getCountProperty(), 0);
}

TEST(NetGatedBehaviourPinTests, Fix2042_ExpiredCookiesArePurgedNotRetained) {
    // INVERTED. The finding did not name this half -- #2047's pin found it: there was no expiry
    // cleanup at all, so an expired cookie was retained and only hidden from emission.
    //
    // An expired cookie is an explicit REMOVAL command in .NET (CookieContainer.cs:263-275), so
    // adding one stores nothing.
    CookieContainer container;
    const Uri origin("http://origin.invalid/");
    Cookie expired("gone", "v");
    expired.setExpiresProperty(System::DateTime(2000, 1, 1));
    container.Add(origin, expired);
    container.Add(origin, Cookie("kept", "v"));

    EXPECT_EQ(container.getCountProperty(), 1);
    EXPECT_EQ(container.GetCookieHeader(origin), "kept=v");
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
TEST(NetGatedBehaviourPinTests, KindRipple_UtcExpiryIsComparedOnTheUtcTimeline) {
    // UTC-14 is POSIX spelling for UTC+14. Before #2418, Cookie compared an Expires value's
    // raw ticks with DateTime::Now. Once Now correctly became a local wall clock this made a
    // UTC expiry thirty minutes in the future look almost fourteen hours old.
    const ScopedProcessTimeZone zone("UTC-14");
    const System::DateTime utcNow =
        System::DateTimeOffset::getUtcNowProperty().getUtcDateTimeProperty();

    Cookie future("future", "v");
    future.setExpiresProperty(utcNow.AddMinutes(30));
    EXPECT_FALSE(future.getExpiredProperty());

    Cookie past("past", "v");
    past.setExpiresProperty(utcNow.AddMinutes(-30));
    EXPECT_TRUE(past.getExpiredProperty());
}
#endif

TEST(NetGatedBehaviourPinTests, KindRipple_MaxAgeCreatesAUtcExpiry) {
    // Max-Age is an interval from the current instant, independent of the local wall clock.
    // Pin both that scale and the Kind: losing either lets a later expiry comparison silently
    // reinterpret the stored value as local time.
    CookieContainer container;
    const Uri origin("http://origin.invalid/");
    const System::DateTime before =
        System::DateTimeOffset::getUtcNowProperty().getUtcDateTimeProperty();
    container.SetCookies(origin, "maxage=v; Max-Age=3600; Path=/");
    const System::DateTime after =
        System::DateTimeOffset::getUtcNowProperty().getUtcDateTimeProperty();

    const CookieCollection stored = container.GetCookies(origin);
    ASSERT_EQ(stored.getCountProperty(), 1);
    const System::DateTime expires = stored[0].getExpiresProperty();
    EXPECT_EQ(expires.getKindProperty(), System::DateTimeKind::Utc);
    EXPECT_GE(expires.getTicksProperty(), before.AddSeconds(3600).getTicksProperty());
    EXPECT_LE(expires.getTicksProperty(), after.AddSeconds(3600).getTicksProperty());
}

TEST(NetGatedBehaviourPinTests, KindRipple_ExpiresParsesAllThreeHttpDateFormsAsUtc) {
    const Uri origin("http://origin.invalid/");
    const auto verifyFuture = [&origin](const std::string& name, const std::string& expires) {
        CookieContainer container;
        container.SetCookies(origin, name + "=v; Expires=" + expires + "; Path=/");
        const CookieCollection stored = container.GetCookies(origin);
        ASSERT_EQ(stored.getCountProperty(), 1) << expires;
        EXPECT_EQ(stored[0].getExpiresProperty().getKindProperty(),
                  System::DateTimeKind::Utc) << expires;
        EXPECT_FALSE(stored[0].getExpiredProperty()) << expires;
    };

    verifyFuture("imf", "Sat, 06 Nov 2094 08:49:37 GMT");
    verifyFuture("rfc850", "Tuesday, 06-Nov-29 08:49:37 GMT");
    verifyFuture("asctime", "Sat Nov  6 08:49:37 2094");

    CookieContainer past;
    past.SetCookies(origin, "gone=v; Expires=Wed, 21 Oct 2015 07:28:00 GMT; Path=/");
    EXPECT_EQ(past.getCountProperty(), 0);
}

TEST(NetGatedBehaviourPinTests, KindRipple_CookieDateHasItsOwnBoundedInvariantGrammar) {
    const Uri origin("http://origin.invalid/");

    CookieContainer mixedCase;
    mixedCase.SetCookies(
        origin, "mixed=v; Expires=sAt, 06 nOv 2094 08:49:37 GMT; Path=/");
    EXPECT_EQ(mixedCase.getCountProperty(), 1);

    CookieContainer noWeekday;
    noWeekday.SetCookies(
        origin, "cookie=v; Expires=06-Nov-2094 08:49:37 GMT; Path=/");
    EXPECT_EQ(noWeekday.getCountProperty(), 1)
        << "Cookie ParseCookieDate accepts this common form; HTTP header dates do not";

    // The shared invariant two-digit-year policy ends at 2049. Both inputs use the matching
    // weekday, so storage versus expiry distinguishes the window rather than weekday validation.
    CookieContainer window;
    window.SetCookies(origin, "future=v; Expires=Saturday, 06-Nov-49 08:49:37 GMT; Path=/");
    EXPECT_EQ(window.getCountProperty(), 1);
    window.SetCookies(origin, "past=v; Expires=Monday, 06-Nov-50 08:49:37 GMT; Path=/");
    EXPECT_EQ(window.getCountProperty(), 1) << "the 1950 cookie is discarded";

    CookieContainer invalid;
    EXPECT_THROW(invalid.SetCookies(
                     origin, "bad=v; Expires=Mon, 06 Nov 2094 08:49:37 GMT; Path=/"),
                 System::Net::CookieException)
        << "a real weekday name must still agree with the calendar date";
    EXPECT_THROW(invalid.SetCookies(
                     origin, "bad=v; Expires=Sat, +06 Nov 2094 08:49:37 GMT; Path=/"),
                 System::Net::CookieException)
        << "numeric format tokens do not admit signs";
}

TEST(NetGatedBehaviourPinTests, KindRipple_FirstExpiresOrMaxAgeAttributeWins) {
    const Uri origin("http://origin.invalid/");
    constexpr const char* future = "Sat, 06 Nov 2094 08:49:37 GMT";
    constexpr const char* past = "Wed, 21 Oct 2015 07:28:00 GMT";

    CookieContainer maxAgeFirst;
    maxAgeFirst.SetCookies(
        origin, std::string("kept=v; Max-Age=3600; Expires=") + past + "; Path=/");
    EXPECT_EQ(maxAgeFirst.getCountProperty(), 1);

    CookieContainer expiresFirst;
    expiresFirst.SetCookies(
        origin, std::string("kept=v; Expires=") + future + "; Max-Age=0; Path=/");
    EXPECT_EQ(expiresFirst.getCountProperty(), 1);

    CookieContainer expiredFirst;
    expiredFirst.SetCookies(
        origin, std::string("gone=v; Expires=") + past + "; Max-Age=3600; Path=/");
    EXPECT_EQ(expiredFirst.getCountProperty(), 0);

    CookieContainer zeroFirst;
    zeroFirst.SetCookies(
        origin, std::string("gone=v; Max-Age=0; Expires=") + future + "; Path=/");
    EXPECT_EQ(zeroFirst.getCountProperty(), 0);
}

TEST(NetGatedBehaviourPinTests, KindRipple_MalformedMaxAgeIsFullyRejected) {
    const Uri origin("http://origin.invalid/");
    CookieContainer container;

    EXPECT_THROW(container.SetCookies(origin, "bad=v; Max-Age=3600junk; Path=/"),
                 System::Net::CookieException);
    EXPECT_THROW(container.SetCookies(origin, "huge=v; Max-Age=999999999999999; Path=/"),
                 System::Net::CookieException);
    EXPECT_THROW(container.SetCookies(origin, "bad=v; Expires=not-an-http-date; Path=/"),
                 System::Net::CookieException);
    EXPECT_EQ(container.getCountProperty(), 0);
}

TEST(NetGatedBehaviourPinTests, Fix2042_MaxCookieSizeReportsRatherThanEvicting) {
    // The one limit that throws. It bounds the VALUE alone -- not the name, not the whole
    // cookie -- and the message is .NET's (Strings.resx:87-89).
    CookieContainer container;
    const Uri origin("http://origin.invalid/");
    EXPECT_NO_THROW(container.Add(origin, Cookie("ok", std::string(4096, 'x'))));
    try {
        container.Add(origin, Cookie("big", std::string(4097, 'x')));
        ADD_FAILURE() << "an oversized cookie value was accepted";
    } catch (const System::Net::CookieException& e) {
        const std::string what = e.what();
        EXPECT_NE(what.find("This exceeds the configured maximum size, which is '4096'."),
                  std::string::npos) << what;
    }
    // A long NAME is not a long value: the limit is on Value alone, so this must be accepted.
    EXPECT_NO_THROW(container.Add(origin, Cookie(std::string(5000, 'n'), "v")));
}

TEST(NetGatedBehaviourPinTests, Fix2042_ReplacingACookieConsumesNoSlot) {
    // .NET's InternalAdd adds zero to m_count for an overwrite, so re-adding the same identity
    // must not evict anything -- the row that fails if the capacity check runs before the
    // identity replacement.
    //
    // THE DOMAIN MUST BE AT ITS LIMIT for this to discriminate: below the limit the capacity
    // check does nothing and the two orders agree, which is how a first version of this case
    // let that mutation through. Twenty-one adds leave the domain at exactly 20 (the 21st ages
    // it to 19 and then inserts), which is the state where the order is observable.
    CookieContainer container;
    const Uri origin("http://origin.invalid/");
    for (int i = 0; i < 21; ++i) container.Add(origin, Cookie("n" + std::to_string(i), "v"));
    ASSERT_EQ(container.getCountProperty(), 20);
    const std::string before = container.GetCookieHeader(origin);

    // Re-adding an identity already present must refresh it in place: no eviction, no growth.
    for (int i = 0; i < 50; ++i) container.Add(origin, Cookie("n20", "refreshed"));
    EXPECT_EQ(container.getCountProperty(), 20);
    EXPECT_NE(container.GetCookieHeader(origin).find("n20=refreshed"), std::string::npos);
    // ...and nothing else was dropped to make room for it. The header is bound to a NAMED
    // string first: taking begin() and end() from two separate GetCookieHeader() calls would be
    // iterators into two different temporaries, which is undefined and hung this test once.
    const std::string after = container.GetCookieHeader(origin);
    EXPECT_EQ(std::count(before.begin(), before.end(), ';'),
              std::count(after.begin(), after.end(), ';'));
}

TEST(NetGatedBehaviourPinTests, Fix2042_AgingPurgesCookiesThatExpiredWhileStored) {
    // The purge inside aging is NOT reachable through Add, because a cookie that is already
    // expired when added is an explicit removal and is never stored. It becomes reachable the
    // only way it can: a cookie stored while valid, whose Expires then passes.
    //
    // The wait is deterministic rather than a race -- Expires is a fixed instant and sleeping
    // past it can only overshoot -- which is why this is a legitimate test where the SIGSTOP
    // shape in #2031 was not.
    // THE EXPIRED COOKIE MUST NOT BE THE OLDEST, or the case cannot discriminate: dropping it
    // would be what plain oldest-first eviction does anyway. A first version put it first and
    // the mutation went through. Here eleven live cookies precede it, so:
    //   with the purge    -> "shortlived" goes and n0 survives;
    //   without the purge -> n0 is evicted as the oldest and "shortlived" stays in storage.
    CookieContainer container;
    const Uri origin("http://origin.invalid/");
    for (int i = 0; i < 11; ++i) container.Add(origin, Cookie("n" + std::to_string(i), "v"));

    Cookie soon("shortlived", "v");
    soon.setExpiresProperty(System::DateTime::getNowProperty().AddMilliseconds(300));
    container.Add(origin, soon);
    ASSERT_EQ(container.getCountProperty(), 12);

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Fill the domain to its limit so aging runs.
    for (int i = 11; i < 20; ++i) container.Add(origin, Cookie("n" + std::to_string(i), "v"));
    const std::string header = container.GetCookieHeader(origin);
    EXPECT_NE(header.find("n0=v"), std::string::npos)
        << "the oldest LIVE cookie was evicted even though an expired one was available: "
        << header;
    EXPECT_EQ(header.find("shortlived"), std::string::npos) << header;
}

TEST(NetGatedBehaviourPinTests, Fix2042_TheLimitSettersValidateAsDotNetsDo) {
    CookieContainer container;
    // Capacity's lower bound is PerDomainCapacity, not zero (CookieContainer.cs:117-119).
    EXPECT_THROW(container.setCapacityProperty(0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(container.setCapacityProperty(-1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(container.setCapacityProperty(19), System::ArgumentOutOfRangeException);
    EXPECT_NO_THROW(container.setCapacityProperty(20));

    // PerDomainCapacity may not exceed Capacity (:163-172).
    EXPECT_THROW(container.setPerDomainCapacityProperty(21), System::ArgumentOutOfRangeException);
    EXPECT_THROW(container.setPerDomainCapacityProperty(0), System::ArgumentOutOfRangeException);
    EXPECT_NO_THROW(container.setPerDomainCapacityProperty(5));

    EXPECT_THROW(container.setMaxCookieSizeProperty(0), System::ArgumentOutOfRangeException);
    EXPECT_NO_THROW(container.setMaxCookieSizeProperty(1));

    // The three-argument constructor sets per-domain first, so a pair that is only valid in that
    // order is accepted -- .NET's own ordering (CookieContainer.cs:88-106).
    EXPECT_NO_THROW((CookieContainer{10, 10, 100}));
    EXPECT_THROW((CookieContainer{5, 10, 100}), System::ArgumentOutOfRangeException);
}

TEST(NetGatedBehaviourPinTests, Fix2042_ShrinkingTheCapacityEvictsImmediately) {
    // .NET ages as soon as the smaller value is set (:121-124) rather than waiting for the next
    // Add, or Count could exceed a limit the caller has already established.
    CookieContainer container;
    for (int d = 0; d < 5; ++d) {
        const Uri origin("http://d" + std::to_string(d) + ".invalid/");
        for (int i = 0; i < 10; ++i) container.Add(origin, Cookie("n" + std::to_string(i), "v"));
    }
    ASSERT_EQ(container.getCountProperty(), 50);
    container.setCapacityProperty(30);
    EXPECT_LE(container.getCountProperty(), 30);
}

// ===========================================================================
// #2044 RESOLVED -- the HTML escaping policy, and the coupling premise was wrong.
//
// The deferral said this was "coupled to System::Text's #2019: two HTML encoders in one
// repository must not diverge". The reference answers that directly: .NET HAS EXACTLY TWO HTML
// ENCODERS, WITH EXACTLY TWO DIFFERENT ESCAPE SETS, DELIBERATELY.
//
//   * System.Text.Encodings.Web.HtmlEncoder -- an ALLOW-LIST, Basic Latin only, everything
//     above escaped as &#xHH; UPPERCASE HEX. A defence-in-depth encoder. That is #2019, landed.
//   * System.Net.WebUtility.HtmlEncode -- the five specials, plus 160..255 and supplementary
//     scalars as &#NNN; DECIMAL, and everything else passed through (WebUtility.cs:78-140).
//
// So each matches its own counterpart, and a repository-wide "consistency" would match neither.
// ===========================================================================

TEST(NetGatedBehaviourPinTests, Pin2044_HtmlEncodeEscapesExactlyFiveAsciiCharacters) {
    EXPECT_EQ(WebUtility::HtmlEncode("<a href=\"x\">&'"), "&lt;a href=&quot;x&quot;&gt;&amp;&#39;");
    // Every other ASCII character passes through untouched, including ones .NET's encoder
    // leaves alone too -- pinned so the "exactly five" claim is a measurement, not a summary.
    EXPECT_EQ(WebUtility::HtmlEncode("azAZ09 -_.!*()~`@#$%^+=[]{}|\\:;,/?"),
              "azAZ09 -_.!*()~`@#$%^+=[]{}|\\:;,/?");
}

TEST(NetGatedBehaviourPinTests, Fix2044_TheLatin1SupplementAndSupplementaryScalarsAreEncoded) {
    // WebUtility.cs:110 -- "The seemingly arbitrary 160 comes from RFC" -- and :113-124 for the
    // surrogate pair. DECIMAL, not hex: that is WebUtility's form, where HtmlEncoder's is
    // &#xHH; uppercase hex.
    EXPECT_EQ(WebUtility::HtmlEncode("caf\xC3\xA9"), "caf&#233;");        // U+00E9
    EXPECT_EQ(WebUtility::HtmlEncode("\xC2\xA0"), "&#160;");             // U+00A0, the boundary
    EXPECT_EQ(WebUtility::HtmlEncode("\xC3\xBF"), "&#255;");             // U+00FF, the other end
    EXPECT_EQ(WebUtility::HtmlEncode("\xF0\x9F\x98\x80"), "&#128512;")  // U+1F600
        << "one reference for the whole scalar, not one per surrogate half";
}

TEST(NetGatedBehaviourPinTests, Fix2044_EverythingOutsideThatWindowStillPassesThrough) {
    // NOT an oversight in .NET, and pinned so it is not "tidied" later: 128..159 and every BMP
    // scalar above 255 pass through unchanged. They need no escaping in HTML, and escaping them
    // would change nothing but the length.
    EXPECT_EQ(WebUtility::HtmlEncode("\xE2\x82\xAC"), "\xE2\x82\xAC");   // U+20AC, above 255
    EXPECT_EQ(WebUtility::HtmlEncode("\xC2\x85"), "\xC2\x85");           // U+0085, below 160
    EXPECT_EQ(WebUtility::HtmlEncode("\xC2\x9F"), "\xC2\x9F");           // U+009F, the boundary
    EXPECT_EQ(WebUtility::HtmlEncode("\xE4\xB8\xAD"), "\xE4\xB8\xAD");   // U+4E2D
}

TEST(NetGatedBehaviourPinTests, Fix2044_ThisEncoderDIFFERSFromHtmlEncoderAndThatIsDotNets) {
    // THE PREMISE CORRECTION, pinned. The deferral held that "two HTML encoders in one
    // repository must not be given two different escape sets". .NET has exactly two, with
    // exactly two different sets, deliberately -- so each must match its own counterpart.
    //
    // This half asserts WHAT THIS ENCODER DOES. The other half lives where it belongs, in
    // SharpRuntimeTests_Text: HtmlEncoderRangeTests.Fix2019_TheDefaultEncodersEscape-
    // OutsideBasicLatin pins that HtmlEncoder emits "&#xE9;" and "&#x20AC;" for these same two
    // scalars. Deliberately NOT asserted here by including HtmlEncoder: that would need a
    // test-only component edge from Net to Text, which is a real change to the module graph
    // for a comparison two independent pins make just as well.
    EXPECT_EQ(WebUtility::HtmlEncode("\xC3\xA9"), "&#233;")
        << "DECIMAL here; HtmlEncoder emits &#xE9; -- uppercase hex";
    EXPECT_EQ(WebUtility::HtmlEncode("\xE2\x82\xAC"), "\xE2\x82\xAC")
        << "passed through here; HtmlEncoder emits &#x20AC;";
}

TEST(NetGatedBehaviourPinTests, Fix2044_EncodeThenDecodeIsStillTheIdentity) {
    // The property that matters to a caller, and the one the widening could have broken.
    for (const char* text : {"caf\xC3\xA9", "<a href=\"x\">&'", "\xF0\x9F\x98\x80",
                             "\xC2\xA0", "\xE2\x82\xAC", "plain ascii"}) {
        SCOPED_TRACE(text);
        EXPECT_EQ(WebUtility::HtmlDecode(WebUtility::HtmlEncode(text)), text);
    }
}

TEST(NetGatedBehaviourPinTests, Pin2044_HtmlDecodeUnderstandsMoreThanTheEncoderProduces) {
    // Plan §4.4's premise correction, pinned: SR-AUD-309 describes the ENCODE direction only.
    // The decoder already handles decimal and hexadecimal character references and named
    // entities the encoder never emits -- so the finding is an ASYMMETRY, and a repair that
    // widens only the encoder still has to say what the decoder accepts.
    const std::string copyright = "\xC2\xA9";                   // U+00A9
    EXPECT_EQ(WebUtility::HtmlDecode("&copy;"), copyright);
    EXPECT_EQ(WebUtility::HtmlDecode("&#169;"), copyright);
    EXPECT_EQ(WebUtility::HtmlDecode("&#xA9;"), copyright);
    EXPECT_EQ(WebUtility::HtmlDecode("&nbsp;"), std::string("\xC2\xA0"));
    EXPECT_EQ(WebUtility::HtmlDecode("&reg;"), std::string("\xC2\xAE"));
    EXPECT_EQ(WebUtility::HtmlDecode("&trade;"), std::string("\xE2\x84\xA2"));
    // ...and an entity it does NOT know is left literal rather than dropped.
    EXPECT_EQ(WebUtility::HtmlDecode("&hearts;"), "&hearts;");
    EXPECT_EQ(WebUtility::HtmlDecode("&#zz;"), "&#zz;");
}

TEST(NetGatedBehaviourPinTests, Pin2044_EncodeThenDecodeRoundTripsTheFiveEscapedCharacters) {
    const std::string original = "<a href=\"x\">&'";
    EXPECT_EQ(WebUtility::HtmlDecode(WebUtility::HtmlEncode(original)), original);
}
