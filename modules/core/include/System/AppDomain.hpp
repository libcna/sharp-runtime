// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <string>
#include <optional>
#include "System/ArgumentException.hpp"
#include "System/EventArgs.hpp"
#include "System/MarshalByRefObject.hpp"
#include "System/UnhandledExceptionEventHandler.hpp"
#include "System/Environment.hpp"

namespace System {

    /**
     * @brief Represents an application domain — the isolated execution environment
     * for an application. This class cannot be inherited.
     *
     * C++ counterpart of .NET System.AppDomain (sealed).
     * Only the singleton CurrentDomain() is supported. Implements the subset of
     * the .NET AppDomain API needed for game-engine porting: friendly name,
     * base directory, identity properties, and event/data stubs.
     *
     * Assembly loading, reflection, and code-access security are not implemented.
     */
    class AppDomain final : public MarshalByRefObject {
        std::string friendlyName_  = "DefaultDomain";
        std::string baseDirectory_;

        AppDomain();

    public:
        // -----------------------------------------------------------------------
        // Singleton access
        // -----------------------------------------------------------------------

        /**
         * @brief Gets the current application domain for the current thread.
         *
         * C++ counterpart of .NET AppDomain.CurrentDomain.
         * Returns the single process-wide AppDomain (there is no multi-domain
         * support in this port).
         */
        static AppDomain& CurrentDomain() {
            static AppDomain domain;
            return domain;
        }

        // -----------------------------------------------------------------------
        // Properties
        // -----------------------------------------------------------------------

        /**
         * @brief Gets the friendly name of this application domain.
         *
         * C++ counterpart of .NET AppDomain.FriendlyName.
         * Always returns @c "DefaultDomain" in this port.
         */
        [[nodiscard]] const std::string& getFriendlyNameProperty() const {
            return friendlyName_;
        }

        /**
         * @brief Gets the base directory that the assembly resolver uses to probe
         * for assemblies.
         *
         * C++ counterpart of .NET AppDomain.BaseDirectory.
         * The directory containing the executable, with a trailing '/': on Linux (and other
         * POSIX platforms besides macOS), resolved from /proc/self/exe; on macOS, from
         * _NSGetExecutablePath; on Windows, from GetModuleFileNameW. Falls back to "./" if
         * detection fails, and always "./" on Emscripten (no real executable path to resolve
         * -- runs against the virtual filesystem root).
         */
        [[nodiscard]] const std::string& getBaseDirectoryProperty() const {
            return baseDirectory_;
        }

        /**
         * @brief Gets the path under the base directory where the assembly resolver
         * probes for private assemblies.
         *
         * C++ counterpart of .NET AppDomain.RelativeSearchPath.
         * Always returns an empty string in this port (no dynamic probing).
         */
        [[nodiscard]] std::string getRelativeSearchPathProperty() const { return {}; }

        /**
         * @brief Gets the directory that the assembly resolver uses to probe for
         * dynamically created assemblies.
         *
         * C++ counterpart of .NET AppDomain.DynamicDirectory.
         * Always returns an empty string in this port.
         */
        [[nodiscard]] std::string getDynamicDirectoryProperty() const { return {}; }

        /**
         * @brief Gets a unique integer identifier for this application domain.
         *
         * C++ counterpart of .NET AppDomain.Id.
         * Always returns 1 (there is only one domain in this port).
         */
        [[nodiscard]] int getIdProperty() const noexcept { return 1; }

        /**
         * @brief Gets a value indicating whether the current application domain is
         * fully trusted.
         *
         * C++ counterpart of .NET AppDomain.IsFullyTrusted.
         * Always returns true in this port.
         */
        [[nodiscard]] bool getIsFullyTrustedProperty() const noexcept { return true; }

        /**
         * @brief Gets a value indicating whether the current application domain is
         * homogeneous.
         *
         * C++ counterpart of .NET AppDomain.IsHomogenous.
         * Always returns true in this port.
         */
        [[nodiscard]] bool getIsHomogenousProperty() const noexcept { return true; }

        // -----------------------------------------------------------------------
        // Methods
        // -----------------------------------------------------------------------

        /**
         * @brief Returns a value indicating whether this is the default application
         * domain for the current process.
         *
         * C++ counterpart of .NET AppDomain.IsDefaultAppDomain().
         * Always returns true in this port.
         */
        [[nodiscard]] bool IsDefaultAppDomain() const noexcept { return true; }

        /**
         * @brief Returns a value indicating whether this application domain is
         * being unloaded.
         *
         * C++ counterpart of .NET AppDomain.IsFinalizingForUnload().
         * Always returns false in this port (domains cannot be unloaded).
         */
        [[nodiscard]] bool IsFinalizingForUnload() const noexcept { return false; }

        /**
         * @brief Returns the policy-mapped assembly display name for a given
         * assembly name.
         *
         * C++ counterpart of .NET AppDomain.ApplyPolicy(string).
         * There is no policy engine in this port, so a valid name is returned
         * unchanged — but the identity route is applied only to input .NET itself
         * accepts. .NET rejects a null or empty @p assemblyName, and rejects a name
         * whose *first* character is NUL (the native identity parser sees a
         * zero-length string). A NUL anywhere else is accepted and returned
         * unchanged, which this port reproduces rather than "improving on".
         *
         * The null case is unrepresentable here: the parameter is a
         * @c const std::string&.
         *
         * **Both rejections carry the same message, and that is .NET's** (ticket #2260's
         * sibling #2252, resolved against the reference tree). `AppDomain.ApplyPolicy`
         * rejects the empty name through `ArgumentException.ThrowIfNullOrEmpty`, which
         * throws `ArgumentException(SR.Argument_EmptyString, paramName)`
         * (`ArgumentException.cs:126-130`), and rejects the leading-NUL name with
         * `ArgumentException(SR.Argument_EmptyString, nameof(assemblyName))`
         * (`AppDomain.cs:104-110`) — the *same* resource, which reads **"The value cannot be
         * an empty string."** (`Strings.resx:3992`). The `Argument_StringZeroLength` resource
         * this port previously quoted for the second case is a .NET Framework-era string that
         * **does not exist anywhere in the reference tree**.
         *
         * @param assemblyName The assembly display name to map.
         * @return @p assemblyName unchanged.
         * @throws System::ArgumentException if @p assemblyName is empty or begins
         *         with a NUL character.
         */
        [[nodiscard]] std::string ApplyPolicy(const std::string& assemblyName) const {
            if (assemblyName.empty())
                throw System::ArgumentException("The value cannot be an empty string.", "assemblyName");
            if (assemblyName[0] == '\0')
                throw System::ArgumentException("The value cannot be an empty string.", "assemblyName");
            return assemblyName;
        }

        /**
         * @brief Returns a string representation of this application domain.
         *
         * C++ counterpart of .NET AppDomain.ToString().
         */
        [[nodiscard]] std::string ToString() const {
            return "Name:" + friendlyName_;
        }

        // -----------------------------------------------------------------------
        // Data store
        // -----------------------------------------------------------------------

        /**
         * @brief Assigns a value to the named data element of the current domain.
         *
         * C++ counterpart of .NET AppDomain.SetData(string, object), which is a
         * direct AppContext.SetData forwarding call. This port forwards to
         * System::AppContext for the same reason: there is one domain, so the
         * domain's data store and the context's data store are the same store.
         *
         * The body is out of line because System/AppContext.hpp includes this
         * header for BaseDirectory; the include may not run the other way.
         *
         * @param name The name of the data element.
         * @param data A pointer to the value to associate with @p name. The store
         *             holds the pointer and owns nothing; keeping the pointee alive
         *             is the caller's responsibility.
         */
        void SetData(const std::string& name, void* data);

        /**
         * @brief Gets the value stored in the current domain under the given name.
         *
         * C++ counterpart of .NET AppDomain.GetData(string), which is a direct
         * AppContext.GetData forwarding call.
         *
         * @param name The name of the data element.
         * @return The pointer stored under @p name, or nullptr if @p name has no
         *         entry.
         */
        void* GetData(const std::string& name);

        // -----------------------------------------------------------------------
        // Events (stubs)
        // -----------------------------------------------------------------------

        /**
         * @brief Stub — event registration is not functional; provided for API
         * compatibility.
         *
         * C++ counterpart of .NET AppDomain.UnhandledException event add accessor.
         */
        void add_UnhandledException(const UnhandledExceptionEventHandler& /*handler*/) {}

        /**
         * @brief Stub — event registration is not functional; provided for API
         * compatibility.
         *
         * C++ counterpart of .NET AppDomain.UnhandledException event remove accessor.
         */
        void remove_UnhandledException(const UnhandledExceptionEventHandler& /*handler*/) {}

        /**
         * @brief Stub — event registration is not functional; provided for API
         * compatibility.
         *
         * C++ counterpart of .NET AppDomain.ProcessExit event add accessor.
         */
        void add_ProcessExit(const std::function<void(void*, EventArgs&)>& /*handler*/) {}

        /**
         * @brief Stub — event registration is not functional; provided for API
         * compatibility.
         *
         * C++ counterpart of .NET AppDomain.ProcessExit event remove accessor.
         */
        void remove_ProcessExit(const std::function<void(void*, EventArgs&)>& /*handler*/) {}

        /**
         * @brief Stub — event registration is not functional; provided for API
         * compatibility.
         *
         * C++ counterpart of .NET AppDomain.DomainUnload event add accessor.
         */
        void add_DomainUnload(const std::function<void(void*, EventArgs&)>& /*handler*/) {}

        /**
         * @brief Stub — event registration is not functional; provided for API
         * compatibility.
         *
         * C++ counterpart of .NET AppDomain.DomainUnload event remove accessor.
         */
        void remove_DomainUnload(const std::function<void(void*, EventArgs&)>& /*handler*/) {}

        // -----------------------------------------------------------------------
        // Additional properties
        // -----------------------------------------------------------------------

        /**
         * @brief Gets a value indicating whether shadow copying of files is enabled.
         *
         * C++ counterpart of .NET AppDomain.ShadowCopyFiles.
         * Always returns false in this port.
         */
        [[nodiscard]] bool getShadowCopyFilesProperty() const noexcept { return false; }

        /**
         * @brief Determines whether a compatibility switch is set.
         *
         * C++ counterpart of .NET `AppDomain.IsCompatibilitySwitchSet(string)`
         * (`AppDomain.cs:171-174`), transcribed:
         *
         * ```csharp
         * return AppContext.TryGetSwitch(value, out bool result) ? result : default(bool?);
         * ```
         *
         * @par Ticket #2250 made both approval-bound changes together
         * It used to `return false` unconditionally, without consulting the `System::AppContext`
         * switch registry at all — so a switch a caller had explicitly **set to true** still
         * reported as unset. Following .NET needed two changes that had to land together, and
         * SA-10 covers both:
         *
         *  - **the return type is `std::optional<bool>`**, because a C++ `bool` cannot
         *    distinguish an explicitly-false switch from an unset one — which is precisely the
         *    distinction `bool?` exists to carry;
         *  - **the `noexcept` is gone**, because `AppContext::TryGetSwitch` raises
         *    `System::ArgumentException` for an empty switch name and takes a `std::mutex` whose
         *    `lock()` can throw. Forwarding from a `noexcept` member would have turned both into
         *    `std::terminate`, so the drop is not a stylistic relaxation but the only safe way to
         *    forward at all.
         *
         * @param value The name of the compatibility switch.
         * @return The switch's value, or `std::nullopt` if it is not set.
         * @throws System::ArgumentException if @p value is empty, exactly as
         *         `AppContext::TryGetSwitch` does — the diagnostic now reaches the caller instead
         *         of being swallowed by an unconditional `false`.
         *
         * @note The body is out of line for the same reason `SetData`/`GetData` above are:
         *       `System/AppContext.hpp` includes this header for `BaseDirectory`, so the include
         *       may not run the other way.
         */
        [[nodiscard]] std::optional<bool> IsCompatibilitySwitchSet(const std::string& value) const;

        // -----------------------------------------------------------------------
        // Additional static methods
        // -----------------------------------------------------------------------

        /**
         * @brief Gets an integer that uniquely identifies the current managed thread.
         *
         * C++ counterpart of .NET AppDomain.GetCurrentThreadId().
         * @deprecated In .NET this method is deprecated; prefer Thread.ManagedThreadId.
         *             Delegates to Environment::getCurrentManagedThreadIdProperty().
         */
        [[nodiscard]] [[deprecated(
            "AppDomain.GetCurrentThreadId has been deprecated because it does not provide a "
            "stable Id when managed threads are running on fibers (aka lightweight threads). To "
            "get a stable identifier for a managed thread, use the ManagedThreadId property on "
            "Thread instead.")]]
        static SharpRuntime::intcs GetCurrentThreadId() {
            return Environment::getCurrentManagedThreadIdProperty();
        }

        // -----------------------------------------------------------------------
        // Deprecated no-op stub methods
        // -----------------------------------------------------------------------

        /**
         * @brief No-op stub.
         *
         * C++ counterpart of .NET AppDomain.SetDynamicBase(string) [Obsolete].
         */
        void SetDynamicBase(const std::string& /*path*/) {}

        /**
         * @brief No-op stub.
         *
         * C++ counterpart of .NET AppDomain.AppendPrivatePath(string) [Obsolete].
         */
        void AppendPrivatePath(const std::string& /*path*/) {}

        /**
         * @brief No-op stub.
         *
         * C++ counterpart of .NET AppDomain.ClearPrivatePath() [Obsolete].
         */
        void ClearPrivatePath() {}

        /**
         * @brief No-op stub.
         *
         * C++ counterpart of .NET AppDomain.ClearShadowCopyPath() [Obsolete].
         */
        void ClearShadowCopyPath() {}

        /**
         * @brief No-op stub.
         *
         * C++ counterpart of .NET AppDomain.SetShadowCopyFiles() [Obsolete].
         */
        void SetShadowCopyFiles() {}

        /**
         * @brief No-op stub.
         *
         * C++ counterpart of .NET AppDomain.SetShadowCopyPath(string) [Obsolete].
         */
        void SetShadowCopyPath(const std::string& /*path*/) {}

        /**
         * @brief No-op stub.
         *
         * C++ counterpart of .NET AppDomain.SetCachePath(string) [Obsolete].
         */
        void SetCachePath(const std::string& /*path*/) {}
    };

} // namespace System
