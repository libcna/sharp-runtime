// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors

// Standalone, opt-in baseline for the OrderedDictionary operations used by CNA's
// content pipeline. Timings are observations, never correctness thresholds.
// Configure a Release build in build-probe/ and run this executable repeatedly.
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "System/Collections/Generic/OrderedDictionary.hpp"

namespace {

using Clock = std::chrono::steady_clock;
using OrderedDictionary = System::Collections::Generic::OrderedDictionary<std::string, std::string>;

volatile std::size_t sink = 0;

std::vector<std::string> MakeKeys(std::size_t count) {
    std::vector<std::string> keys;
    keys.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        keys.push_back("content_asset_" + std::to_string(i));
    return keys;
}

template <typename Fn>
void Measure(const char* operation, std::size_t size, std::size_t calls, Fn&& fn) {
    const auto start = Clock::now();
    const std::size_t observed = fn();
    const auto end = Clock::now();
    sink = sink + observed;
    const auto ns = std::chrono::duration<double, std::nano>(end - start).count();
    std::printf("%s,%zu,%zu,%.1f\n", operation, size, calls, ns / static_cast<double>(calls));
}

void Run(std::size_t size) {
    const auto keys = MakeKeys(size);
    const std::string value = "asset_payload";
    const std::size_t rounds = size <= 256 ? 64 : 8;

    Measure("add", size, size * rounds, [&] {
        std::size_t observed = 0;
        for (std::size_t round = 0; round < rounds; ++round) {
            OrderedDictionary dictionary;
            for (const auto& key : keys) dictionary.Add(key, value);
            observed += static_cast<std::size_t>(dictionary.getCountProperty());
        }
        return observed;
    });

    OrderedDictionary dictionary;
    for (const auto& key : keys) dictionary.Add(key, value);
    std::vector<std::string> missing;
    missing.reserve(size);
    for (std::size_t i = 0; i < size; ++i)
        missing.push_back("absent_" + std::to_string(i));
    constexpr std::size_t lookups = 100000;

    Measure("contains_hit", size, lookups, [&] {
        std::size_t observed = 0;
        for (std::size_t i = 0; i < lookups; ++i)
            observed += dictionary.ContainsKey(keys[i % size]);
        return observed;
    });

    Measure("lookup_hit", size, lookups, [&] {
        std::size_t observed = 0;
        for (std::size_t i = 0; i < lookups; ++i)
            observed += static_cast<const OrderedDictionary&>(dictionary)[keys[i % size]].size();
        return observed;
    });

    Measure("contains_miss", size, lookups, [&] {
        std::size_t observed = 0;
        for (std::size_t i = 0; i < lookups; ++i)
            observed += dictionary.ContainsKey(missing[i % size]);
        return observed;
    });

    Measure("ordered_scan", size, size * rounds, [&] {
        std::size_t observed = 0;
        for (std::size_t round = 0; round < rounds; ++round)
            for (const auto& entry : dictionary) {
                // Keep each traversal observable to the optimizer without a runtime fence.
                std::atomic_signal_fence(std::memory_order_seq_cst);
                observed += entry.first.size() + entry.second.size();
            }
        return observed;
    });

    std::vector<OrderedDictionary> removable(rounds);
    for (auto& item : removable)
        for (const auto& key : keys) item.Add(key, value);

    Measure("remove_front", size, size * rounds, [&] {
        std::size_t observed = 0;
        for (auto& item : removable)
            for (const auto& key : keys) observed += item.Remove(key);
        return observed;
    });
}

} // namespace

int main() {
    std::fprintf(stderr, "sizeof(OrderedDictionary<string,string>)=%zu\n",
                 sizeof(OrderedDictionary));
    std::puts("operation,entries,calls,ns_per_call");
    for (const std::size_t size : {16U, 256U, 4096U}) Run(size);
    std::fprintf(stderr, "checksum=%zu\n", static_cast<std::size_t>(sink));
    return 0;
}
