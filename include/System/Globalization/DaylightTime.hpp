// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/DateTime.hpp"
#include "System/TimeSpan.hpp"

namespace System::Globalization {

/** <summary>Defines the period of daylight saving time, including the start, end, and time delta.</summary> */
class DaylightTime {
    System::DateTime start_;
    System::DateTime end_;
    System::TimeSpan delta_;

public:
    /** Constructs a DaylightTime with the given @p start, @p end, and clock @p delta. */
    DaylightTime(const System::DateTime& start, const System::DateTime& end, const System::TimeSpan& delta)
        : start_(start), end_(end), delta_(delta) {}

    /** @return The date and time when daylight saving time begins. */
    [[nodiscard]] const System::DateTime& getStartProperty() const { return start_; }
    /** @return The date and time when daylight saving time ends. */
    [[nodiscard]] const System::DateTime& getEndProperty()   const { return end_; }
    /** @return The clock offset applied when daylight saving time is in effect. */
    [[nodiscard]] const System::TimeSpan& getDeltaProperty() const { return delta_; }
};

} // namespace System::Globalization
