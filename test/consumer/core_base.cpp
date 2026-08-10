// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Exception.hpp"
#include "System/String.hpp"
#include <string>
#include <string_view>

namespace {
    // Ticket #1776 (REMED-CORE-ARGNULL-MESSAGE): a standalone, -Werror public-header
    // consumer verifying ArgumentNullException(paramName) constructs, throws, and is
    // catchable through System::Exception with a single (not doubled) parameter suffix.
    int checkArgumentNullExceptionMessage() {
        try {
            throw System::ArgumentNullException("destination");
        } catch (const System::Exception& exception) {
            const bool paramNameOk =
                dynamic_cast<const System::ArgumentNullException&>(exception)
                    .getParamNameProperty() == "destination";
            const bool messageOk =
                std::string(exception.what()) == "Value cannot be null. (Parameter 'destination')";
            return (paramNameOk && messageOk) ? 0 : 1;
        }
        return 1;
    }

    // Ticket #2254 (finding SR-AUD-091). The nine ArgumentOutOfRangeException::ThrowIf* guards
    // formatted every operand with a bare std::to_string(value), an undeclared requirement on top
    // of each guard's declared comparison contract that rejected half of the advertised generic
    // surface -- including std::string, std::string_view and every scoped enumeration. This is the
    // POSITIVE half of that repair, proven from outside the library: the negative half, that the
    // deliberately excluded shapes really are rejected per site, is
    // test/consumer/core_argument_out_of_range_guard_domain_negative.cpp.
    //
    // Each block below exercises one branch of the formatter, and the ordering matters: branch 1
    // is the pre-existing std::to_string call, so an arithmetic operand must still render exactly
    // as it always did.

    // Branch 2 -- this repository's own rendering convention, System::Object::ToString(), which
    // Decimal, TimeSpan, DateTime and Guid all provide.
    struct Money {
        int cents;
        bool operator==(const Money&) const = default;
        [[nodiscard]] std::string ToString() const { return "$" + std::to_string(cents); }
    };

    // Branch 3 -- a scoped enumeration, which did not compile at all before #2254.
    enum class Severity : int { Low = 1, High = 7 };

    // Branch 6 -- the documented ADL extension point, which is how a comparison-only type opts in
    // without this header paying for <sstream>.
    struct Slot {
        int index;
        bool operator==(const Slot&) const = default;
        friend bool operator>(Slot a, Slot b) { return a.index > b.index; }
    };
    std::string to_string(Slot s) { return "slot#" + std::to_string(s.index); }

    // Returns the actual value the guard recorded, or "<none>" if it did not throw.
    template<typename Fn>
    std::string actualValueOf(Fn&& fn) {
        try {
            fn();
        } catch (const System::ArgumentOutOfRangeException& exception) {
            return exception.getActualValueProperty();
        }
        return "<none>";
    }

    int checkArgumentOutOfRangeGuardDomain() {
        // Branch 1: unchanged, and the regression pin for the whole ticket.
        if (actualValueOf([] {
                System::ArgumentOutOfRangeException::ThrowIfGreaterThan(10, 5, "val");
            }) != "10") {
            return 1;
        }
        // Branch 2: a ToString() member.
        if (actualValueOf([] {
                System::ArgumentOutOfRangeException::ThrowIfEqual(Money{500}, Money{500}, "price");
            }) != "$500") {
            return 1;
        }
        // Branch 3: a scoped enumeration renders as its underlying integer, exactly as an
        // unscoped one already did through integral promotion.
        if (actualValueOf([] {
                System::ArgumentOutOfRangeException::ThrowIfEqual(Severity::High, Severity::High,
                                                                  "severity");
            }) != "7") {
            return 1;
        }
        // Branch 4: std::string and std::string_view. Comparing two strings is an obvious call
        // that did not compile before #2254.
        if (actualValueOf([] {
                System::ArgumentOutOfRangeException::ThrowIfEqual(std::string("a"),
                                                                  std::string("a"), "p");
            }) != "a") {
            return 1;
        }
        if (actualValueOf([] {
                System::ArgumentOutOfRangeException::ThrowIfNotEqual(std::string_view("left"),
                                                                      std::string_view("right"),
                                                                      "side");
            }) != "left") {
            return 1;
        }
        // Branch 6: the ADL extension point.
        if (actualValueOf([] {
                System::ArgumentOutOfRangeException::ThrowIfGreaterThan(Slot{9}, Slot{2}, "slot");
            }) != "slot#9") {
            return 1;
        }
        // A guard that does not fire must not render its operand at all.
        if (actualValueOf([] {
                System::ArgumentOutOfRangeException::ThrowIfGreaterThan(Slot{2}, Slot{9}, "slot");
            }) != "<none>") {
            return 1;
        }
        return 0;
    }
}

int main() {
    if (System::String::IsNullOrEmpty("") == false) return 1;
    if (checkArgumentNullExceptionMessage() != 0) return 1;
    return checkArgumentOutOfRangeGuardDomain();
}
