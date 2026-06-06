// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Globalization {

    enum class GregorianCalendarTypes {
        Localized       = 1,
        USEnglish       = 2,
        MiddleEastFrench = 9,
        Arabic          = 10,
        TransliteratedEnglish = 11,
        TransliteratedFrench  = 12,
    };

} // namespace System::Globalization
