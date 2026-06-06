// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <functional>
#include <future>
#include <vector>

namespace System::Threading::Tasks {

    struct ParallelOptions {
        int MaxDegreeOfParallelism = -1; // -1 = unlimited
    };

    struct ParallelLoopState {
        bool shouldStop_  = false;
        bool isExceptional_ = false;

        void Stop()  { shouldStop_ = true; }
        void Break() { shouldStop_ = true; }
        [[nodiscard]] bool getShouldExitCurrentIterationProperty() const { return shouldStop_; }
        [[nodiscard]] bool getIsExceptionalProperty()              const { return isExceptional_; }
    };

    struct ParallelLoopResult {
        bool isCompleted_  = false;
        bool lowestBreakIteration_ = false;
        [[nodiscard]] bool getIsCompletedProperty() const { return isCompleted_; }
    };

    class Parallel {
    public:
        // For(int, int, Action<int>)
        static ParallelLoopResult For(int fromInclusive, int toExclusive, std::function<void(int)> body) {
            std::vector<std::future<void>> futures;
            for (int i = fromInclusive; i < toExclusive; ++i) {
                int idx = i;
                futures.push_back(std::async(std::launch::async, [body, idx]{ body(idx); }));
            }
            for (auto& f : futures) f.get();
            ParallelLoopResult result;
            result.isCompleted_ = true;
            return result;
        }

        // For(int, int, ParallelOptions, Action<int>)
        static ParallelLoopResult For(int fromInclusive, int toExclusive, const ParallelOptions& /*opts*/,
                                       std::function<void(int)> body) {
            return For(fromInclusive, toExclusive, std::move(body));
        }

        // ForEach<T>(IEnumerable<T>, Action<T>)
        template<typename TSource>
        static ParallelLoopResult ForEach(const std::vector<TSource>& source, std::function<void(TSource)> body) {
            std::vector<std::future<void>> futures;
            for (auto& item : source) {
                futures.push_back(std::async(std::launch::async, [body, &item]{ body(item); }));
            }
            for (auto& f : futures) f.get();
            ParallelLoopResult result;
            result.isCompleted_ = true;
            return result;
        }

        // Invoke(Action[])
        static void Invoke(std::initializer_list<std::function<void()>> actions) {
            std::vector<std::future<void>> futures;
            for (auto& a : actions)
                futures.push_back(std::async(std::launch::async, a));
            for (auto& f : futures) f.get();
        }
    };

} // namespace System::Threading::Tasks
