// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/UriParser.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <mutex>

#include "System/ArgumentNullException.hpp"
#include "System/InvalidOperationException.hpp"

namespace System {

    namespace {

        /**
         * @brief The registration table, and its lock.
         *
         * .NET's is a `static readonly Hashtable` guarded by `lock (s_table)`
         * (`UriSyntax.cs:78, 160`) -- a `Hashtable` "used instead of `Dictionary<>` for lock-free
         * reads", its own comment says. This port cannot borrow that property: `std::map` has no
         * lock-free read, so BOTH the read and the write take the mutex. The observable contract
         * is the same and the difference is in throughput alone.
         *
         * Function-local statics rather than namespace-scope ones, so there is no
         * static-initialisation-order dependency for a caller who registers from a constructor.
         */
        std::map<std::string, std::shared_ptr<UriParser>>& registry() {
            static std::map<std::string, std::shared_ptr<UriParser>> table;
            return table;
        }

        std::mutex& registryMutex() {
            static std::mutex m;
            return m;
        }

        std::string toLowerInvariant(const std::string& s) {
            std::string lower = s;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lower;
        }

        /**
         * @brief .NET's built-in registration table, by scheme name.
         *
         * Matches `UriSyntax.cs:78-96` exactly: the sixteen schemes `s_table` is initialised with.
         * `wais` is deliberately NOT here -- ticket #1998 removed it as a historical RFC 1738
         * scheme .NET never registers.
         */
        bool isBuiltInScheme(const std::string& lowerScheme) {
            static const char* known[] = {
                "http", "https", "ws", "wss", "ftp", "file", "gopher",
                "nntp", "news", "mailto", "uuid", "telnet", "ldap",
                "net.tcp", "net.pipe", "vsmacros"
            };
            for (const auto* s : known) {
                if (lowerScheme == s) return true;
            }
            return false;
        }

    } // namespace

    void UriParser::Register(const std::shared_ptr<UriParser>& uriParser,
                             const std::string& schemeName, intcs defaultPort) {
        // The order is .NET's, and it is asserted rather than assumed: null, then the
        // one-character rule, then the scheme grammar, then the port
        // (`UriScheme.cs:168-177`).
        if (!uriParser) {
            throw System::ArgumentNullException("uriParser");
        }

        // A ONE-CHARACTER SCHEME IS REJECTED HERE THOUGH IT IS A VALID SCHEME NAME. .NET writes
        // `ArgumentOutOfRangeException.ThrowIfEqual(schemeName.Length, 1)` and then ALSO calls
        // `Uri.CheckSchemeName`, which accepts a single letter -- so the two rules disagree on
        // exactly one input and the narrower one runs first. Deriving the check from
        // CheckSchemeName alone would silently accept `Register(p, "a", 80)`.
        if (schemeName.size() == 1) {
            throw System::ArgumentOutOfRangeException(
                "schemeName", "A one-character scheme name cannot have a registered parser.");
        }

        if (!Uri::CheckSchemeName(schemeName)) {
            throw System::ArgumentOutOfRangeException(
                "schemeName", "The scheme name is not a valid URI scheme.");
        }

        // .NET's test is `(uint)defaultPort > 0xFFFF && defaultPort != -1`, and the CAST IS THE
        // RULE: every negative value other than -1 becomes a very large unsigned number and is
        // rejected, so the accepted set is 0..65535 plus the single sentinel -1. A signed
        // `defaultPort < -1 || defaultPort > 0xFFFF` is the same set, written so that it is
        // legible; a naive `defaultPort > 0xFFFF` alone would accept -2.
        if ((defaultPort < 0 || defaultPort > 0xFFFF) && defaultPort != -1) {
            throw System::ArgumentOutOfRangeException(
                "defaultPort", "The default port is outside the range of valid port numbers.");
        }

        const std::string lower = toLowerInvariant(schemeName);

        // `FetchSyntax` (`UriSyntax.cs:155-181`) -- TWO DISTINCT InvalidOperationExceptions, and
        // they are two different questions: has THIS PARSER already been registered, and has THIS
        // SCHEME already been taken. Collapsing them into one message would leave a caller unable
        // to tell which of the two mistakes they made.
        std::lock_guard<std::mutex> guard(registryMutex());

        if (!uriParser->scheme_.empty()) {
            throw System::InvalidOperationException(
                "The URI parser instance passed into 'uriParser' parameter is already registered "
                "with the scheme name '" + uriParser->scheme_ + "'.");
        }

        // THE BUILT-IN SCHEMES COUNT AS TAKEN, and this line is not a refinement -- leaving it
        // out is a divergence. .NET keeps built-ins and customs in ONE table (`s_table` is
        // initialised with the sixteen at `UriSyntax.cs:78-96`), so its `oldSyntax != null` test
        // refuses `Register(p, "http", 80)` by the same statement that refuses a repeated custom
        // scheme. Splitting the two into a built-in list and a custom map -- which this port does,
        // because it has no parser objects for the built-ins -- makes it possible to check only
        // one of them, and a first cut of #1997 A-4 did exactly that and would have let a caller
        // take over `gopher`.
        if (isBuiltInScheme(lower)) {
            throw System::InvalidOperationException(
                "A URI scheme name '" + lower + "' already has a registered custom parser.");
        }

        const auto existing = registry().find(lower);
        if (existing != registry().end()) {
            throw System::InvalidOperationException(
                "A URI scheme name '" + existing->second->scheme_ +
                "' already has a registered custom parser.");
        }

        // ON_REGISTER RUNS BEFORE THE SCHEME IS STORED, which is .NET's order
        // (`UriSyntax.cs:175-176`) and is what makes the parameter load-bearing: inside the
        // callback `getSchemeNameProperty()` is still empty, so a subclass that ignored the
        // argument and read the property would see nothing.
        uriParser->OnRegister(lower, defaultPort);
        uriParser->scheme_ = lower;
        uriParser->port_ = defaultPort;

        registry()[lower] = uriParser;
    }

    bool UriParser::IsKnownScheme(const std::string& schemeName) {
        // Ticket #1998 (SR-AUD-147). This lower-cased any string and returned false, so "" and
        // "ht tp" were reported as merely UNKNOWN schemes rather than invalid arguments -- a
        // caller could not tell "I do not recognise this scheme" from "that is not a scheme".
        // .NET rejects the second outright (`UriScheme.cs:186-195`). The null check has no C++
        // counterpart: the parameter is a `const std::string&`.
        if (!Uri::CheckSchemeName(schemeName)) {
            throw System::ArgumentOutOfRangeException(
                "schemeName", "The scheme name is not a valid URI scheme.");
        }

        const std::string lower = toLowerInvariant(schemeName);
        if (isBuiltInScheme(lower)) return true;

        // A REGISTERED SCHEME MUST ANSWER TRUE, and this line is the only thing that makes
        // `Register` more than a validating no-op (#1997 A-4). .NET gets the same linkage for
        // free, both members reading the one `s_table`.
        std::lock_guard<std::mutex> guard(registryMutex());
        return registry().find(lower) != registry().end();
    }

} // namespace System
