// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <mutex>
#include <string>
#include <any>
#include <cctype>
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
        static std::string getBaseDirectoryProperty() {
            // .NET's own resolution (`AppContext.cs:28-32`):
            //     GetData("APP_CONTEXT_BASE_DIRECTORY") as string ?? GetBaseDirectoryCore()
            // and its comment says the value "has to be a string and it is not allowed to be any
            // other type" -- an `as string` cast, so a non-string entry falls through silently
            // rather than throwing. #2255 makes that expressible: `std::any_cast<std::string>`
            // on a pointer answers the question a `void*` could only be reinterpreted blindly to.
            //
            // The return type moved from `const std::string&` to a value with #2255, because the
            // override is materialised here and there is no stable storage to lend.
            if (const std::string* override_ = std::any_cast<std::string>(&dataStoreEntry_(
                    "APP_CONTEXT_BASE_DIRECTORY"))) {
                return *override_;
            }
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
         * C++ counterpart of .NET `AppContext.GetData(string)`, which returns `object?`.
         *
         * @par Ticket #2255 typed the store
         * It was a `std::unordered_map<std::string, void*>`, which carries **no type tag and no
         * ownership** -- so nothing could ask whether an entry was a string, and a caller who
         * stored a pointer to a temporary left a dangling entry `GetData` handed straight back.
         * A `std::any` carries both.
         *
         * @param name The name of the data element.
         * @return The stored value, or an empty `std::any` if @p name is not found.
         */
        static std::any GetData(const std::string& name) {
            std::lock_guard<std::mutex> lock(mutex_());
            auto& store = dataStore_();
            auto it = store.find(name);
            return (it != store.end()) ? it->second : std::any{};
        }

        /**
         * @brief Assigns a value to the named data element of the current application context.
         *
         * C++ counterpart of .NET `AppContext.SetData(string, object)`, which stores a **boxed
         * object whose runtime type can be interrogated**. Since ticket #2255 this port stores a
         * `std::any`, which is that -- so the store now OWNS its values, and the dangling-entry
         * hazard the `void*` carried is gone.
         *
         * Storing a name that already has an entry REPLACES it, as .NET's does.
         *
         * @param name The name of the data element.
         * @param data The value to associate with @p name.
         */
        static void SetData(const std::string& name, std::any data) {
            std::lock_guard<std::mutex> lock(mutex_());
            dataStore_()[name] = std::move(data);
        }

        // -----------------------------------------------------------------------
        // Compatibility switches
        // -----------------------------------------------------------------------

        /**
         * @brief Tries to get the value of the named compatibility switch.
         *
         * C++ counterpart of .NET AppContext.TryGetSwitch(string, out bool).
         *
         * @par The string fallback works since ticket #2255
         * .NET falls back to the named data store when no explicit switch entry exists and
         * parses a **string** value there as the switch's boolean (`AppContext.cs:158-161`):
         * `if (GetData(switchName) is string value && bool.TryParse(value, out isEnabled))`.
         * This port could not do it while the store held `void*`, because a `void*` cannot be
         * recognised as a string -- reading one back AS a `std::string` would have been
         * undefined behaviour, unfalsifiable at the point of use. `std::any` makes the question
         * answerable, so the fallback is real.
         *
         * The parse is .NET's `bool.TryParse`, which accepts only `"True"`/`"False"`
         * case-insensitively with surrounding whitespace trimmed -- not `"1"`, not `"yes"`.
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

            // .NET's fallback, now reachable because the store is typed. The lock is already
            // held, so the entry is read directly rather than through GetData().
            auto& store = dataStore_();
            auto entry = store.find(switchName);
            if (entry != store.end()) {
                if (const std::string* text = std::any_cast<std::string>(&entry->second)) {
                    if (tryParseBoolean(*text, isEnabled)) return true;
                }
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
        /// .NET's `bool.TryParse`: only "True"/"False", case-insensitive, surrounding whitespace
        /// trimmed. Deliberately NOT "1"/"0"/"yes" -- a laxer parser here would accept switch
        /// values .NET rejects, which is the kind of quiet widening this port avoids.
        static bool tryParseBoolean(const std::string& text, bool& value) {
            const auto first = text.find_first_not_of(" \t\n\r\f\v");
            if (first == std::string::npos) return false;
            const auto last = text.find_last_not_of(" \t\n\r\f\v");
            std::string trimmed = text.substr(first, last - first + 1);
            for (char& c : trimmed) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (trimmed == "true")  { value = true;  return true; }
            if (trimmed == "false") { value = false; return true; }
            return false;
        }

        /// Returns a reference to the stored entry, or to a shared empty `std::any` when absent.
        /// Used only by getBaseDirectoryProperty, which needs a POINTER-form any_cast so a
        /// non-string entry falls through rather than throwing -- matching .NET's `as string`.
        static const std::any& dataStoreEntry_(const std::string& name) {
            static const std::any kAbsent{};
            std::lock_guard<std::mutex> lock(mutex_());
            auto& store = dataStore_();
            auto it = store.find(name);
            return (it != store.end()) ? it->second : kAbsent;
        }

        static std::mutex& mutex_() {
            static std::mutex m;
            return m;
        }
        static std::unordered_map<std::string, std::any>& dataStore_() {
            static std::unordered_map<std::string, std::any> s;
            return s;
        }
        static std::unordered_map<std::string, bool>& switches_() {
            static std::unordered_map<std::string, bool> s;
            return s;
        }
    };

} // namespace System
