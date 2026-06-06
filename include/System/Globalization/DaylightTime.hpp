// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/DateTime.hpp"
#include "System/TimeSpan.hpp"

namespace System::Globalization {

    class DaylightTime {
        System::DateTime start_;
        System::DateTime end_;
        System::TimeSpan delta_;

    public:
        DaylightTime(const System::DateTime& start, const System::DateTime& end, const System::TimeSpan& delta)
            : start_(start), end_(end), delta_(delta) {}

        [[nodiscard]] const System::DateTime& getStartProperty() const { return start_; }
        [[nodiscard]] const System::DateTime& getEndProperty()   const { return end_; }
        [[nodiscard]] const System::TimeSpan& getDeltaProperty() const { return delta_; }
    };

} // namespace System::Globalization
