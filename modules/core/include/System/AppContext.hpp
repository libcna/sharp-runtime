// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <mutex>
#include <string>
#include <unordered_map>
#include "System/AppDomain.hpp"
#include "System/ArgumentException.hpp"

namespace System {

    /**
     * @brief Provides members for setting and retrieving data about an application's context.
     *
     * C++ counterpart of .NET System.AppContext static class.
     * Supports the named-data store, compatibility switches, and the base directory.
     */
    class AppContext {
    public:
        AppContext() = delete;

        // -----------------------------------------------------------------------
        // Properties
        // -----------------------------------------------------------------------

        /**
         * @brief Gets the file path of the base directory that the assembly resolver
         * uses to probe for assemblies.
         *
         * C++ counterpart of .NET AppContext.BaseDirectory.
         * Delegates to AppDomain.CurrentDomain().BaseDirectory.
         *
         * DOCUMENTED DIVERGENCE (finding SR-AUD-102). Current .NET resolves this from named
         * data key `APP_CONTEXT_BASE_DIRECTORY` first and only computes the directory when
         * that key is absent. This port always computes it, because the named data store
         * holds untyped `void*` and cannot be asked whether an entry is a string. Honouring
         * the key would additionally require returning by VALUE rather than by reference:
         * a reference into caller-owned data-store storage would have no liveness boundary.
         * Both are approval-bound -- ticket #2255,
         * docs/CoreAppContextNamedDataDesign.md.
         *
         * The returned reference refers to storage that outlives the call (it is owned by
         * the process-lifetime AppDomain), so it is safe to hold.
         * @return The base directory path (ends with a directory separator).
         */
        static const std::string& getBaseDirectoryProperty() {
            return AppDomain::CurrentDomain().getBaseDirectoryProperty();
        }

        /**
         * @brief Gets the name of the framework version targeted by the current application.
         *
         * C++ counterpart of .NET AppContext.TargetFrameworkName.
         * Always returns an empty string in sharp-runtime (CLR reflection not available):
         * the value comes from an assembly-level attribute on the entry assembly, and this
         * port has no assembly metadata to read. This is the declared reflection deviation
         * in CLAUDE.md's parity philosophy, not a missing implementation, and it is
         * indistinguishable from .NET's own empty result for a host with no entry assembly.
         * @return An empty string.
         */
        static std::string getTargetFrameworkNameProperty() {
            return {};
        }

        // -----------------------------------------------------------------------
        // Data store
        // -----------------------------------------------------------------------
        //
        // OWNERSHIP AND LIFETIME (ticket #2256, finding SR-AUD-102). .NET's
        // SetData(string, object) stores a BOXED OBJECT whose runtime type can be
        // interrogated. This port stores a bare `void*`, and the difference has two
        // documented consequences that a caller cannot otherwise see:
        //
        //  1. The pointer is BORROWED. This class neither copies nor owns nor extends the
        //     lifetime of whatever it points at, and it never deletes it. A caller who
        //     stores a pointer to an automatic object and lets that object die leaves a
        //     dangling entry that GetData will hand straight back. Store a pointer whose
        //     lifetime is at least as long as the process, or remove nothing and read
        //     nothing after the target dies -- there is no removal door.
        //  2. The stored value carries NO RUNTIME TYPE, so nothing here can ask whether an
        //     entry is a string. That is why two documented .NET behaviours are absent:
        //     getBaseDirectoryProperty() does not honour an APP_CONTEXT_BASE_DIRECTORY
        //     entry, and TryGetSwitch does not fall back to a string-valued data entry.
        //     Implementing either needs the store to carry a type, which is a public
        //     signature change on this class AND on AppDomain (whose SetData/GetData have
        //     forwarded here since #2249). That decision is ticket #2255; the design and
        //     the three priced alternatives are docs/CoreAppContextNamedDataDesign.md.
        //     Reading the `void*` back AS a std::string for those two keys is deliberately
        //     NOT done: a void* cannot be checked, so the cast would be undefined
        //     behaviour whenever the caller stored anything else.

        /**
         * @brief Returns the value of the named data element assigned to the current application.
         *
         * C++ counterpart of .NET AppContext.GetData(string).
         * Returns the borrowed pointer exactly as it was stored, including a null one --
         * a stored `nullptr` is therefore indistinguishable from an absent key.
         * @param name The name of the data element.
         * @return A pointer to the stored value, or nullptr if @p name is not found.
         */
        static void* GetData(const std::string& name) {
            std::lock_guard<std::mutex> lock(mutex_());
            auto& store = dataStore_();
            auto it = store.find(name);
            return (it != store.end()) ? it->second : nullptr;
        }

        /**
         * @brief Assigns a value to the named data element of the current application context.
         *
         * C++ counterpart of .NET AppContext.SetData(string, object).
         * Stores @p data as a BORROWED pointer -- see the ownership note above. Storing a
         * name that already has an entry REPLACES it; the previous pointer is dropped and
         * not deleted, because this class never owned it. .NET's own SetData likewise
         * overwrites.
         * @param name The name of the data element.
         * @param data A pointer to the value to associate with @p name.
         */
        static void SetData(const std::string& name, void* data) {
            std::lock_guard<std::mutex> lock(mutex_());
            dataStore_()[name] = data;
        }

        // -----------------------------------------------------------------------
        // Compatibility switches
        // -----------------------------------------------------------------------

        /**
         * @brief Tries to get the value of the named compatibility switch.
         *
         * C++ counterpart of .NET AppContext.TryGetSwitch(string, out bool).
         *
         * DOCUMENTED DIVERGENCE (finding SR-AUD-102). Current .NET falls back to the named
         * data store when no explicit switch entry exists and parses a STRING value there
         * as the switch's boolean. This port keeps the two maps independent, for the same
         * reason as above: a `void*` entry cannot be recognised as a string. Approval-bound,
         * ticket #2255.
         * @param switchName The name of the switch.
         * @param isEnabled  Set to the switch value on success, or false on failure.
         * @return true if the switch was found and its value was assigned to @p isEnabled;
         *         false if the switch has not been set.
         * @throws System::ArgumentException if @p switchName is empty.
         */
        static bool TryGetSwitch(const std::string& switchName, bool& isEnabled) {
            if (switchName.empty())
                throw System::ArgumentException("The value cannot be an empty string.", "switchName");
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

        /**
         * @brief Sets the value of the named compatibility switch.
         *
         * C++ counterpart of .NET AppContext.SetSwitch(string, bool).
         * @param switchName The name of the switch.
         * @param isEnabled  The value to assign to the switch.
         * @throws System::ArgumentException if @p switchName is empty.
         */
        static void SetSwitch(const std::string& switchName, bool isEnabled) {
            if (switchName.empty())
                throw System::ArgumentException("The value cannot be an empty string.", "switchName");
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
