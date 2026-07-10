// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Dns.hpp"
#include "System/Net/Sockets/SocketException.hpp"
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
namespace { inline void wsaInit() {} }
#endif

namespace System::Net {

    using System::Net::Sockets::AddressFamily;
    using System::Net::Sockets::SocketException;
    using System::Net::Sockets::SocketError;
    using SharpRuntime::intcs;

#if !defined(__EMSCRIPTEN__)
    namespace {
        std::optional<IPAddress> tryParseIPv4(const std::string& s) {
            unsigned a, b, c, d;
            char extra;
            if (std::sscanf(s.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4) return std::nullopt;
            if (a > 255 || b > 255 || c > 255 || d > 255) return std::nullopt;
            return IPAddress((a << 24) | (b << 16) | (c << 8) | d);
        }

        IPAddress fromSockaddrIn(const sockaddr_in& sin) {
            uint32_t hostOrder = ntohl(sin.sin_addr.s_addr);
            return IPAddress(hostOrder);
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
            throw SocketException(static_cast<intcs>(SocketError::SocketError));
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
        if (family == AddressFamily::InterNetworkV6) {
            return {};
        }

        if (auto parsed = tryParseIPv4(hostNameOrAddress)) {
            return { *parsed };
        }

        wsaInit();
        struct addrinfo hints {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = 0;
        struct addrinfo* res = nullptr;
        int rc = ::getaddrinfo(hostNameOrAddress.c_str(), nullptr, &hints, &res);
        if (rc != 0) {
            throw SocketException(static_cast<intcs>(SocketError::HostNotFound));
        }

        std::vector<IPAddress> result;
        for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
            if (p->ai_family == AF_INET) {
                result.push_back(fromSockaddrIn(*reinterpret_cast<sockaddr_in*>(p->ai_addr)));
            }
        }
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
        if (auto parsed = tryParseIPv4(hostNameOrAddress)) {
            return GetHostEntry(*parsed);
        }

        if (family == AddressFamily::InterNetworkV6) {
            IPHostEntry empty;
            empty.setHostNameProperty(hostNameOrAddress);
            return empty;
        }

        wsaInit();
        struct addrinfo hints {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = 0;
        hints.ai_flags = AI_CANONNAME;
        struct addrinfo* res = nullptr;
        int rc = ::getaddrinfo(hostNameOrAddress.c_str(), nullptr, &hints, &res);
        if (rc != 0) {
            throw SocketException(static_cast<intcs>(SocketError::HostNotFound));
        }

        IPHostEntry entry;
        std::string canonicalName = hostNameOrAddress;
        if (res != nullptr && res->ai_canonname != nullptr) {
            canonicalName = res->ai_canonname;
        }
        entry.setHostNameProperty(canonicalName);

        std::vector<IPAddress> addresses;
        for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
            if (p->ai_family == AF_INET) {
                addresses.push_back(fromSockaddrIn(*reinterpret_cast<sockaddr_in*>(p->ai_addr)));
            }
        }
        ::freeaddrinfo(res);
        entry.setAddressListProperty(addresses);
        return entry;
#endif
    }

    IPHostEntry Dns::GetHostEntry(const IPAddress& address) {
#if defined(__EMSCRIPTEN__)
        (void)address;
        throw System::PlatformNotSupportedException("Dns.GetHostEntry is not supported on Emscripten.");
#else
        wsaInit();
        sockaddr_in sin{};
        sin.sin_family = AF_INET;
        sin.sin_port = 0;
        sin.sin_addr.s_addr = htonl(address.getAddressProperty());

        char hostBuf[NI_MAXHOST] = {};
        int rc = ::getnameinfo(reinterpret_cast<sockaddr*>(&sin), sizeof(sin), hostBuf, sizeof(hostBuf), nullptr, 0, 0);
        if (rc != 0) {
            throw SocketException(static_cast<intcs>(SocketError::HostNotFound));
        }

        return GetHostEntry(std::string(hostBuf), AddressFamily::InterNetwork);
#endif
    }

} // namespace System::Net
