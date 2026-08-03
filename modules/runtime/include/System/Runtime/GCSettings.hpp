// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/ArgumentOutOfRangeException.hpp"

namespace System::Runtime {

    /** Controls how the GC compacts the large-object heap. */
    enum class GCLargeObjectHeapCompactionMode {
        Default     = 1, ///< No compaction (default behaviour).
        CompactOnce = 2, ///< Compact the LOH on the next full GC.
    };

    /** Indicates the current garbage-collection latency mode. */
    enum class GCLatencyMode {
        Batch                = 0, ///< Optimise for throughput.
        Interactive          = 1, ///< Balance throughput and responsiveness (default).
        LowLatency           = 2, ///< Minimise GC pause times.
        SustainedLowLatency  = 3, ///< Minimise pauses for extended periods.
        NoGCRegion           = 4, ///< No-GC region is active.
    };

    /**
     * Provides global configuration settings for the garbage collector.
     * 
     * All members are static; the class cannot be instantiated.
     */
    class GCSettings {
        static GCLatencyMode latencyMode_;
        static GCLargeObjectHeapCompactionMode compactionMode_;
        static bool isServerGC_;

    public:
        GCSettings() = delete;

        /**
         * @return The current GC latency mode.
         *
         * @note The readable domain is deliberately WIDER than the writable one: `NoGCRegion`
         * is runtime-owned state that a caller may observe but not set. See
         * setLatencyModeProperty().
         */
        [[nodiscard]] static GCLatencyMode getLatencyModeProperty()            { return latencyMode_; }

        /**
         * @brief Sets the GC latency mode.
         *
         * C++ counterpart of .NET GCSettings.LatencyMode's setter, which validates the value
         * from `Batch` through `SustainedLowLatency` and rejects anything else. `NoGCRegion` is
         * excluded on purpose: it reports that a no-GC region is active, which only the runtime
         * may establish, so it is a legal value to read and an illegal value to write.
         *
         * @param m The new latency mode.
         * @throws System::ArgumentOutOfRangeException if @p m is outside
         *         `Batch`..`SustainedLowLatency`. Without this check an arbitrary enum cast
         *         became persistent, observable global configuration (ticket #1976 /
         *         SR-AUD-156).
         */
        static void setLatencyModeProperty(GCLatencyMode m) {
            if (m < GCLatencyMode::Batch || m > GCLatencyMode::SustainedLowLatency) {
                throw System::ArgumentOutOfRangeException("value");
            }
            latencyMode_ = m;
        }

        /** @return The current LOH compaction mode. */
        [[nodiscard]] static GCLargeObjectHeapCompactionMode getLargeObjectHeapCompactionModeProperty() {
            return compactionMode_;
        }

        /**
         * @brief Sets the LOH compaction mode.
         *
         * C++ counterpart of .NET GCSettings.LargeObjectHeapCompactionMode's setter, which
         * validates the value from `Default` through `CompactOnce` and rejects anything else.
         *
         * @param m The new large-object-heap compaction mode.
         * @throws System::ArgumentOutOfRangeException if @p m is outside
         *         `Default`..`CompactOnce`. Note that `0` is NOT a member of this enum -- it
         *         starts at `Default = 1` -- so the natural value-initialised default is an
         *         out-of-domain value and is rejected (ticket #1976 / SR-AUD-156).
         */
        static void setLargeObjectHeapCompactionModeProperty(GCLargeObjectHeapCompactionMode m) {
            if (m < GCLargeObjectHeapCompactionMode::Default ||
                m > GCLargeObjectHeapCompactionMode::CompactOnce) {
                throw System::ArgumentOutOfRangeException("value");
            }
            compactionMode_ = m;
        }

        /** @return True if the server GC is enabled (always false in this stub). */
        [[nodiscard]] static bool getIsServerGCProperty() { return isServerGC_; }
    };

    inline GCLatencyMode GCSettings::latencyMode_ = GCLatencyMode::Interactive;
    inline GCLargeObjectHeapCompactionMode GCSettings::compactionMode_ = GCLargeObjectHeapCompactionMode::Default;
    inline bool GCSettings::isServerGC_ = false;

} // namespace System::Runtime
