// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/NetworkInformation/NetworkInterface.hpp"
#include "System/Net/NetworkInformation/NetworkInformationException.hpp"
#include "System/PlatformNotSupportedException.hpp"

#if defined(_WIN32)
// No Windows PAL implemented yet.
#elif defined(__EMSCRIPTEN__)
// No network interface enumeration available under Emscripten.
#elif defined(__linux__) || defined(__APPLE__)
// Both enumerate with getifaddrs(); only the link-layer record differs -- AF_PACKET /
// sockaddr_ll on Linux, AF_LINK / sockaddr_dl on Darwin -- along with where the speed comes
// from and what the loopback interface is called.
#define SHARP_RUNTIME_NETWORKINTERFACE_IFADDRS 1
#include <cstdio>
#include <cstring>
#include <ifaddrs.h>
#include <map>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>
#  if defined(__linux__)
#    include <linux/if_packet.h>
#    include <net/if_arp.h>
#  else
#    include <net/if_dl.h>
#    include <net/if_types.h>
#  endif
#endif

namespace System::Net::NetworkInformation {

#if defined(SHARP_RUNTIME_NETWORKINTERFACE_IFADDRS)
namespace {

#if defined(__linux__)
    SharpRuntime::longcs readSpeed(const std::string& name) {
        std::string path = "/sys/class/net/" + name + "/speed";
        FILE* f = std::fopen(path.c_str(), "r");
        if (f == nullptr) {
            return -1;
        }
        long long value = -1;
        int matched = std::fscanf(f, "%lld", &value);
        std::fclose(f);
        if (matched != 1 || value < 0) {
            return -1;
        }
        return static_cast<SharpRuntime::longcs>(value) * 1'000'000; // Mbps -> bits/sec
    }

    NetworkInterfaceType mapHardwareType(unsigned short arphrdType, bool isLoopback) {
        if (isLoopback) {
            return NetworkInterfaceType::Loopback;
        }
        switch (arphrdType) {
            case ARPHRD_ETHER:
                return NetworkInterfaceType::Ethernet;
            case ARPHRD_LOOPBACK:
                return NetworkInterfaceType::Loopback;
            case ARPHRD_PPP:
                return NetworkInterfaceType::Ppp;
            case ARPHRD_TUNNEL:
            case ARPHRD_TUNNEL6:
            case ARPHRD_SIT:
            case ARPHRD_IPGRE:
                return NetworkInterfaceType::Tunnel;
            case ARPHRD_FDDI:
                return NetworkInterfaceType::Fddi;
            default:
                return NetworkInterfaceType::Unknown;
        }
    }
#else
    // sockaddr_dl's sdl_type is an IFT_* value, which for most types IS the IANA ifType number
    // that NetworkInterfaceType uses, but not for all (the BSD tunnels IFT_GIF/IFT_STF are local
    // numbers). Mapped explicitly, as .NET's BSD PAL does.
    NetworkInterfaceType mapInterfaceType(unsigned char ifType, bool isLoopback) {
        if (isLoopback) {
            return NetworkInterfaceType::Loopback;
        }
        switch (ifType) {
            case IFT_ETHER:
            case IFT_L2VLAN:
                return NetworkInterfaceType::Ethernet;
            case IFT_ISO88025:
                return NetworkInterfaceType::TokenRing;
            case IFT_FDDI:
                return NetworkInterfaceType::Fddi;
            case IFT_ISDNBASIC:
                return NetworkInterfaceType::BasicIsdn;
            case IFT_ISDNPRIMARY:
                return NetworkInterfaceType::PrimaryIsdn;
            case IFT_PPP:
                return NetworkInterfaceType::Ppp;
            case IFT_LOOP:
                return NetworkInterfaceType::Loopback;
            case IFT_SLIP:
                return NetworkInterfaceType::Slip;
            case IFT_GIF:
            case IFT_STF:
                return NetworkInterfaceType::Tunnel;
#if defined(IFT_IEEE80211)
            // Darwin has no IFT_IEEE80211 -- it reports Wi-Fi as IFT_ETHER -- other BSDs do.
            case IFT_IEEE80211:
                return NetworkInterfaceType::Wireless80211;
#endif
            case IFT_IEEE1394:
                return NetworkInterfaceType::HighPerformanceSerialBus;
            default:
                return NetworkInterfaceType::Unknown;
        }
    }
#endif

    struct InterfaceAccumulator {
        NetworkInterfaceType type = NetworkInterfaceType::Unknown;
        OperationalStatus status = OperationalStatus::Unknown;
        SharpRuntime::longcs speed = -1;
        bool supportsMulticast = false;
        bool supportsIPv4 = false;
        bool supportsIPv6 = false;
        PhysicalAddress physicalAddress = PhysicalAddress::None;
    };

} // namespace

std::vector<std::shared_ptr<NetworkInterface>> NetworkInterface::GetAllNetworkInterfaces() {
    ifaddrs* addrs = nullptr;
    if (getifaddrs(&addrs) != 0) {
        throw NetworkInformationException();
    }

    std::vector<std::string> order;
    std::map<std::string, InterfaceAccumulator> byName;

    for (ifaddrs* cur = addrs; cur != nullptr; cur = cur->ifa_next) {
        if (cur->ifa_name == nullptr) {
            continue;
        }
        std::string name(cur->ifa_name);
        auto [it, inserted] = byName.try_emplace(name);
        if (inserted) {
            order.push_back(name);
        }
        InterfaceAccumulator& acc = it->second;

        bool isLoopback = (cur->ifa_flags & IFF_LOOPBACK) != 0;
        acc.supportsMulticast = acc.supportsMulticast || ((cur->ifa_flags & IFF_MULTICAST) != 0);
        bool up = (cur->ifa_flags & IFF_UP) != 0;
        bool running = (cur->ifa_flags & IFF_RUNNING) != 0;
        acc.status = (up && running) ? OperationalStatus::Up : OperationalStatus::Down;

        if (cur->ifa_addr == nullptr) {
            continue;
        }

        if (cur->ifa_addr->sa_family == AF_INET) {
            acc.supportsIPv4 = true;
        } else if (cur->ifa_addr->sa_family == AF_INET6) {
            acc.supportsIPv6 = true;
#if defined(__APPLE__)
        } else if (cur->ifa_addr->sa_family == AF_LINK) {
            const auto* dl = reinterpret_cast<const sockaddr_dl*>(cur->ifa_addr);
            acc.type = mapInterfaceType(dl->sdl_type, isLoopback);
            if (dl->sdl_alen > 0) {
                const auto* bytes = reinterpret_cast<const unsigned char*>(LLADDR(dl));
                std::vector<SharpRuntime::bytecs> mac(bytes, bytes + dl->sdl_alen);
                acc.physicalAddress = PhysicalAddress(mac);
            }
            // The AF_LINK entry's ifa_data is the interface's if_data; 0 means "not reported".
            if (cur->ifa_data != nullptr) {
                const auto baud = static_cast<const if_data*>(cur->ifa_data)->ifi_baudrate;
                acc.speed = baud > 0 ? static_cast<SharpRuntime::longcs>(baud) : -1;
            }
#else
        } else if (cur->ifa_addr->sa_family == AF_PACKET) {
            const auto* ll = reinterpret_cast<sockaddr_ll*>(cur->ifa_addr);
            acc.type = mapHardwareType(ll->sll_hatype, isLoopback);
            if (ll->sll_halen > 0) {
                std::vector<SharpRuntime::bytecs> mac(ll->sll_addr, ll->sll_addr + ll->sll_halen);
                acc.physicalAddress = PhysicalAddress(mac);
            }
            acc.speed = readSpeed(name);
#endif
        }

        if (isLoopback && acc.type == NetworkInterfaceType::Unknown) {
            acc.type = NetworkInterfaceType::Loopback;
        }
    }

    freeifaddrs(addrs);

    std::vector<std::shared_ptr<NetworkInterface>> result;
    result.reserve(order.size());
    for (const std::string& name : order) {
        const InterfaceAccumulator& acc = byName[name];
        result.push_back(std::shared_ptr<NetworkInterface>(new NetworkInterface(
            name, acc.type, acc.status, acc.speed, acc.supportsMulticast, acc.supportsIPv4, acc.supportsIPv6,
            acc.physicalAddress)));
    }
    return result;
}

bool NetworkInterface::GetIsNetworkAvailable() {
    // Verified against NetworkInterfacePal.Linux.cs's GetIsNetworkAvailable: real .NET skips
    // both Loopback AND Tunnel interfaces, not just Loopback -- a machine whose only "up"
    // non-loopback interface is a VPN tunnel previously (silently, incorrectly) reported
    // network availability as true here.
    for (const auto& iface : GetAllNetworkInterfaces()) {
        NetworkInterfaceType type = iface->getNetworkInterfaceTypeProperty();
        if (type == NetworkInterfaceType::Loopback || type == NetworkInterfaceType::Tunnel) {
            continue;
        }
        if (iface->getOperationalStatusProperty() == OperationalStatus::Up) {
            return true;
        }
    }
    return false;
}

SharpRuntime::intcs NetworkInterface::getIPv6LoopbackInterfaceIndexProperty() {
    return getLoopbackInterfaceIndexProperty();
}

SharpRuntime::intcs NetworkInterface::getLoopbackInterfaceIndexProperty() {
#if defined(__linux__)
    unsigned int index = if_nametoindex("lo");
#else
    // Darwin calls it lo0, and nothing guarantees the name; the interface flagged IFF_LOOPBACK
    // is the one that is meant.
    unsigned int index = 0;
    ifaddrs* addrs = nullptr;
    if (getifaddrs(&addrs) != 0) {
        throw NetworkInformationException();
    }
    for (ifaddrs* cur = addrs; cur != nullptr && index == 0; cur = cur->ifa_next) {
        if (cur->ifa_name != nullptr && (cur->ifa_flags & IFF_LOOPBACK) != 0) {
            index = if_nametoindex(cur->ifa_name);
        }
    }
    freeifaddrs(addrs);
#endif
    if (index == 0) {
        throw NetworkInformationException();
    }
    return static_cast<SharpRuntime::intcs>(index);
}

bool NetworkInterface::Supports(NetworkInterfaceComponent networkInterfaceComponent) const {
    return networkInterfaceComponent == NetworkInterfaceComponent::IPv4 ? supportsIPv4_ : supportsIPv6_;
}

#else

std::vector<std::shared_ptr<NetworkInterface>> NetworkInterface::GetAllNetworkInterfaces() {
    throw System::PlatformNotSupportedException(
        "NetworkInterface.GetAllNetworkInterfaces() is only implemented on Linux and macOS in this runtime.");
}

bool NetworkInterface::GetIsNetworkAvailable() {
    throw System::PlatformNotSupportedException(
        "NetworkInterface.GetIsNetworkAvailable() is only implemented on Linux and macOS in this runtime.");
}

SharpRuntime::intcs NetworkInterface::getIPv6LoopbackInterfaceIndexProperty() {
    throw System::PlatformNotSupportedException(
        "NetworkInterface.IPv6LoopbackInterfaceIndex is only implemented on Linux and macOS in this runtime.");
}

SharpRuntime::intcs NetworkInterface::getLoopbackInterfaceIndexProperty() {
    throw System::PlatformNotSupportedException(
        "NetworkInterface.LoopbackInterfaceIndex is only implemented on Linux and macOS in this runtime.");
}

bool NetworkInterface::Supports(NetworkInterfaceComponent) const {
    throw System::PlatformNotSupportedException(
        "NetworkInterface.Supports() is only implemented on Linux and macOS in this runtime.");
}

#endif

} // namespace System::Net::NetworkInformation
