// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 6/7/25.
//

#pragma once

#include <string>

#include "System/Object.hpp"
#include "System/TimeSpan.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::longcs;

    /**
     * @brief Represents an instant in time, expressed as the number of ticks.
     *
     * This is a partial C++ counterpart of the .NET System::DateTime structure.
     *
     * @note Status: Partial.
     * @note This implementation currently focuses on tick storage and basic arithmetic.
     */
    class DateTime : public Object {
    private:
        longcs ticks_;

    public:
        /**
         * @brief Initializes a new instance with zero ticks.
         */
        DateTime();

        /**
         * @brief Initializes a new instance with the specified number of ticks.
         *
         * @param ticks A date and time expressed in 100-nanosecond ticks.
         */
        explicit DateTime(longcs ticks);

        /**
         * @brief Gets the number of ticks represented by this instance.
         *
         * @return Tick count.
         */
        [[nodiscard]] longcs getTicksProperty() const;

        /**
         * @brief Adds the specified time span to the current DateTime.
         *
         * @param value Time span to add.
         * @return A new DateTime instance.
         */
        [[nodiscard]] DateTime Add(const TimeSpan& value) const;

        /**
         * @brief Subtracts the specified time span from the current DateTime.
         *
         * @param value Time span to subtract.
         * @return A new DateTime instance.
         */
        [[nodiscard]] DateTime Subtract(const TimeSpan& value) const;

        /**
         * @brief Subtracts another DateTime from the current DateTime.
         *
         * @param value Another DateTime.
         * @return The time interval between the two instances.
         */
        [[nodiscard]] TimeSpan Subtract(const DateTime& value) const;

        /**
 * @brief Gets the current local date and time.
 *
 * @return Current local DateTime represented in .NET-compatible ticks.
 *
 * @note Status: PARTIAL
 * @note This currently stores ticks compatible with .NET DateTime.Ticks,
 *       but does not store DateTimeKind.
 */
        [[nodiscard]] static DateTime getNowProperty();
        [[nodiscard]] TimeSpan getTimeOfDayProperty() const;

        /**
         * @brief Returns a string representation of the current instance.
         *
         * @return String representation.
         */
        [[nodiscard]] std::string ToString() const override;

        [[nodiscard]] bool operator==(const DateTime& other) const;
        [[nodiscard]] bool operator!=(const DateTime& other) const;
        [[nodiscard]] bool operator<(const DateTime& other) const;
        [[nodiscard]] bool operator<=(const DateTime& other) const;
        [[nodiscard]] bool operator>(const DateTime& other) const;
        [[nodiscard]] bool operator>=(const DateTime& other) const;
        GetTypeNameHPP()
    };

} // namespace System