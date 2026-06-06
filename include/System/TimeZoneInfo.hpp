// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "System/DateTime.hpp"
#include "System/TimeSpan.hpp"

namespace System {

    class TimeZoneInfo {
        std::string id_;
        std::string displayName_;
        std::string standardName_;
        std::string daylightName_;
        TimeSpan baseUtcOffset_;
        bool supportsDst_ = false;

        TimeZoneInfo(std::string id, TimeSpan baseUtcOffset,
                     std::string displayName, std::string standardName,
                     std::string daylightName, bool supportsDst)
            : id_(std::move(id)), displayName_(std::move(displayName)),
              standardName_(std::move(standardName)), daylightName_(std::move(daylightName)),
              baseUtcOffset_(baseUtcOffset), supportsDst_(supportsDst) {}

    public:
        [[nodiscard]] const std::string& getIdProperty()           const { return id_; }
        [[nodiscard]] const std::string& getDisplayNameProperty()  const { return displayName_; }
        [[nodiscard]] const std::string& getStandardNameProperty() const { return standardName_; }
        [[nodiscard]] const std::string& getDaylightNameProperty() const { return daylightName_; }
        [[nodiscard]] TimeSpan getBaseUtcOffsetProperty()          const { return baseUtcOffset_; }
        [[nodiscard]] bool getSupportsDaylightSavingTimeProperty() const { return supportsDst_; }

        [[nodiscard]] bool IsDaylightSavingTime(const DateTime& /*dt*/) const { return false; }
        [[nodiscard]] TimeSpan GetUtcOffset(const DateTime& /*dt*/) const { return baseUtcOffset_; }

        [[nodiscard]] bool IsAmbiguousTime(const DateTime& /*dt*/) const { return false; }
        [[nodiscard]] bool IsInvalidTime(const DateTime& /*dt*/)   const { return false; }

        [[nodiscard]] DateTime ConvertTimeToUtc(const DateTime& dt) const {
            return dt.Add(-baseUtcOffset_);
        }

        static const TimeZoneInfo& Utc() {
            static TimeZoneInfo tz("UTC", TimeSpan::Zero(), "Coordinated Universal Time",
                                   "Coordinated Universal Time", "Coordinated Universal Time", false);
            return tz;
        }

        static const TimeZoneInfo& Local() {
            static TimeZoneInfo tz("Local", TimeSpan::Zero(), "Local Time", "Local Time", "Local Time", false);
            return tz;
        }

        static std::shared_ptr<TimeZoneInfo> FindSystemTimeZoneById(const std::string& id) {
            if (id == "UTC") return std::make_shared<TimeZoneInfo>(Utc());
            throw std::invalid_argument("Time zone not found: " + id);
        }

        static std::vector<std::shared_ptr<TimeZoneInfo>> GetSystemTimeZones() {
            return { std::make_shared<TimeZoneInfo>(Utc()), std::make_shared<TimeZoneInfo>(Local()) };
        }

        static std::shared_ptr<TimeZoneInfo> CreateCustomTimeZone(
            const std::string& id, const TimeSpan& utcOffset,
            const std::string& displayName, const std::string& standardName)
        {
            return std::make_shared<TimeZoneInfo>(id, utcOffset, displayName, standardName, standardName, false);
        }

        static DateTime ConvertTimeBySystemTimeZoneId(const DateTime& dt, const std::string& destinationTimeZoneId) {
            auto tz = FindSystemTimeZoneById(destinationTimeZoneId);
            return dt.Add(tz->baseUtcOffset_);
        }
    };

} // namespace System
