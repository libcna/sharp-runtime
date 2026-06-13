// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 5/28/25.
//

#include "System/Random.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace System {

    Random::Random()
        : generator(std::random_device{}()) {}

    Random::Random(intcs seed)
        : generator(static_cast<uint32_t>(seed)) {}

    intcs Random::Next()
    {
        return Next(0, SharpRuntime::INTCS_MAX);
    }

    intcs Random::Next(intcs maxValue)
    {
        if (maxValue < 0)
        {
            throw ArgumentOutOfRangeException("maxValue must be >= 0");
        }

        if (maxValue == 0)
        {
            return 0;
        }

        return Next(0, maxValue);
    }

    intcs Random::Next(intcs minValue, intcs maxValue)
    {
        if (minValue > maxValue)
        {
            throw ArgumentOutOfRangeException("minValue must be <= maxValue");
        }

        if (minValue == maxValue)
        {
            return minValue;
        }

        std::uniform_int_distribution<intcs> distribution(minValue, maxValue - 1);
        return distribution(generator);
    }

    double Random::NextDouble()
    {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(generator);
    }

    void Random::NextBytes(std::vector<bytecs>& buffer)
    {
        std::uniform_int_distribution<int> dist(0, 255);
        for (auto& b : buffer)
            b = static_cast<bytecs>(dist(generator));
    }

} // namespace System