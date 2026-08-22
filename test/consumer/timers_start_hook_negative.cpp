// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for the final Timer start-order regression. Production declares and
// befriends TimerStartAccess<Timer>, but never defines it; the only specialization lives in
// TimerLifecyclePinTests.cpp. scripts/check_version_seam_odr.py enforces that single test-tree
// definition, while this fixture proves an ordinary consumer cannot arm the hook or reach its
// private storage.
//
// NEGATIVE-FIXTURE: component=Timers

#include "System/Timers/Timer.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Timers::Timer;

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(timer-start-seam-incomplete): incomplete type
    //     | used in nested name specifier
    SharpRuntime::Testing::TimerStartAccess<Timer>::setBeforeArmHook(nullptr);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(timer-start-hook-private): is private within this context
    //     | private member
    Timer::beforeArmTestHook_.store(nullptr);
#endif

    return 0;
}
