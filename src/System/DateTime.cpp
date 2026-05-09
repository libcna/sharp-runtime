//
// Created by robertvokac on 6/7/25.
//

#include "System/DateTime.hpp"
#include <chrono>

namespace System {

    DateTime::DateTime()
        : ticks_(0) {
    }

    DateTime::DateTime(longcs ticks)
        : ticks_(ticks) {
    }

    longcs DateTime::getTicksProperty() const {
        return ticks_;
    }

    DateTime DateTime::Add(const TimeSpan& value) const {
        return DateTime(ticks_ + value.getTicksProperty());
    }

    DateTime DateTime::Subtract(const TimeSpan& value) const {
        return DateTime(ticks_ - value.getTicksProperty());
    }

    TimeSpan DateTime::Subtract(const DateTime& value) const {
        return TimeSpan(ticks_ - value.ticks_);
    }

    DateTime DateTime::getNowProperty() {
        using namespace std::chrono;

        static constexpr longcs TicksPerSecond = 10000000LL;
        static constexpr longcs UnixEpochTicks = 621355968000000000LL;

        const auto now = system_clock::now();
        const auto duration = now.time_since_epoch();

        const auto secondsPart = duration_cast<seconds>(duration);
        const auto subSecondPart = duration - secondsPart;

        const longcs ticks =
            UnixEpochTicks
            + static_cast<longcs>(secondsPart.count()) * TicksPerSecond
            + static_cast<longcs>(duration_cast<nanoseconds>(subSecondPart).count() / 100);

        return DateTime(ticks);
    }

    TimeSpan DateTime::getTimeOfDayProperty() const {
        static constexpr longcs TicksPerDay = 864000000000LL;
        return {ticks_ % TicksPerDay};
    }

    std::string DateTime::ToString() const {
        return "DateTime(" + std::to_string(ticks_) + ")";
    }

    bool DateTime::operator==(const DateTime& other) const {
        return ticks_ == other.ticks_;
    }

    bool DateTime::operator!=(const DateTime& other) const {
        return ticks_ != other.ticks_;
    }

    bool DateTime::operator<(const DateTime& other) const {
        return ticks_ < other.ticks_;
    }

    bool DateTime::operator<=(const DateTime& other) const {
        return ticks_ <= other.ticks_;
    }

    bool DateTime::operator>(const DateTime& other) const {
        return ticks_ > other.ticks_;
    }

    bool DateTime::operator>=(const DateTime& other) const {
        return ticks_ >= other.ticks_;
    }
    GetTypeNameCPP(DateTime, "System::DateTime")

} // namespace System