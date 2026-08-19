// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2199 / SR-AUD-336.
//
// #2199 implemented XObject's Changed/Changing notification, which had been INERT: the four
// accessors took a handler and discarded it, and no mutation raised anything. Two gates had to
// open, and the second changed the public signature:
//
//   * add_Changed / add_Changing now RETURN an XObjectChangeRegistration token;
//   * remove_Changed / remove_Changing now TAKE that token, not a handler, and return bool.
//
// WHY THE SIGNATURE HAD TO CHANGE. .NET removes by passing the delegate back (`obj.Changed -=
// handler`), which works because a C# delegate is equality-comparable. XObjectChangeEventHandler
// is a std::function, and std::function has NO operator== against another std::function -- so a
// handler-taking remove_* cannot identify which registration a caller means. That is not a cost
// question; it is not implementable as declared, at any layout cost. The token was chosen on
// 2026-08-19 so the divergence is visible IN THE TYPE rather than hidden in the behaviour. Two
// alternatives were declined: removing ALL registrations (silently drops a third party's
// subscription) and throwing from remove_* (a subscriber could never unsubscribe).
//
// MIGRATION. Keep the token that add_* returns and hand it back:
//
//     auto token = element.add_Changed(handler);   // was: element.add_Changed(handler);
//     ...
//     element.remove_Changed(token);               // was: element.remove_Changed(handler);
//
// A caller who never unsubscribed needs only to not discard the return value -- add_* is
// [[nodiscard]], deliberately, because a discarded token is a registration that can never be
// removed.
//
// Records: docs/Migration-XObjectChangeNotification.md,
// docs/NegativeConsumerFixtureValidation.md, docs/StandingApprovals.md SA-13.
//
// NEGATIVE-FIXTURE: component=Xml.Linq
#include <memory>
#include <string>

#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XName.hpp"
#include "System/Xml/Linq/XObject.hpp"
#include "System/Xml/Linq/XObjectChangeEventArgs.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Xml::Linq::XElement;
using System::Xml::Linq::XName;
using System::Xml::Linq::XObjectChangeEventArgs;
using System::Xml::Linq::XObjectChangeEventHandler;
using System::Xml::Linq::XObjectChangeRegistration;

namespace {

    XObjectChangeEventHandler makeHandler() {
        return [](void*, const XObjectChangeEventArgs&) {};
    }

    void removeByHandler() {
        XElement e(XName("e"));
        const XObjectChangeEventHandler handler = makeHandler();
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
        // NEGATIVE(remove-changed-by-handler): no matching function for call to
        //     | cannot convert
        //     | no known conversion
        (void)e.add_Changed(handler);
        e.remove_Changed(handler);
#else
        const XObjectChangeRegistration token = e.add_Changed(handler);
        (void)e.remove_Changed(token);
#endif
    }

    void removeChangingByHandler() {
        XElement e(XName("e"));
        const XObjectChangeEventHandler handler = makeHandler();
#if SHARP_RUNTIME_NEGATIVE_SITE == 2
        // NEGATIVE(remove-changing-by-handler): no matching function for call to
        //     | cannot convert
        //     | no known conversion
        (void)e.add_Changing(handler);
        e.remove_Changing(handler);
#else
        const XObjectChangeRegistration token = e.add_Changing(handler);
        (void)e.remove_Changing(token);
#endif
    }

    void discardTheToken() {
        XElement e(XName("e"));
#if SHARP_RUNTIME_NEGATIVE_SITE == 3
        // THE SPELLING MOST LIKELY TO SURVIVE A CARELESS MIGRATION, and the reason add_* is
        // [[nodiscard]]: the old accessor returned void, so a caller who simply kept the old line
        // would silently create a registration that can NEVER be removed. It is a warning rather
        // than an error by nature, which is why the fixture is compiled with -Werror.
        // NEGATIVE(discard-registration-token): ignoring return value
        //     | nodiscard
        e.add_Changed(makeHandler());
#else
        (void)e.add_Changed(makeHandler());
#endif
    }

    void assignTokenFromVoid() {
        XElement e(XName("e"));
#if SHARP_RUNTIME_NEGATIVE_SITE == 4
        // The inverse mistake: assuming the OLD void return. Caught because a token is not void.
        // NEGATIVE(token-is-not-void): declared void
        //     | void value not ignored
        //     | cannot convert
        void result = e.add_Changing(makeHandler());
        (void)result;
#else
        const XObjectChangeRegistration result = e.add_Changing(makeHandler());
        (void)result;
#endif
    }

    void mintATokenDirectly() {
        XElement e(XName("e"));
#if SHARP_RUNTIME_NEGATIVE_SITE == 5
        // A token must come from add_*. The id-taking constructor is private precisely so a
        // caller cannot fabricate one that happens to match somebody else's registration.
        // NEGATIVE(token-cannot-be-minted): is private within this context
        //     | no matching function for call to
        const XObjectChangeRegistration forged(1);
        (void)e.remove_Changed(forged);
#else
        const XObjectChangeRegistration empty;
        (void)e.remove_Changed(empty);
#endif
    }

} // namespace

int main() {
    removeByHandler();
    removeChangingByHandler();
    discardTheToken();
    assignTokenFromVoid();
    mintATokenDirectly();
    return 0;
}
