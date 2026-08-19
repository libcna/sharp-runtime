// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Dns.hpp"
#include "System/ArgumentException.hpp"
#include "System/Net/Sockets/SocketException.hpp"
#include <array>
#include <cstring>
#include <optional>

#if defined(_WIN32)
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  if defined(_MSC_VER)
#    pragma comment(lib, "ws2_32.lib")
#  endif
#  include <mutex>
namespace {
    inline std::string netErr() {
        char buf[32];
        snprintf(buf, sizeof(buf), "WSA error %d", WSAGetLastError());
        return buf;
    }
    // Resolver failures on Windows carry a WSA code, which is what a Windows caller expects.
    inline std::string resolverErrorText(int /*rc*/) { return netErr(); }
    void wsaInit() {
        static std::once_flag f;
        std::call_once(f, [] { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); });
    }
}
#elif defined(__EMSCRIPTEN__)
#  include "System/PlatformNotSupportedException.hpp"
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <cerrno>
#  include <cstring>
namespace {
    inline void wsaInit() {}
    inline std::string netErr() { return std::strerror(errno); }
    // The resolver's OWN description of its OWN failure code. Before ticket #2039 every Dns
    // failure on POSIX surfaced as the bare Win32Exception default message "Win32 error 11001"
    // -- a Winsock error name fabricated on a platform that has no Winsock -- which told the
    // caller nothing about what actually failed. gai_strerror is the POSIX counterpart, and it
    // is the same helper modules/net-sockets' TcpClient/UdpClient/Socket already use.
    inline std::string resolverErrorText(int rc) { return ::gai_strerror(rc); }
}
#endif

namespace System::Net {

    using System::Net::Sockets::AddressFamily;
    using System::Net::Sockets::SocketException;
    using System::Net::Sockets::SocketError;
    using SharpRuntime::intcs;

#if !defined(__EMSCRIPTEN__)
    namespace {
        // ONE literal parser for the whole module.
        //
        // This used to be two: a bespoke sscanf("%u.%u.%u.%u%c") for IPv4 beside
        // IPAddress::TryParse for IPv6. The duplicate disagreed with the real parser about
        // valid input, not merely invalid input (measured, build-probe/2039_probe1_before.log):
        //
        //   "-0.0.0.1"    accepted as 0.0.0.1   -- %u takes a sign; IPAddress::TryParse rejects
        //   "+1.2.3.4"    accepted as 1.2.3.4   -- likewise, and not named by the finding
        //   " 1.2.3.4"    accepted as 1.2.3.4   -- %u skips leading whitespace
        //   "0177.0.0.1"  read as 177.0.0.1     -- DECIMAL, while IPAddress::Parse of the same
        //                                          text yields 127.0.0.1, reading it as OCTAL
        //   "1.2.3"       declined entirely     -- only 3 conversions, so it fell through to
        //                                          the resolver
        //
        // The last two are the sharp ones: one module returned two different addresses for one
        // valid literal depending on which door was used. Deleting the duplicate is the repair
        // (ticket #2039, SR-AUD-304) -- IPAddress::TryParse is this module's own tested parser,
        // and no external reference is needed to prefer it.
        std::optional<IPAddress> tryParseIPLiteral(const std::string& s) {
            IPAddress address;
            if (!IPAddress::TryParse(s, address)) return std::nullopt;
            return address;
        }

        // A literal answers a request only when no particular family was asked for, or when the
        // family asked for is its own. Before ticket #2039 the two literal shortcuts were guarded
        // by `family != InterNetworkV6` / `family != InterNetwork`, so EVERY other value --
        // Unix, Unknown, Max -- passed both guards and got the literal back regardless.
        bool literalSatisfiesFamily(AddressFamily requested, const IPAddress& literal) {
            return requested == AddressFamily::Unspecified ||
                   requested == literal.getAddressFamilyProperty();
        }

        IPAddress fromSockaddrIn(const sockaddr_in& sin) {
            uint32_t hostOrder = ntohl(sin.sin_addr.s_addr);
            return IPAddress(hostOrder);
        }

        IPAddress fromSockaddrIn6(const sockaddr_in6& sin6) {
            std::array<bytecs, 16> bytes{};
            std::memcpy(bytes.data(), sin6.sin6_addr.s6_addr, bytes.size());
            return IPAddress(bytes, static_cast<longcs>(sin6.sin6_scope_id));
        }

        // Whether the resolver can serve a request for this family at all.
        //
        // #2046. This used to be absent, and `addressFamilyToAiFamily` mapped EVERY value other
        // than the two IP families to AF_UNSPEC -- so a NAME resolved normally under
        // `AddressFamily::Unix` while the same request against a LITERAL was refused. One
        // function answered the same question two ways depending on its argument's shape.
        bool resolverCanServeFamily(AddressFamily family) {
            return family == AddressFamily::Unspecified
                || family == AddressFamily::InterNetwork
                || family == AddressFamily::InterNetworkV6;
        }

        // Maps a requested AddressFamily to the getaddrinfo() hint that resolves it: IPv4-only,
        // IPv6-only, or (for Unspecified) both families via AF_UNSPEC.
        //
        // Only ever called for a family `resolverCanServeFamily` accepts.
        int addressFamilyToAiFamily(AddressFamily family) {
            if (family == AddressFamily::InterNetwork) return AF_INET;
            if (family == AddressFamily::InterNetworkV6) return AF_INET6;
            return AF_UNSPEC;
        }

        // Fills the getaddrinfo hints.
        //
        // ai_socktype was left at 0, which asks getaddrinfo for one addrinfo PER SOCKET TYPE --
        // so every answer came back THREE times, once for SOCK_STREAM, SOCK_DGRAM and SOCK_RAW.
        // That, not the duplicate literal parser, is where SR-AUD-304's duplicate results came
        // from: measured directly against getaddrinfo (build-probe/2039_probe1_before.log §E),
        // ai_socktype = 0 returns 3 entries for "localhost", "127.0.0.1" and "1.2.3" alike,
        // while SOCK_STREAM returns exactly 1. It affected every resolved NAME too, not just the
        // one literal the finding names, and GetHostEntry(8.8.8.8) returned SIX entries for two
        // distinct addresses. SOCK_STREAM is what this repository's own TcpClient, UdpClient and
        // Socket already pass; with no service name it constrains the returned socktype, not the
        // set of addresses.
        void fillResolverHints(struct addrinfo& hints, AddressFamily family, bool wantCanonicalName) {
            hints.ai_family = addressFamilyToAiFamily(family);
            hints.ai_socktype = SOCK_STREAM;
            if (wantCanonicalName) hints.ai_flags = AI_CANONNAME;
        }

        // Collects a getaddrinfo result list, in the resolver's own order, with duplicates
        // removed.
        //
        // The equality used is System::Net::IPAddress::operator== -- BINARY address equality:
        // address family, all address bits, and (for IPv6) the scope id. Deliberately not
        // textual equality and deliberately not canonicalising: 1.2.3.4 and ::ffff:1.2.3.4 are
        // DIFFERENT families and both are kept, as are fe80::1%7 and fe80::1%9, which name
        // different links. Only an address already present is dropped.
        //
        // Ordering is the resolver's, by first occurrence -- getaddrinfo has already applied
        // RFC 6724 destination-address selection, and reordering it would silently override the
        // system's own preference.
        std::vector<IPAddress> collectAddresses(const struct addrinfo* res) {
            std::vector<IPAddress> addresses;
            for (const struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
                std::optional<IPAddress> address;
                if (p->ai_family == AF_INET) {
                    address = fromSockaddrIn(*reinterpret_cast<const sockaddr_in*>(p->ai_addr));
                } else if (p->ai_family == AF_INET6) {
                    address = fromSockaddrIn6(*reinterpret_cast<const sockaddr_in6*>(p->ai_addr));
                }
                if (!address) continue;
                bool alreadyPresent = false;
                for (const IPAddress& seen : addresses) {
                    if (seen == *address) { alreadyPresent = true; break; }
                }
                if (!alreadyPresent) addresses.push_back(*address);
            }
            return addresses;
        }

        // Every Dns resolver failure raises the same exception TYPE and the same SocketError as
        // before -- SocketException / HostNotFound / native code 11001 -- and only the message
        // changes, from the fabricated "Win32 error 11001" to one naming the operation, the host
        // and the platform resolver's own description of its own failure.
        [[noreturn]] void throwResolutionFailed(const std::string& host, int rc) {
            throw SocketException(SocketError::HostNotFound,
                                  "Dns: could not resolve host '" + host + "': " + resolverErrorText(rc));
        }

        std::string addressFamilyName(AddressFamily family) {
            switch (family) {
                case AddressFamily::Unknown:        return "Unknown";
                case AddressFamily::Unspecified:    return "Unspecified";
                case AddressFamily::Unix:           return "Unix";
                case AddressFamily::InterNetwork:   return "InterNetwork";
                case AddressFamily::InterNetworkV6: return "InterNetworkV6";
                case AddressFamily::Max:            return "Max";
                default:                            return std::to_string(static_cast<int>(family));
            }
        }

        // A literal that exists but is of the wrong family has no valid answer for the request.
        //
        // #2039 made this raise SocketException(HostNotFound) rather than returning an empty
        // vector, on the ground that an empty vector is "indistinguishable from 'checked and
        // found nothing'", and recorded that the plan's prediction of an empty result "was made
        // without those tests in view and is corrected ... rather than followed".
        //
        // #2046 REVERSES THAT, because the reference settles it and #2039 did not have the
        // reference. Dns.cs:213 is, verbatim:
        //
        //     addresses = (family == AddressFamily.Unspecified || address.AddressFamily == family)
        //         ? new IPAddress[] { address } : Array.Empty<IPAddress>();
        //
        // -- an EMPTY ARRAY, no exception. So the plan's prediction was right and its
        // "correction" was the error. GetHostAddresses now returns an empty vector;
        // GetHostEntry cannot, because it returns one entry rather than a list, and .NET's own
        // GetHostEntry does not short-circuit a literal at all (see its own note below).
        /**
         * @brief Rejects the two unspecified ("wildcard") addresses, as .NET does.
         *
         * Ticket #2043 (SR-AUD-304's wildcard half). `GetHostAddresses("0.0.0.0")` returned the
         * wildcard address, and the ticket was split out of #2039 precisely because this is the
         * one half that **removes a working, meaningful result** -- so it needed evidence rather
         * than judgement. The reference supplies it: `Dns.cs:686-690` in the string path and
         * `:46-50`, `:158-162` in the `IPAddress` overloads all raise
         * `ArgumentException(SR.net_invalid_ip_addr)` for `IPAddress.Any` or `IPAddress.IPv6Any`.
         *
         * .NET's message says why, and it is transcribed rather than paraphrased: these are
         * *unspecified* addresses. They name "every local interface" to `bind`, and nothing at
         * all to `connect` -- so resolving one to itself hands a caller a target it cannot use.
         *
         * @param parameterName `"hostName"` on the string path and `"address"` on the
         *        `IPAddress` one, matching .NET's own `nameof` at each site.
         */
        void throwIfUnspecifiedAddress(const IPAddress& address, const char* parameterName) {
            if (address == IPAddress::Any || address == IPAddress::IPv6Any) {
                throw System::ArgumentException(
                    "IPv4 address 0.0.0.0 and IPv6 address ::0 are unspecified addresses that "
                    "cannot be used as a target address.",
                    parameterName);
            }
        }

        [[noreturn]] void throwNoAddressOfRequestedFamily(const std::string& host, AddressFamily family) {
            throw SocketException(SocketError::HostNotFound,
                                  "Dns: host '" + host + "' has no address in the requested address family '" +
                                      addressFamilyName(family) + "'.");
        }

        /**
         * @brief Refuses a family this resolver cannot serve, with .NET's own error code.
         *
         * #2046. .NET reaches `SocketError.AddressFamilyNotSupported` here by TWO routes and both
         * end in the same place, which is why this port may take the shorter one:
         *
         *  - a family its native converter does not recognise fails
         *    `TryConvertAddressFamilyPalToPlatform` and returns `EAI_FAMILY` outright
         *    (`pal_networking.c`, `SystemNative_GetHostEntryForName`);
         *  - a family it DOES convert but that `getaddrinfo` will not accept -- `AF_UNIX` is the
         *    case that matters -- returns `EAI_FAMILY` from the C library. Measured directly in
         *    this container: `getaddrinfo` with an `ai_family` of `AF_UNIX`, `AF_PACKET` or `99`
         *    all return -6, `EAI_FAMILY`, "ai_family not supported".
         *
         * and `NameResolutionPal.Unix.cs:41-42` maps `EAI_FAMILY` to
         * `SocketError.AddressFamilyNotSupported`.
         */
        [[noreturn]] void throwFamilyNotSupportedByResolver(const std::string& host,
                                                            AddressFamily      family) {
            throw SocketException(SocketError::AddressFamilyNotSupported,
                                  "Dns: the address family '" + addressFamilyName(family) +
                                      "' requested for host '" + host +
                                      "' cannot be resolved by this runtime's resolver.");
        }
    }
#endif

    std::string Dns::GetHostName() {
#if defined(__EMSCRIPTEN__)
        throw System::PlatformNotSupportedException("Dns.GetHostName is not supported on Emscripten.");
#else
        wsaInit();
        char buf[256] = {};
        if (::gethostname(buf, sizeof(buf) - 1) != 0) {
            // Same SocketError as before; only the message stops fabricating "Win32 error -1"
            // on a platform with no Win32 (ticket #2039).
            throw SocketException(SocketError::SocketError,
                                  "Dns::GetHostName: gethostname failed: " + netErr());
        }
        return std::string(buf);
#endif
    }

    std::vector<IPAddress> Dns::GetHostAddresses(const std::string& hostNameOrAddress) {
        return GetHostAddresses(hostNameOrAddress, AddressFamily::Unspecified);
    }

    std::vector<IPAddress> Dns::GetHostAddresses(const std::string& hostNameOrAddress, AddressFamily family) {
#if defined(__EMSCRIPTEN__)
        (void)hostNameOrAddress;
        (void)family;
        throw System::PlatformNotSupportedException("Dns.GetHostAddresses is not supported on Emscripten.");
#else
        if (auto literal = tryParseIPLiteral(hostNameOrAddress)) {
            // #2043: BEFORE the family check, matching Dns.cs:686-690, which tests the wildcard
            // immediately after IPAddress.TryParse and before anything else looks at the value.
            throwIfUnspecifiedAddress(*literal, "hostNameOrAddress");
            // #2046: an EMPTY vector, not an exception -- Dns.cs:213. See the note on
            // literalSatisfiesFamily for why this reverses #2039.
            if (!literalSatisfiesFamily(family, *literal)) return {};
            return { *literal };
        }

        // #2046: the family gate the NAME path never had. Without it every family other than the
        // two IP ones became AF_UNSPEC and the name resolved as though none had been asked for.
        if (!resolverCanServeFamily(family)) {
            throwFamilyNotSupportedByResolver(hostNameOrAddress, family);
        }

        wsaInit();
        struct addrinfo hints {};
        fillResolverHints(hints, family, /*wantCanonicalName=*/false);
        struct addrinfo* res = nullptr;
        int rc = ::getaddrinfo(hostNameOrAddress.c_str(), nullptr, &hints, &res);
        if (rc != 0) {
            throwResolutionFailed(hostNameOrAddress, rc);
        }

        std::vector<IPAddress> result = collectAddresses(res);
        ::freeaddrinfo(res);
        return result;
#endif
    }

    IPHostEntry Dns::GetHostEntry(const std::string& hostNameOrAddress) {
        return GetHostEntry(hostNameOrAddress, AddressFamily::Unspecified);
    }

    IPHostEntry Dns::GetHostEntry(const std::string& hostNameOrAddress, AddressFamily family) {
#if defined(__EMSCRIPTEN__)
        (void)hostNameOrAddress;
        (void)family;
        throw System::PlatformNotSupportedException("Dns.GetHostEntry is not supported on Emscripten.");
#else
        if (auto literal = tryParseIPLiteral(hostNameOrAddress)) {
            // #2046. .NET's GetHostEntry does NOT short-circuit a literal: it reverse-resolves
            // the address to a name and then forward-resolves that name WITH the family
            // (Dns.cs:290-320), so a family the resolver cannot serve reaches the same
            // EAI_FAMILY it would for a name. This port keeps the short-circuit -- reproducing
            // .NET's route means a reverse DNS lookup on every literal, a network round trip
            // this door has never made -- and reaches .NET's OUTCOME directly.
            if (!resolverCanServeFamily(family)) {
                throwFamilyNotSupportedByResolver(hostNameOrAddress, family);
            }
            // A mismatch between the two IP families keeps HostNotFound. This is a STATED
            // residual difference rather than parity: .NET would forward-resolve the
            // reverse-resolved name and could legitimately find an address of the requested
            // family, where this port answers from the literal alone.
            if (!literalSatisfiesFamily(family, *literal)) {
                throwNoAddressOfRequestedFamily(hostNameOrAddress, family);
            }
            return GetHostEntry(*literal);
        }

        if (!resolverCanServeFamily(family)) {
            throwFamilyNotSupportedByResolver(hostNameOrAddress, family);
        }

        wsaInit();
        struct addrinfo hints {};
        fillResolverHints(hints, family, /*wantCanonicalName=*/true);
        struct addrinfo* res = nullptr;
        int rc = ::getaddrinfo(hostNameOrAddress.c_str(), nullptr, &hints, &res);
        if (rc != 0) {
            throwResolutionFailed(hostNameOrAddress, rc);
        }

        IPHostEntry entry;
        std::string canonicalName = hostNameOrAddress;
        if (res != nullptr && res->ai_canonname != nullptr) {
            canonicalName = res->ai_canonname;
        }
        entry.setHostNameProperty(canonicalName);
        entry.setAddressListProperty(collectAddresses(res));
        ::freeaddrinfo(res);
        return entry;
#endif
    }

    IPHostEntry Dns::GetHostEntry(const IPAddress& address) {
#if defined(__EMSCRIPTEN__)
        (void)address;
        throw System::PlatformNotSupportedException("Dns.GetHostEntry is not supported on Emscripten.");
#else
        wsaInit();
        char hostBuf[NI_MAXHOST] = {};
        int rc;

        if (address.getIsIPv6Property()) {
            sockaddr_in6 sin6{};
            sin6.sin6_family = AF_INET6;
            sin6.sin6_port = 0;
            sin6.sin6_scope_id = static_cast<uint32_t>(address.getScopeIdProperty());
            std::vector<bytecs> bytes = address.GetAddressBytes();
            std::memcpy(sin6.sin6_addr.s6_addr, bytes.data(), bytes.size());
            rc = ::getnameinfo(reinterpret_cast<sockaddr*>(&sin6), sizeof(sin6), hostBuf, sizeof(hostBuf), nullptr, 0, 0);
        } else {
            sockaddr_in sin{};
            sin.sin_family = AF_INET;
            sin.sin_port = 0;
            sin.sin_addr.s_addr = htonl(address.getAddressProperty());
            rc = ::getnameinfo(reinterpret_cast<sockaddr*>(&sin), sizeof(sin), hostBuf, sizeof(hostBuf), nullptr, 0, 0);
        }
        if (rc != 0) {
            // Same exception type and SocketError as before; the message now carries
            // getnameinfo's own description instead of the fabricated "Win32 error 11001".
            throw SocketException(SocketError::HostNotFound,
                                  "Dns: reverse lookup failed for '" + address.ToString() +
                                      "': " + resolverErrorText(rc));
        }

        // getnameinfo() without NI_NAMEREQD does NOT fail when the address has no reverse
        // mapping: it succeeds and writes the address back in its NUMERIC form. Handing that
        // text to the string overload re-parses it as a literal, which sends it straight back
        // into this function with the same address -- unbounded mutual recursion, stack
        // overflow, SIGSEGV, reachable from ordinary public input on any host that simply has
        // no PTR record or hosts entry for the queried address.
        //
        // Measured: on a container whose /etc/hosts has `127.0.0.1 localhost` but no `::1`
        // line, getnameinfo(127.0.0.1) returns "localhost" and terminates, while
        // getnameinfo(::1) returns "::1" and recurses until the process dies. That is why the
        // IPv4 case looked healthy and only DnsTests.GetHostEntry_LiteralIPv6_* crashed
        // (ticket #1961).
        //
        // When the reverse lookup yields a literal rather than a name there is nothing further
        // to resolve, so the entry is built directly: the host name is the address text, and
        // the address list is the address that was asked about.
        //
        // The literal test now goes through the module's single parser too. Note that this makes
        // the #1961 guard STRICTER, not weaker: the old pair could miss a literal the resolver
        // wrote back in a spelling the sscanf parser declined (a short form such as "1.2.3", or
        // an octal or hexadecimal octet), and a missed literal is exactly the input that
        // re-entered this function.
        const std::string resolvedHost(hostBuf);
        const bool resolvedToALiteral = tryParseIPLiteral(resolvedHost).has_value();
        if (resolvedToALiteral) {
            IPHostEntry entry;
            entry.setHostNameProperty(resolvedHost);
            entry.setAddressListProperty(std::vector<IPAddress>{address});
            return entry;
        }

        AddressFamily resolvedFamily = address.getIsIPv6Property()
            ? AddressFamily::InterNetworkV6 : AddressFamily::InterNetwork;
        return GetHostEntry(resolvedHost, resolvedFamily);
#endif
    }

} // namespace System::Net
