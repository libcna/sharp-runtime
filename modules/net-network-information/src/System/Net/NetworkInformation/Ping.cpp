// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/NetworkInformation/Ping.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IndexOutOfRangeException.hpp"
#include "System/Net/Dns.hpp"
#include "System/Net/NetworkInformation/NetworkInformationException.hpp"
#include "System/Net/NetworkInformation/PingException.hpp"
#include "System/PlatformNotSupportedException.hpp"
#include <exception>

#if defined(_WIN32)
// No Windows PAL implemented yet.
#elif defined(__EMSCRIPTEN__)
// No raw/ping-socket support available under Emscripten.
#else
#define SHARP_RUNTIME_PING_POSIX 1
#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <netinet/icmp6.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <unistd.h>

#if defined(__linux__)
#define SHARP_RUNTIME_PING_LINUX_ICMP 1
#else
// BSD/Darwin's <netinet/ip_icmp.h> ships a genuinely different ICMPv4 ABI from Linux's: the
// header struct is named `struct icmp` (not `icmphdr`), and its type/code/checksum/id/sequence
// fields are reached as `icmp_type`/`icmp_code`/`icmp_cksum`/`icmp_id`/`icmp_seq` (the last two
// via the system header's own convenience macros over a nested union), not
// `type`/`code`/`checksum`/`un.echo.id`/`un.echo.sequence` like Linux's `icmphdr`. The
// Destination-Unreachable/Time-Exceeded sub-code constants this file's own mapIcmpV4Status()
// switch already names are aliased onto BSD's OWN real macros (ICMP_UNREACH/ICMP_UNREACH_NET/
// etc.) below -- deliberately not hand-typed numeric values, so the actual RFC 792 type/code
// numbers always come from the system's own header, never guessed. ICMP_ECHOREPLY and ICMP_ECHO
// need no alias -- both platforms already use those exact names for the same RFC 792 values.
#define ICMP_DEST_UNREACH   ICMP_UNREACH
#define ICMP_NET_UNREACH    ICMP_UNREACH_NET
#define ICMP_HOST_UNREACH   ICMP_UNREACH_HOST
#define ICMP_PROT_UNREACH   ICMP_UNREACH_PROTOCOL
#define ICMP_PORT_UNREACH   ICMP_UNREACH_PORT
#define ICMP_TIME_EXCEEDED  ICMP_TIMXCEED
#endif
#endif

namespace System::Net::NetworkInformation {

std::vector<SharpRuntime::bytecs> Ping::defaultSendBuffer() {
    std::vector<SharpRuntime::bytecs> buffer(DefaultSendBufferSize);
    for (SharpRuntime::intcs i = 0; i < DefaultSendBufferSize; ++i) {
        buffer[static_cast<size_t>(i)] = static_cast<SharpRuntime::bytecs>('a' + (i % 23));
    }
    return buffer;
}

void Ping::checkArgs(SharpRuntime::intcs timeout, const std::vector<SharpRuntime::bytecs>& buffer) {
    if (buffer.size() > static_cast<size_t>(MaxBufferSize)) {
        throw System::ArgumentException("The data buffer is too long. It must be 65,500 bytes or less.", "buffer");
    }
    System::ArgumentOutOfRangeException::ThrowIfNegative(timeout, "timeout");
}

void Ping::checkArgs(const System::Net::IPAddress& address, SharpRuntime::intcs timeout,
                      const std::vector<SharpRuntime::bytecs>& buffer) {
    checkArgs(timeout, buffer);
    if (address == System::Net::IPAddress::Any || address == System::Net::IPAddress::IPv6Any) {
        throw System::ArgumentException("An invalid IP address was specified.", "address");
    }
}

#if defined(SHARP_RUNTIME_PING_POSIX)
namespace {

    // Real struct name differs (Linux: `icmphdr`, BSD/Darwin: `icmp`) -- see the platform-detect
    // block above the includes for why. This alias lets sizeof(IcmpV4Header)/pointer-cast call
    // sites stay identical on both platforms; the few field-name-dependent lines (construction,
    // parsing) are still written per-platform explicitly, deliberately not hidden behind further
    // macros, so they stay easy to read and verify against each platform's real header.
#if defined(SHARP_RUNTIME_PING_LINUX_ICMP)
    using IcmpV4Header = ::icmphdr;
#else
    using IcmpV4Header = ::icmp;
#endif

    uint16_t internetChecksum(const void* data, size_t len) {
        const auto* p = static_cast<const uint8_t*>(data);
        uint32_t sum = 0;
        while (len > 1) {
            sum += (static_cast<uint32_t>(p[0]) << 8) | p[1];
            p += 2;
            len -= 2;
        }
        if (len == 1) {
            sum += static_cast<uint32_t>(p[0]) << 8;
        }
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        return static_cast<uint16_t>(~sum);
    }

    IPStatus mapIcmpV4Status(uint8_t type, uint8_t code) {
        switch (type) {
            case ICMP_ECHOREPLY:
                return IPStatus::Success;
            case ICMP_DEST_UNREACH:
                switch (code) {
                    case ICMP_NET_UNREACH:
                        return IPStatus::DestinationNetworkUnreachable;
                    case ICMP_HOST_UNREACH:
                        return IPStatus::DestinationHostUnreachable;
                    case ICMP_PROT_UNREACH:
                        return IPStatus::DestinationProtocolUnreachable;
                    case ICMP_PORT_UNREACH:
                        return IPStatus::DestinationPortUnreachable;
                    default:
                        return IPStatus::DestinationUnreachable;
                }
            case ICMP_TIME_EXCEEDED:
                return IPStatus::TtlExpired;
            default:
                return IPStatus::Unknown;
        }
    }

    IPStatus mapIcmpV6Status(uint8_t type) {
        switch (type) {
            case ICMP6_ECHO_REPLY:
                return IPStatus::Success;
            case ICMP6_DST_UNREACH:
                return IPStatus::DestinationUnreachable;
            case ICMP6_TIME_EXCEEDED:
                return IPStatus::TtlExpired;
            default:
                return IPStatus::Unknown;
        }
    }

    /**
     * Owns an open descriptor for the rest of the enclosing scope -- ticket #2193.
     *
     * `sendPingCore` used to hold a bare `int` and call `::close` by hand on each of its four
     * exits, but between the socket call and those exits it resizes a packet vector of up to
     * 65508 bytes, allocates a 65535-byte receive buffer, calls `GetAddressBytes()`, assigns a
     * reply buffer and constructs a `PingReply`. An exception from any of those leaked the
     * descriptor. Non-copyable and non-movable on purpose: there is exactly one owner and it never
     * changes hands.
     */
    class OwnedDescriptor {
        int fd_;

    public:
        explicit OwnedDescriptor(int fd) noexcept : fd_(fd) {}
        OwnedDescriptor(const OwnedDescriptor&) = delete;
        OwnedDescriptor& operator=(const OwnedDescriptor&) = delete;
        ~OwnedDescriptor() {
            if (fd_ >= 0) {
                ::close(fd_);
            }
        }

        [[nodiscard]] int get() const noexcept { return fd_; }
    };

    OwnedDescriptor createIcmpSocket(bool isIPv6) {
        int family = isIPv6 ? static_cast<int>(AF_INET6) : static_cast<int>(AF_INET);
        int protocol = isIPv6 ? static_cast<int>(IPPROTO_ICMPV6) : static_cast<int>(IPPROTO_ICMP);
        int fd = ::socket(family, SOCK_DGRAM, protocol);
        if (fd < 0) {
            throw NetworkInformationException();
        }
        return OwnedDescriptor(fd);
    }

    /**
     * @brief Applies a socket option, reporting a kernel refusal instead of discarding it.
     *
     * Ticket #2194. Every `setsockopt` here discarded its return value, so an option the kernel
     * rejected was silently not applied -- a caller who set `Ttl = 1` expecting a
     * `TtlExpired` reply got a normal one and no indication that the option had been dropped.
     */
    void setOptionOrThrow(int fd, int level, int name, const void* value, socklen_t size) {
        if (::setsockopt(fd, level, name, value, size) != 0) {
            throw NetworkInformationException(errno);
        }
    }

    /**
     * @brief Whether @p data is a reply that correlates with the request we just sent.
     *
     * HONEST NOTE: a mutation that RESTARTS the timeout on every foreign datagram (rather than
     * keeping the original deadline, as the call site does) is NOT caught. Observing it needs a
     * SUSTAINED flood of foreign datagrams plus a wall-clock assertion, and under the mutation
     * the call would not fail but HANG -- so the test would be both flaky and useless, the
     * shape #2352 and #2105 were repaired for. The original deadline is kept because a busy or
     * hostile socket must not be able to extend this call without bound, which is the same
     * class of defect #2032 removed from WaitForExit.
     *
     * Ticket #2194. The correlation is the SEQUENCE NUMBER -- see the long note at the call site
     * for why the identifier cannot be used on a ping socket and why the source address must not
     * be. An ICMP ERROR reply quotes the original request after its own header, so the sequence
     * is read from the quoted request rather than from the error's own header, which carries
     * none: that is the same "original IP+ICMP request is in the payload" rule .NET follows
     * (`Ping.RawSocket.cs:166-190`).
     */
    bool replyMatchesRequest(const uint8_t* data, size_t size, bool isIPv6, uint16_t sequence) {
        if (isIPv6) {
            if (size < sizeof(icmp6_hdr)) return false;
            icmp6_hdr hdr{};
            std::memcpy(&hdr, data, sizeof(hdr));
            if (hdr.icmp6_type == ICMP6_ECHO_REPLY) return ntohs(hdr.icmp6_seq) == sequence;
            // An error quotes the IPv6 header (40 bytes) then the original ICMPv6 header.
            const size_t quoted = sizeof(icmp6_hdr) + 40;
            if (size < quoted + sizeof(icmp6_hdr)) return false;
            icmp6_hdr original{};
            std::memcpy(&original, data + quoted, sizeof(original));
            return ntohs(original.icmp6_seq) == sequence;
        }
        if (size < sizeof(IcmpV4Header)) return false;
        IcmpV4Header hdr{};
        std::memcpy(&hdr, data, sizeof(hdr));
#if defined(SHARP_RUNTIME_PING_LINUX_ICMP)
        const uint8_t type = hdr.type;
        if (type == ICMP_ECHOREPLY) return ntohs(hdr.un.echo.sequence) == sequence;
#else
        const uint8_t type = hdr.icmp_type;
        if (type == ICMP_ECHOREPLY) return ntohs(hdr.icmp_seq) == sequence;
#endif
        // An error quotes the original IP header, whose length is in its low nibble, then the
        // original ICMP header.
        if (size < sizeof(IcmpV4Header) + 1) return false;
        const size_t quotedIpLength = 4u * static_cast<size_t>(data[sizeof(IcmpV4Header)] & 0x0F);
        const size_t quoted = sizeof(IcmpV4Header) + quotedIpLength;
        if (quotedIpLength < 20 || size < quoted + sizeof(IcmpV4Header)) return false;
        IcmpV4Header original{};
        std::memcpy(&original, data + quoted, sizeof(original));
#if defined(SHARP_RUNTIME_PING_LINUX_ICMP)
        return ntohs(original.un.echo.sequence) == sequence;
#else
        return ntohs(original.icmp_seq) == sequence;
#endif
    }

    void applyOptions(int fd, bool isIPv6, const PingOptions* options) {
        if (options == nullptr) {
            return;
        }
        int ttl = static_cast<int>(options->getTtlProperty());
        if (isIPv6) {
            setOptionOrThrow(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl));
        } else {
            setOptionOrThrow(fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
#if defined(IP_MTU_DISCOVER)
            int mode = options->getDontFragmentProperty() ? IP_PMTUDISC_DO : IP_PMTUDISC_WANT;
            setOptionOrThrow(fd, IPPROTO_IP, IP_MTU_DISCOVER, &mode, sizeof(mode));
#endif
        }
    }

} // namespace

PingReply Ping::sendPingCore(const System::Net::IPAddress& address, const std::vector<SharpRuntime::bytecs>& buffer,
                               SharpRuntime::intcs timeout, const PingOptions* options) {
    bool isIPv6 = address.getIsIPv6Property();
    // Ticket #2193: the descriptor is owned for the rest of this function, so every exit --
    // including an exception thrown by one of the allocations below -- releases it.
    OwnedDescriptor socket = createIcmpSocket(isIPv6);
    const int fd = socket.get();
    applyOptions(fd, isIPv6, options);

    timeval tv{};
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setOptionOrThrow(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    static std::atomic<uint16_t> sequenceCounter{0};
    uint16_t identifier = static_cast<uint16_t>(::getpid() & 0xFFFF);
    uint16_t sequence = ++sequenceCounter;

    std::vector<uint8_t> packet;
    if (isIPv6) {
        packet.resize(sizeof(icmp6_hdr) + buffer.size());
        // Build the header in a local, properly-typed object rather than reinterpret_cast'ing
        // packet.data() (a uint8_t*) to icmp6_hdr* and memset'ing through it -- GCC 14's
        // -Warray-bounds (enabled by -Werror in Release) cannot prove the vector's dynamic
        // storage is large enough through that cast and flags it as an out-of-bounds write.
        icmp6_hdr hdr{};
        hdr.icmp6_type = ICMP6_ECHO_REQUEST;
        hdr.icmp6_code = 0;
        hdr.icmp6_id = htons(identifier);
        hdr.icmp6_seq = htons(sequence);
        std::memcpy(packet.data(), &hdr, sizeof(hdr));
        if (!buffer.empty()) {
            std::memcpy(packet.data() + sizeof(icmp6_hdr), buffer.data(), buffer.size());
        }
    } else {
        packet.resize(sizeof(IcmpV4Header) + buffer.size());
        // Same rationale as the icmp6_hdr branch above.
        IcmpV4Header hdr{};
#if defined(SHARP_RUNTIME_PING_LINUX_ICMP)
        hdr.type = ICMP_ECHO;
        hdr.code = 0;
        hdr.checksum = 0;
        hdr.un.echo.id = htons(identifier);
        hdr.un.echo.sequence = htons(sequence);
#else
        // BSD/Darwin `struct icmp`: icmp_id/icmp_seq are the system header's own convenience
        // macros over a nested union (icmp_hun.ih_idseq.icd_id/icd_seq) -- assigning through them
        // directly, exactly like real BSD ping(8)/traceroute(8) implementations do, rather than
        // reaching into the union by hand.
        hdr.icmp_type = ICMP_ECHO;
        hdr.icmp_code = 0;
        hdr.icmp_cksum = 0;
        hdr.icmp_id = htons(identifier);
        hdr.icmp_seq = htons(sequence);
#endif
        std::memcpy(packet.data(), &hdr, sizeof(hdr));
        if (!buffer.empty()) {
            std::memcpy(packet.data() + sizeof(IcmpV4Header), buffer.data(), buffer.size());
        }
        // The checksum covers the whole packet (header + payload), so it can only be computed
        // once the payload has been copied in; patch it into both the local header and the
        // packet buffer afterward.
#if defined(SHARP_RUNTIME_PING_LINUX_ICMP)
        hdr.checksum = htons(internetChecksum(packet.data(), packet.size()));
#else
        hdr.icmp_cksum = htons(internetChecksum(packet.data(), packet.size()));
#endif
        std::memcpy(packet.data(), &hdr, sizeof(hdr));
    }

    sockaddr_storage dest{};
    socklen_t destLen;
    if (isIPv6) {
        auto* dst6 = reinterpret_cast<sockaddr_in6*>(&dest);
        dst6->sin6_family = AF_INET6;
        auto bytes = address.GetAddressBytes();
        std::memcpy(&dst6->sin6_addr, bytes.data(), bytes.size());
        destLen = sizeof(sockaddr_in6);
    } else {
        auto* dst4 = reinterpret_cast<sockaddr_in*>(&dest);
        dst4->sin_family = AF_INET;
        dst4->sin_addr.s_addr = htonl(address.getAddressProperty());
        destLen = sizeof(sockaddr_in);
    }

    auto start = std::chrono::steady_clock::now();
    ssize_t sent = ::sendto(fd, packet.data(), packet.size(), 0, reinterpret_cast<sockaddr*>(&dest), destLen);
    if (sent < 0) {
        throw NetworkInformationException(errno);
    }

    // #2194: read until a datagram that CORRELATES WITH THIS REQUEST arrives, or the deadline
    // passes. The old code took the first datagram on the socket unconditionally, so a reply to
    // somebody else's outstanding request -- or a stale one of our own -- was reported as the
    // answer to this one, with its status and its round-trip time.
    //
    // THE TICKET'S ACCEPTANCE CRITERION IS WRONG ON TWO OF ITS THREE FIELDS, and both are
    // measured rather than argued:
    //
    //  * IDENTIFIER -- cannot be matched here. This runtime opens a SOCK_DGRAM/IPPROTO_ICMP
    //    "ping socket", and the Linux kernel REWRITES the ICMP identifier: probed directly, a
    //    request written with id 0x1234 came back as 0x94d4 (the kernel uses the socket's own
    //    port). .NET checks the identifier (`Ping.RawSocket.cs:230`) because its raw-socket path
    //    writes an id the kernel leaves alone; on a ping socket that check would reject every
    //    reply.
    //  * SOURCE ADDRESS -- deliberately NOT matched, and .NET does not match it either: it
    //    reports `socketConfig.EndPoint.Address`, the address it SENT to (`:245`). Requiring the
    //    source to equal the destination would reject the legitimate error replies that come
    //    from an intermediate router -- TimeExceeded above all, which is exactly what a caller
    //    setting a low Ttl is asking for.
    //  * SEQUENCE -- matched. It survives the ping socket unchanged (probed: 0x5678 out, 0x5678
    //    back), it is ours, and it is the field that distinguishes this request from another.
    std::vector<uint8_t> recvBuf(65535);
    const auto deadline = start + std::chrono::milliseconds(timeout);
    ssize_t received = -1;
    for (;;) {
        received = ::recv(fd, recvBuf.data(), recvBuf.size(), 0);
        if (received < 0) break;
        if (replyMatchesRequest(recvBuf.data(), static_cast<size_t>(received), isIPv6, sequence))
            break;
        // Not ours. Keep the ORIGINAL deadline rather than restarting the timeout, so a stream
        // of foreign datagrams cannot extend this call without bound.
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            received = -1;
            errno = EAGAIN;
            break;
        }
        const auto remaining =
            std::chrono::duration_cast<std::chrono::microseconds>(deadline - now).count();
        timeval rest{};
        rest.tv_sec = static_cast<time_t>(remaining / 1000000);
        rest.tv_usec = static_cast<suseconds_t>(remaining % 1000000);
        setOptionOrThrow(fd, SOL_SOCKET, SO_RCVTIMEO, &rest, sizeof(rest));
    }
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    // The descriptor stays owned until this function returns (ticket #2193). The `::close(fd)`
    // that used to sit here also ran BEFORE the `errno` reads below, and `close()` is allowed to
    // set `errno`, so the recv error being reported could be the close's instead of the recv's.
    // Letting OwnedDescriptor close at scope exit removes that window as well as the leak.

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return PingReply(address, options != nullptr ? std::make_optional(*options) : std::nullopt,
                              IPStatus::TimedOut, 0, {});
        }
        throw NetworkInformationException(errno);
    }

    IPStatus status = IPStatus::Unknown;
    std::vector<SharpRuntime::bytecs> replyBuffer;
    if (isIPv6) {
        if (static_cast<size_t>(received) >= sizeof(icmp6_hdr)) {
            const auto* hdr = reinterpret_cast<const icmp6_hdr*>(recvBuf.data());
            status = mapIcmpV6Status(hdr->icmp6_type);
            if (static_cast<size_t>(received) > sizeof(icmp6_hdr)) {
                replyBuffer.assign(recvBuf.begin() + sizeof(icmp6_hdr), recvBuf.begin() + received);
            }
        }
    } else {
        if (static_cast<size_t>(received) >= sizeof(IcmpV4Header)) {
            const auto* hdr = reinterpret_cast<const IcmpV4Header*>(recvBuf.data());
#if defined(SHARP_RUNTIME_PING_LINUX_ICMP)
            status = mapIcmpV4Status(hdr->type, hdr->code);
#else
            status = mapIcmpV4Status(hdr->icmp_type, hdr->icmp_code);
#endif
            if (static_cast<size_t>(received) > sizeof(IcmpV4Header)) {
                replyBuffer.assign(recvBuf.begin() + sizeof(IcmpV4Header), recvBuf.begin() + received);
            }
        }
    }

    return PingReply(address, options != nullptr ? std::make_optional(*options) : std::nullopt, status,
                      static_cast<SharpRuntime::longcs>(status == IPStatus::Success ? elapsedMs : 0), replyBuffer);
}

#else

PingReply Ping::sendPingCore(const System::Net::IPAddress&, const std::vector<SharpRuntime::bytecs>&,
                               SharpRuntime::intcs, const PingOptions*) {
    throw System::PlatformNotSupportedException("Ping is only implemented on POSIX platforms in this runtime.");
}

#endif

namespace {

    /**
     * The platform send core, as a plain function pointer. `Ping::sendPingCore` is a private
     * static member, so the file-local helpers below cannot name it; every public door forms this
     * pointer in its own (member) scope and hands it down, which is the idiom this file already
     * used before ticket #2188 and the reason none of that ticket's restructuring needed a change
     * to `Ping.hpp`.
     */
    using PingCore = PingReply (*)(const System::Net::IPAddress&, const std::vector<SharpRuntime::bytecs>&,
                                    SharpRuntime::intcs, const PingOptions*);

    /**
     * The ONE message .NET gives every `PingException` it raises.
     *
     * Ticket #2192 read it out of the reference: `SR.net_ping` is
     * "An exception occurred during a Ping request." (`System.Net.Ping/src/Resources/Strings.resx:75`),
     * and it is the message at **every** `PingException` construction site in the reference —
     * `Ping.cs:411`, `Ping.Windows.cs:283`, `Ping.PingUtility.cs:69` and `:104`. There is no
     * separate resource for a resolver failure and none for an ICMP failure. The two distinct
     * texts this port used before #2192 were invented here.
     */
    constexpr const char* PingFailureMessage = "An exception occurred during a Ping request.";

    PingReply sendWithExceptionWrapping(const System::Net::IPAddress& address,
                                          const std::vector<SharpRuntime::bytecs>& buffer, SharpRuntime::intcs timeout,
                                          const PingOptions* options, PingCore core) {
        try {
            return core(address, buffer, timeout, options);
        } catch (const System::PlatformNotSupportedException&) {
            throw;
        } catch (...) {
            // Ticket #2189 (SR-AUD-254). This used to read
            //   catch (const std::exception& e) { ... std::make_exception_ptr(e) ... }
            // which captures by the handler parameter's STATIC type: make_exception_ptr
            // copy-constructs a `std::exception`, so the object stored was the base subobject of
            // whatever was thrown. Measured before the fix, every wrapped Ping failure in a
            // container that denies ICMP carried inner type `St9exception` with message
            // "std::exception", while the object actually thrown was
            // NetworkInformationException("Win32 error 13") with getErrorCodeProperty() == 13 --
            // the type, the message and the native error code were all destroyed at the moment
            // of capture, not at rethrow.
            //
            // std::current_exception() instead returns an exception_ptr to the exception object
            // CURRENTLY BEING HANDLED, so the dynamic type and every payload survive, and the
            // catch-all means a cause that does not derive from std::exception is preserved too
            // rather than escaping this wrapper unwrapped.
            throw PingException(PingFailureMessage, std::current_exception());
        }
    }

    /**
     * Resolves @p hostNameOrAddress and sends to its first address.
     *
     * Preconditions, both established at the public door before this is reached: the host name is
     * non-empty and is NOT an IP literal (a literal short-circuits to the address overload), and
     * @p timeout / @p buffer have already been validated. Ticket #2188 moved that validation to
     * the door precisely so this function -- which runs on a worker thread for the asynchronous
     * doors -- performs I/O only.
     *
     * @note **Resolution happens INSIDE the wrapper, and that is .NET's shape** (ticket #2192,
     * settled against the reference). `Ping.GetAddressAndSend` is
     *
     *     try { IPAddress[] addresses = Dns.GetHostAddresses(hostNameOrAddress);
     *           return SendPingCore(addresses[0], buffer, timeout, options); }
     *     catch (Exception e) when (e is not PlatformNotSupportedException)
     *     { throw new PingException(SR.net_ping, e); }
     *
     * (`Ping.cs:686-702`) — **one** `try` covering both the resolve and the send. Before #2192
     * this port called `Dns::GetHostAddresses` outside the wrapper, so an unresolvable host
     * escaped as a bare `System::Net::Sockets::SocketException` where .NET reports a
     * `PingException` carrying that `SocketException` as its inner exception.
     *
     * @note The empty-list branch is also .NET's, in the only form .NET has: the reference
     * indexes `addresses[0]` with no emptiness check at all, so an empty result raises
     * `IndexOutOfRangeException` **inside** the try and reaches the caller as a `PingException`
     * wrapping it. Throwing that exception explicitly reproduces the observable outcome without
     * relying on `std::vector::operator[]`, whose out-of-range behaviour is undefined. The port's
     * former "Could not resolve host name or address." message has no counterpart in the
     * reference and is gone.
     *
     * @note There is deliberately no nested call to `sendWithExceptionWrapping` here — that
     * would wrap a `PingException` inside another `PingException`, which .NET's single `try`
     * cannot produce.
     */
    PingReply sendToResolvedHost(const std::string& hostNameOrAddress, SharpRuntime::intcs timeout,
                                  const std::vector<SharpRuntime::bytecs>& buffer, const PingOptions* options,
                                  PingCore core) {
        try {
            auto addresses = System::Net::Dns::GetHostAddresses(hostNameOrAddress);
            if (addresses.empty()) {
                throw System::IndexOutOfRangeException();
            }
            return core(addresses[0], buffer, timeout, options);
        } catch (const System::PlatformNotSupportedException&) {
            throw;
        } catch (...) {
            throw PingException(PingFailureMessage, std::current_exception());
        }
    }

} // namespace

PingReply Ping::Send(const std::string& hostNameOrAddress) {
    return Send(hostNameOrAddress, DefaultTimeout, defaultSendBuffer());
}

PingReply Ping::Send(const std::string& hostNameOrAddress, SharpRuntime::intcs timeout) {
    return Send(hostNameOrAddress, timeout, defaultSendBuffer());
}

PingReply Ping::Send(const System::Net::IPAddress& address) {
    return Send(address, DefaultTimeout, defaultSendBuffer());
}

PingReply Ping::Send(const System::Net::IPAddress& address, SharpRuntime::intcs timeout) {
    return Send(address, timeout, defaultSendBuffer());
}

PingReply Ping::Send(const std::string& hostNameOrAddress, SharpRuntime::intcs timeout,
                      const std::vector<SharpRuntime::bytecs>& buffer) {
    // Ticket #2190 (SR-AUD-255). This used to read
    //   return Send(hostNameOrAddress, timeout, buffer, PingOptions());
    // which invented a default PingOptions{ttl = 128, dontFragment = false} the caller never
    // supplied, so a successful reply reported options that were never requested and the socket
    // had its TTL forced to 128. .NET passes null here, and so does this file's own sibling
    // Send(address, timeout, buffer) three lines below -- the inconsistency was internal.
    System::ArgumentException::ThrowIfNullOrEmpty(hostNameOrAddress, "hostNameOrAddress");

    System::Net::IPAddress parsed;
    if (System::Net::IPAddress::TryParse(hostNameOrAddress, parsed)) {
        return Send(parsed, timeout, buffer);
    }

    checkArgs(timeout, buffer);
    return sendToResolvedHost(hostNameOrAddress, timeout, buffer, nullptr, &Ping::sendPingCore);
}

PingReply Ping::Send(const System::Net::IPAddress& address, SharpRuntime::intcs timeout,
                      const std::vector<SharpRuntime::bytecs>& buffer) {
    checkArgs(address, timeout, buffer);
    return sendWithExceptionWrapping(address, buffer, timeout, nullptr, &Ping::sendPingCore);
}

PingReply Ping::Send(const std::string& hostNameOrAddress, SharpRuntime::intcs timeout,
                      const std::vector<SharpRuntime::bytecs>& buffer, const PingOptions& options) {
    System::ArgumentException::ThrowIfNullOrEmpty(hostNameOrAddress, "hostNameOrAddress");

    System::Net::IPAddress parsed;
    if (System::Net::IPAddress::TryParse(hostNameOrAddress, parsed)) {
        return Send(parsed, timeout, buffer, options);
    }

    checkArgs(timeout, buffer);
    return sendToResolvedHost(hostNameOrAddress, timeout, buffer, &options, &Ping::sendPingCore);
}

PingReply Ping::Send(const System::Net::IPAddress& address, SharpRuntime::intcs timeout,
                      const std::vector<SharpRuntime::bytecs>& buffer, const PingOptions& options) {
    checkArgs(address, timeout, buffer);
    return sendWithExceptionWrapping(address, buffer, timeout, &options, &Ping::sendPingCore);
}

System::Threading::Tasks::TaskT<PingReply> Ping::SendPingAsync(const System::Net::IPAddress& address) {
    return SendPingAsync(address, DefaultTimeout, defaultSendBuffer());
}

System::Threading::Tasks::TaskT<PingReply> Ping::SendPingAsync(const std::string& hostNameOrAddress) {
    return SendPingAsync(hostNameOrAddress, DefaultTimeout, defaultSendBuffer());
}

System::Threading::Tasks::TaskT<PingReply> Ping::SendPingAsync(const System::Net::IPAddress& address,
                                                                 SharpRuntime::intcs timeout) {
    return SendPingAsync(address, timeout, defaultSendBuffer());
}

System::Threading::Tasks::TaskT<PingReply> Ping::SendPingAsync(const std::string& hostNameOrAddress,
                                                                 SharpRuntime::intcs timeout) {
    return SendPingAsync(hostNameOrAddress, timeout, defaultSendBuffer());
}

// The four asynchronous doors below all share the shape ticket #2188 (SR-AUD-253) introduced:
// validate at the CALL, then construct the task. They used to construct `TaskT<PingReply>` around
// a lambda that called `Send`, so `checkArgs` ran on the worker: every one of the eight overloads
// returned a task for an argument that was already invalid and faulted later, where .NET throws
// before it creates the async operation. Because `TaskT`'s callable constructor is
// `std::async(std::launch::async, ...)`, that also started a real OS thread per rejected argument.
//
// Each worker lambda now captures values only, and calls the file-local send helpers through a
// `PingCore` pointer formed here, in member scope. That is ticket #2191: the lambdas used to
// capture a raw `this` and call a non-static member on it from the worker, so destroying the Ping
// while its task ran called a member function on a destroyed object. `sizeof(Ping) == 1` -- the
// class declares no data members -- so the capture was removable outright, which is NOT the case
// for the blocked stateful raw-`this` family (#2066, #2088, #2134); nothing here resolves or
// pre-empts those.

System::Threading::Tasks::TaskT<PingReply> Ping::SendPingAsync(const System::Net::IPAddress& address,
                                                                 SharpRuntime::intcs timeout,
                                                                 const std::vector<SharpRuntime::bytecs>& buffer) {
    checkArgs(address, timeout, buffer);
    const PingCore core = &Ping::sendPingCore;
    // nullptr, not PingOptions(): ticket #2190 (SR-AUD-255), same reason as the synchronous door.
    return System::Threading::Tasks::TaskT<PingReply>([address, timeout, buffer, core]() {
        return sendWithExceptionWrapping(address, buffer, timeout, nullptr, core);
    });
}

System::Threading::Tasks::TaskT<PingReply> Ping::SendPingAsync(const std::string& hostNameOrAddress,
                                                                 SharpRuntime::intcs timeout,
                                                                 const std::vector<SharpRuntime::bytecs>& buffer) {
    System::ArgumentException::ThrowIfNullOrEmpty(hostNameOrAddress, "hostNameOrAddress");

    System::Net::IPAddress parsed;
    if (System::Net::IPAddress::TryParse(hostNameOrAddress, parsed)) {
        return SendPingAsync(parsed, timeout, buffer);
    }

    checkArgs(timeout, buffer);
    const PingCore core = &Ping::sendPingCore;
    return System::Threading::Tasks::TaskT<PingReply>([hostNameOrAddress, timeout, buffer, core]() {
        return sendToResolvedHost(hostNameOrAddress, timeout, buffer, nullptr, core);
    });
}

System::Threading::Tasks::TaskT<PingReply> Ping::SendPingAsync(const System::Net::IPAddress& address,
                                                                 SharpRuntime::intcs timeout,
                                                                 const std::vector<SharpRuntime::bytecs>& buffer,
                                                                 const PingOptions& options) {
    checkArgs(address, timeout, buffer);
    const PingCore core = &Ping::sendPingCore;
    // `options` is captured BY VALUE, so `&options` inside the body is the address of the closure's
    // own copy -- live for the whole call, and never a pointer back into the caller's frame.
    return System::Threading::Tasks::TaskT<PingReply>([address, timeout, buffer, options, core]() {
        return sendWithExceptionWrapping(address, buffer, timeout, &options, core);
    });
}

System::Threading::Tasks::TaskT<PingReply> Ping::SendPingAsync(const std::string& hostNameOrAddress,
                                                                 SharpRuntime::intcs timeout,
                                                                 const std::vector<SharpRuntime::bytecs>& buffer,
                                                                 const PingOptions& options) {
    System::ArgumentException::ThrowIfNullOrEmpty(hostNameOrAddress, "hostNameOrAddress");

    System::Net::IPAddress parsed;
    if (System::Net::IPAddress::TryParse(hostNameOrAddress, parsed)) {
        return SendPingAsync(parsed, timeout, buffer, options);
    }

    checkArgs(timeout, buffer);
    const PingCore core = &Ping::sendPingCore;
    return System::Threading::Tasks::TaskT<PingReply>([hostNameOrAddress, timeout, buffer, options, core]() {
        return sendToResolvedHost(hostNameOrAddress, timeout, buffer, &options, core);
    });
}

} // namespace System::Net::NetworkInformation
