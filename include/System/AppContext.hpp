// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace System {

    class AppContext {
    public:
        AppContext() = delete;

        static const std::string& getBaseDirProperty() {
            static std::string base = ".";
            return base;
        }

        static void* GetData(const std::string& name) {
            std::lock_guard<std::mutex> lock(mutex_());
            auto& store = dataStore_();
            auto it = store.find(name);
            return (it != store.end()) ? it->second : nullptr;
        }

        static void SetData(const std::string& name, void* data) {
            std::lock_guard<std::mutex> lock(mutex_());
            dataStore_()[name] = data;
        }

        static bool TryGetSwitch(const std::string& switchName, bool& isEnabled) {
            std::lock_guard<std::mutex> lock(mutex_());
            auto& sw = switches_();
            auto it = sw.find(switchName);
            if (it != sw.end()) {
                isEnabled = it->second;
                return true;
            }
            isEnabled = false;
            return false;
        }

        static void SetSwitch(const std::string& switchName, bool isEnabled) {
            std::lock_guard<std::mutex> lock(mutex_());
            switches_()[switchName] = isEnabled;
        }

    private:
        static std::mutex& mutex_() {
            static std::mutex m;
            return m;
        }
        static std::unordered_map<std::string, void*>& dataStore_() {
            static std::unordered_map<std::string, void*> s;
            return s;
        }
        static std::unordered_map<std::string, bool>& switches_() {
            static std::unordered_map<std::string, bool> s;
            return s;
        }
    };

} // namespace System
