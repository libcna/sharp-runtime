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

    /**
     * @brief Represents a time zone — a named region with a fixed UTC offset.
     *
     * Implements the subset of the .NET @c System.TimeZoneInfo API needed for game-engine
     * porting. @c Local() reads the real system timezone via POSIX @c localtime_r().
     * @c FindSystemTimeZoneById() resolves IANA names from @c /usr/share/zoneinfo/.
     */
    class TimeZoneInfo {
        std::string id_;
        std::string displayName_;
        std::string standardName_;
        std::string daylightName_;
        TimeSpan    baseUtcOffset_;
        bool        supportsDst_ = false;

        TimeZoneInfo(std::string id, TimeSpan baseUtcOffset,
                     std::string displayName, std::string standardName,
                     std::string daylightName, bool supportsDst)
            : id_(std::move(id)), displayName_(std::move(displayName)),
              standardName_(std::move(standardName)), daylightName_(std::move(daylightName)),
              baseUtcOffset_(baseUtcOffset), supportsDst_(supportsDst) {}

    public:
        /// @brief Returns the IANA or well-known identifier of this time zone.
        [[nodiscard]] const std::string& getIdProperty()           const { return id_; }
        /// @brief Returns a human-readable display name for this time zone.
        [[nodiscard]] const std::string& getDisplayNameProperty()  const { return displayName_; }
        /// @brief Returns the standard (non-DST) name.
        [[nodiscard]] const std::string& getStandardNameProperty() const { return standardName_; }
        /// @brief Returns the daylight-saving name (may equal standard name when DST is not supported).
        [[nodiscard]] const std::string& getDaylightNameProperty() const { return daylightName_; }
        /// @brief Returns the fixed UTC offset for this zone (DST transitions are not modelled).
        [[nodiscard]] TimeSpan getBaseUtcOffsetProperty()          const { return baseUtcOffset_; }
        /// @brief Returns @c true if this zone ever observes daylight saving time.
        [[nodiscard]] bool getSupportsDaylightSavingTimeProperty() const { return supportsDst_; }

        /// @brief Always returns @c false — DST transitions are not modelled.
        [[nodiscard]] bool IsDaylightSavingTime(const DateTime& /*dt*/) const { return false; }
        /// @brief Returns the fixed UTC offset for any @p dt.
        [[nodiscard]] TimeSpan GetUtcOffset(const DateTime& /*dt*/) const { return baseUtcOffset_; }

        /// @brief Always returns @c false.
        [[nodiscard]] bool IsAmbiguousTime(const DateTime& /*dt*/) const { return false; }
        /// @brief Always returns @c false.
        [[nodiscard]] bool IsInvalidTime(const DateTime& /*dt*/)   const { return false; }

        /// @brief Converts @p dt to UTC by subtracting the zone's UTC offset.
        [[nodiscard]] DateTime ConvertTimeToUtc(const DateTime& dt) const {
            return dt.Add(-baseUtcOffset_);
        }

        /// @brief Returns the UTC singleton (offset zero, no DST).
        static const TimeZoneInfo& Utc() {
            static TimeZoneInfo tz("UTC", TimeSpan::Zero,
                                   "Coordinated Universal Time",
                                   "Coordinated Universal Time",
                                   "Coordinated Universal Time", false);
            return tz;
        }

        /// @brief Returns the local system time zone by reading the OS timezone via
        ///        POSIX @c localtime_r(). The offset reflects the current wall-clock offset
        ///        (including any active DST).
        static const TimeZoneInfo& Local();

        /// @brief Looks up a time zone by IANA ID (e.g. @c "Europe/Prague") using
        ///        @c /usr/share/zoneinfo/. Throws @c std::invalid_argument if not found.
        static std::shared_ptr<TimeZoneInfo> FindSystemTimeZoneById(const std::string& id);

        /// @brief Returns UTC and Local as the minimal system zone list.
        static std::vector<std::shared_ptr<TimeZoneInfo>> GetSystemTimeZones();

        /// @brief Creates a fixed-offset zone with the given parameters.
        static std::shared_ptr<TimeZoneInfo> CreateCustomTimeZone(
            const std::string& id, const TimeSpan& utcOffset,
            const std::string& displayName, const std::string& standardName)
        {
            return std::shared_ptr<TimeZoneInfo>(
                new TimeZoneInfo(id, utcOffset, displayName, standardName, standardName, false));
        }

        /// @brief Converts @p dt (assumed UTC) to the zone identified by @p destinationTimeZoneId.
        static DateTime ConvertTimeBySystemTimeZoneId(const DateTime& dt,
                                                      const std::string& destinationTimeZoneId) {
            auto tz = FindSystemTimeZoneById(destinationTimeZoneId);
            return dt.Add(tz->baseUtcOffset_);
        }

        /// @brief Converts @p dt (assumed UTC) to the specified destination time zone.
        static DateTime ConvertTime(const DateTime& dt, const TimeZoneInfo& destinationTimeZone) {
            return dt.Add(destinationTimeZone.baseUtcOffset_);
        }

        /// @brief Converts @p dt from @p sourceTimeZone to @p destinationTimeZone.
        static DateTime ConvertTime(const DateTime& dt,
                                    const TimeZoneInfo& sourceTimeZone,
                                    const TimeZoneInfo& destinationTimeZone) {
            DateTime utc = dt.Add(-sourceTimeZone.baseUtcOffset_);
            return utc.Add(destinationTimeZone.baseUtcOffset_);
        }

        /// @brief Converts a UTC @p dt to the specified destination time zone.
        static DateTime ConvertTimeFromUtc(const DateTime& dt, const TimeZoneInfo& destinationTimeZone) {
            return dt.Add(destinationTimeZone.baseUtcOffset_);
        }

        /// @brief Tries to find a time zone by id; returns false instead of throwing.
        static bool TryFindSystemTimeZoneById(const std::string& id,
                                              std::shared_ptr<TimeZoneInfo>& result) {
            try {
                result = FindSystemTimeZoneById(id);
                return true;
            } catch (...) {
                return false;
            }
        }

        /// @brief Returns @c true if this zone has the same base UTC offset as @p other.
        [[nodiscard]] bool HasSameRules(const TimeZoneInfo& other) const {
            return baseUtcOffset_ == other.baseUtcOffset_ &&
                   supportsDst_ == other.supportsDst_;
        }

        /// @brief Returns @c true if this zone has the same ID as @p other.
        [[nodiscard]] bool Equals(const TimeZoneInfo& other) const { return id_ == other.id_; }

        bool operator==(const TimeZoneInfo& other) const { return Equals(other); }
        bool operator!=(const TimeZoneInfo& other) const { return !Equals(other); }
    };

} // namespace System
