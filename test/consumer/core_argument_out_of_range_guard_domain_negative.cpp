// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2254 (finding SR-AUD-091).
//
// The nine System::ArgumentOutOfRangeException::ThrowIf* guards used to format every operand
// with a bare std::to_string(value). That imposed an UNDECLARED formatting requirement on top of
// each guard's declared comparison contract: measured over 22 candidate types x 9 guards, 99 of
// 198 (type, guard) pairs failed to compile, and every one of them failed inside
// <bits/basic_string.h> with nine standard-library candidates rather than at any stated
// sharp-runtime constraint. #2254 replaced the bare call with one ordered if-constexpr formatter
// and added a static_assert per guard for the comparison operator that guard evaluates, so the
// whole compile-time domain is now declared AND diagnosed.
//
// A widened domain still has a boundary, and THIS FILE IS THE PROOF THAT THE BOUNDARY IS REAL.
// A static_assert placed in a body that is never instantiated silently enforces nothing, and only
// a per-site compile can tell the difference -- the #2213 precedent. Every
// `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by the compiler; with no
// site selected the file compiles cleanly. scripts/check_negative_consumer_fixtures.py compiles
// each site on its own and asserts the rejection.
//
// The positive half -- that the widened shapes really are accepted from outside the library -- is
// in test/consumer/core_base.cpp, which the selective-components matrix builds AND runs. The
// measured before/after matrix is docs/CoreArgumentOutOfRangeGuardDomainPlan.md.
//
// Migration for a caller a site rejects:
//   * a raw pointer operand -- compare what it points AT, or pass std::string_view. Pointers are
//     excluded deliberately, not by omission: std::string_view from a null pointer is undefined
//     behaviour, and ThrowIfEqual over two const char* compares addresses rather than text, so
//     ThrowIfEqual("a", "b", "p") would silently have compared two string-literal addresses.
//   * a comparison-only type with no rendering -- declare one free `to_string` beside the type.
//     That is the documented extension point, and it is what replaces an operator<< branch:
//     supporting operator<< would force <sstream> into a header 404 translation units depend on,
//     measured at +2,819 preprocessed lines each, where this route costs the header nothing.
//   * a type missing the operator its guard evaluates -- there is no migration and that is the
//     point. ThrowIfLessThan cannot order a type that has no ordering.
//
// The `#else` branches carry the accepted spelling for each site. Those are positive claims and
// are additionally pinned at run time by ArgumentOutOfRangeGuardDomainTests in Core_Base.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <string>
#include <string_view>

#include "System/ArgumentOutOfRangeException.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::ArgumentOutOfRangeException;

namespace {

// Fully ordered, equality-comparable, and deliberately UNRENDERABLE: no std::to_string overload,
// no ToString() member, no conversion to std::string_view or std::string, no ADL to_string.
// This is finding SR-AUD-091's own reproducer type.
struct OrderedOnly {
    int v;
    friend bool operator==(OrderedOnly a, OrderedOnly b) { return a.v == b.v; }
    friend bool operator<(OrderedOnly a, OrderedOnly b) { return a.v < b.v; }
    friend bool operator>(OrderedOnly a, OrderedOnly b) { return a.v > b.v; }
};

// The same shape, opted in through the documented ADL extension point.
struct OrderedRenderable {
    int v;
    friend bool operator==(OrderedRenderable a, OrderedRenderable b) { return a.v == b.v; }
    friend bool operator<(OrderedRenderable a, OrderedRenderable b) { return a.v < b.v; }
    friend bool operator>(OrderedRenderable a, OrderedRenderable b) { return a.v > b.v; }
};
std::string to_string(OrderedRenderable x) { return std::to_string(x.v); }

// Renderable, but with NO ordering at all. Only the comparison assertion can fire for it, which
// is what keeps each site's verdict attributable to exactly one claim.
struct EqualityOnly {
    int v;
    friend bool operator==(EqualityOnly a, EqualityOnly b) { return a.v == b.v; }
};
std::string to_string(EqualityOnly x) { return std::to_string(x.v); }

// Renderable, with operator!= but NO operator==.
struct InequalityOnly {
    int v;
    friend bool operator!=(InequalityOnly a, InequalityOnly b) { return a.v != b.v; }
};
std::string to_string(InequalityOnly x) { return std::to_string(x.v); }

// Renderable and equality-comparable, but NOT default-constructible, so `value == T{}` -- the
// expression ThrowIfZero evaluates -- is ill-formed. That requirement is undeclared in the port
// before #2254 and is named by the guard's assertion now.
struct NoDefaultCtor {
    int v;
    explicit NoDefaultCtor(int x) : v(x) {}
    friend bool operator==(NoDefaultCtor a, NoDefaultCtor b) { return a.v == b.v; }
};
std::string to_string(NoDefaultCtor x) { return std::to_string(x.v); }

}  // namespace

int main() {
    int first = 1;
    int second = 2;

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(guard-raw-pointer-operand): cannot render this type as diagnostic text
    //     | static assertion failed
    ArgumentOutOfRangeException::ThrowIfEqual(&first, &second, "p");
#else
    // Compare what the pointers point at, not the addresses.
    ArgumentOutOfRangeException::ThrowIfEqual(first, second, "p");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(guard-char-pointer-operand): cannot render this type as diagnostic text
    //     | static assertion failed
    // T deduces to const char* by array-to-pointer decay, so this compared two addresses.
    ArgumentOutOfRangeException::ThrowIfEqual("a", "b", "p");
#else
    ArgumentOutOfRangeException::ThrowIfEqual(std::string_view("a"), std::string_view("b"), "p");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(guard-unrenderable-operand): cannot render this type as diagnostic text
    //     | static assertion failed
    ArgumentOutOfRangeException::ThrowIfGreaterThan(OrderedOnly{1}, OrderedOnly{0}, "p");
#else
    ArgumentOutOfRangeException::ThrowIfGreaterThan(OrderedRenderable{0}, OrderedRenderable{1},
                                                    "p");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(guard-ordering-without-ordering): must be less-than-comparable
    //     | static assertion failed
    ArgumentOutOfRangeException::ThrowIfLessThan(EqualityOnly{1}, EqualityOnly{2}, "p");
#else
    ArgumentOutOfRangeException::ThrowIfLessThan(OrderedRenderable{2}, OrderedRenderable{1}, "p");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // NEGATIVE(guard-equality-without-equality): must be equality-comparable
    //     | static assertion failed
    ArgumentOutOfRangeException::ThrowIfEqual(InequalityOnly{1}, InequalityOnly{2}, "p");
#else
    ArgumentOutOfRangeException::ThrowIfEqual(EqualityOnly{1}, EqualityOnly{2}, "p");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 6
    // NEGATIVE(guard-zero-without-default-construction): must be default-constructible and
    //     | equality-comparable
    //     | static assertion failed
    ArgumentOutOfRangeException::ThrowIfZero(NoDefaultCtor{1}, "p");
#else
    ArgumentOutOfRangeException::ThrowIfZero(EqualityOnly{1}, "p");
#endif

    (void)first;
    (void)second;
    return 0;
}
