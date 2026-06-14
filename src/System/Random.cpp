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

    float Random::NextSingle()
    {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(generator);
    }

    longcs Random::NextInt64()
    {
        return NextInt64(0LL, SharpRuntime::LONGCS_MAX);
    }

    longcs Random::NextInt64(longcs maxValue)
    {
        if (maxValue < 0)
            throw ArgumentOutOfRangeException("maxValue must be >= 0");
        if (maxValue == 0)
            return 0;
        return NextInt64(0LL, maxValue);
    }

    longcs Random::NextInt64(longcs minValue, longcs maxValue)
    {
        if (minValue > maxValue)
            throw ArgumentOutOfRangeException("minValue must be <= maxValue");
        if (minValue == maxValue)
            return minValue;
        std::uniform_int_distribution<longcs> dist(minValue, maxValue - 1);
        return dist(generator);
    }

    void Random::NextBytes(std::vector<bytecs>& buffer)
    {
        std::uniform_int_distribution<int> dist(0, 255);
        for (auto& b : buffer)
            b = static_cast<bytecs>(dist(generator));
    }

    void Random::NextBytes(Span<bytecs> buffer)
    {
        std::uniform_int_distribution<int> dist(0, 255);
        for (intcs i = 0; i < buffer.getLengthProperty(); ++i)
            buffer[i] = static_cast<bytecs>(dist(generator));
    }

    Random& Random::getSharedProperty()
    {
        static Random instance;
        return instance;
    }

    std::string Random::GetString(const std::string& choices, intcs length)
    {
        std::string result;
        result.reserve(static_cast<std::size_t>(length));
        for (intcs i = 0; i < length; ++i)
            result += choices[static_cast<std::size_t>(
                Next(static_cast<intcs>(choices.size())))];
        return result;
    }

    std::string Random::GetHexString(intcs stringLength, bool lowercase)
    {
        static constexpr const char* upper = "0123456789ABCDEF";
        static constexpr const char* lower = "0123456789abcdef";
        const char* chars = lowercase ? lower : upper;
        std::string result;
        result.reserve(static_cast<std::size_t>(stringLength));
        for (intcs i = 0; i < stringLength; ++i)
            result += chars[Next(16)];
        return result;
    }

    void Random::GetHexString(Span<char> destination, bool lowercase)
    {
        static constexpr const char* upper = "0123456789ABCDEF";
        static constexpr const char* lower = "0123456789abcdef";
        const char* chars = lowercase ? lower : upper;
        for (intcs i = 0; i < destination.getLengthProperty(); ++i)
            destination[i] = chars[Next(16)];
    }

} // namespace System