// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

namespace SharpRuntime::Tests {

/// Owns an atomically-created scratch directory for one Xml.Linq test and removes it on exit.
///
/// Each process chooses a fresh directory rather than deleting a shared path up front.  A file
/// left by a previously interrupted test therefore cannot be overwritten or removed by a later,
/// parallel run.
class TestTemporaryDirectory final {
public:
    TestTemporaryDirectory() {
        const auto parent = std::filesystem::temp_directory_path();
        static std::atomic<std::uint64_t> sequence{0};
        std::random_device entropy;
        for (int attempt = 0; attempt < 128; ++attempt) {
            const auto random =
                (static_cast<std::uint64_t>(entropy()) << 32U) ^ entropy();
            const auto suffix = random ^ sequence.fetch_add(1, std::memory_order_relaxed);
            auto candidate = parent / ("sharp-runtime-xml-linq-" + std::to_string(suffix));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) {
                path_ = std::move(candidate);
                return;
            }
        }
        throw std::runtime_error("could not create a unique Xml.Linq test directory");
    }

    ~TestTemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TestTemporaryDirectory(const TestTemporaryDirectory&) = delete;
    TestTemporaryDirectory& operator=(const TestTemporaryDirectory&) = delete;

    [[nodiscard]] std::string path(const std::string& name) const {
        return (path_ / name).string();
    }

private:
    std::filesystem::path path_;
};

} // namespace SharpRuntime::Tests
