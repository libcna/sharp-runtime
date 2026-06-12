// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <mutex>
#include <string>
#include <unordered_map>
#include "System/AppDomain.hpp"

namespace System {

    /**
     * @brief Provides access to application-context information and data, mirroring
     *        the .NET @c System.AppContext static class.
     */
    class AppContext {
    public:
        AppContext() = delete;

        /// @brief Returns the base directory of the application — delegates to
        ///        @c AppDomain::CurrentDomain().getBaseDirectoryProperty().
        static const std::string& getBaseDirProperty() {
            return AppDomain::CurrentDomain().getBaseDirectoryProperty();
        }

        /// @brief Returns the named data item previously stored via @c SetData, or @c nullptr.
        static void* GetData(const std::string& name) {
            std::lock_guard<std::mutex> lock(mutex_());
            auto& store = dataStore_();
            auto it = store.find(name);
            return (it != store.end()) ? it->second : nullptr;
        }

        /// @brief Associates a named data item with this application context.
        static void SetData(const std::string& name, void* data) {
            std::lock_guard<std::mutex> lock(mutex_());
            dataStore_()[name] = data;
        }

        /// @brief Returns whether the named switch is enabled. Returns @c false and sets
        ///        @p isEnabled to @c false if the switch has not been set.
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

        /// @brief Sets the value of the named switch.
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
