// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Runtime {

    enum class GCLargeObjectHeapCompactionMode {
        Default     = 1,
        CompactOnce = 2,
    };

    enum class GCLatencyMode {
        Batch                = 0,
        Interactive          = 1,
        LowLatency           = 2,
        SustainedLowLatency  = 3,
        NoGCRegion           = 4,
    };

    class GCSettings {
        static GCLatencyMode latencyMode_;
        static GCLargeObjectHeapCompactionMode compactionMode_;
        static bool isServerGC_;

    public:
        GCSettings() = delete;

        [[nodiscard]] static GCLatencyMode getLatencyModeProperty()            { return latencyMode_; }
        static void setLatencyModeProperty(GCLatencyMode m)                    { latencyMode_ = m; }

        [[nodiscard]] static GCLargeObjectHeapCompactionMode getLargeObjectHeapCompactionModeProperty() {
            return compactionMode_;
        }
        static void setLargeObjectHeapCompactionModeProperty(GCLargeObjectHeapCompactionMode m) {
            compactionMode_ = m;
        }

        [[nodiscard]] static bool getIsServerGCProperty() { return isServerGC_; }
    };

    inline GCLatencyMode GCSettings::latencyMode_ = GCLatencyMode::Interactive;
    inline GCLargeObjectHeapCompactionMode GCSettings::compactionMode_ = GCLargeObjectHeapCompactionMode::Default;
    inline bool GCSettings::isServerGC_ = false;

} // namespace System::Runtime
