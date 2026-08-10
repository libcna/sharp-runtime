// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <exception>
#include <string>
#include <typeinfo>
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Net/NetworkInformation/NetworkInformationException.hpp"
#include "System/Net/NetworkInformation/Ping.hpp"
#include "System/Net/NetworkInformation/PingException.hpp"
#include "System/Net/Sockets/SocketException.hpp"

using namespace System::Net::NetworkInformation;
using System::Net::IPAddress;

namespace {

    /**
     * Produces a PingException from a send that the kernel must refuse, without depending on any
     * particular socket policy -- ticket #2189 (SR-AUD-254) needs a *failing* send to inspect, and
     * whether a send fails at socket creation or at sendto differs between hosts.
     *
     * Candidate order is deliberate:
     *   1. an IPv6 link-local destination with no scope id, which sendto rejects with EINVAL
     *      because the kernel cannot choose an outgoing interface (and whose socket cannot even be
     *      created on a host with no IPv6);
     *   2. the IPv4 broadcast address, which an ICMP socket may not send to without SO_BROADCAST;
     *   3. plain loopback, which fails at socket creation wherever ping_group_range forbids
     *      unprivileged ICMP sockets -- the case this container is in.
     *
     * Returns an empty exception_ptr only if every candidate somehow succeeded, which the callers
     * report as a skip rather than silently passing.
     */
    std::exception_ptr firstRefusedSend() {
        Ping ping;
        const IPAddress candidates[] = {IPAddress::Parse("fe80::1"), IPAddress::Broadcast, IPAddress::Loopback};
        for (const IPAddress& address : candidates) {
            try {
                (void)ping.Send(address, 1000);
            } catch (...) {
                return std::current_exception();
            }
        }
        return {};
    }

    /** Rethrows @p p and returns the mangled dynamic type name of whatever comes out. */
    std::string dynamicTypeName(std::exception_ptr p) {
        if (!p) {
            return "(none)";
        }
        try {
            std::rethrow_exception(p);
        } catch (const std::exception& e) {
            return typeid(e).name();
        } catch (...) {
            return "(non-std)";
        }
    }

} // namespace

TEST(PingTests, Send_Loopback_Succeeds) {
    Ping ping;
    PingReply reply = ping.Send(IPAddress::Loopback);
    EXPECT_EQ(reply.getStatusProperty(), IPStatus::Success);
    EXPECT_EQ(reply.getAddressProperty(), IPAddress::Loopback);
    EXPECT_GE(reply.getRoundtripTimeProperty(), 0);
    EXPECT_EQ(reply.getBufferProperty().size(), 32u);
}

TEST(PingTests, Send_LoopbackByString_Succeeds) {
    Ping ping;
    PingReply reply = ping.Send(std::string("127.0.0.1"));
    EXPECT_EQ(reply.getStatusProperty(), IPStatus::Success);
}

TEST(PingTests, Send_CustomBuffer_EchoedBack) {
    Ping ping;
    std::vector<SharpRuntime::bytecs> buffer{1, 2, 3, 4, 5};
    PingReply reply = ping.Send(IPAddress::Loopback, 5000, buffer);
    EXPECT_EQ(reply.getStatusProperty(), IPStatus::Success);
    EXPECT_EQ(reply.getBufferProperty(), buffer);
}

TEST(PingTests, Send_WithOptions_Succeeds) {
    Ping ping;
    PingOptions options(64, true);
    PingReply reply = ping.Send(IPAddress::Loopback, 5000, std::vector<SharpRuntime::bytecs>{9, 9}, options);
    EXPECT_EQ(reply.getStatusProperty(), IPStatus::Success);
    ASSERT_TRUE(reply.getOptionsProperty().has_value());
    EXPECT_EQ(reply.getOptionsProperty()->getTtlProperty(), 64);
}

TEST(PingTests, Send_NegativeTimeout_Throws) {
    Ping ping;
    EXPECT_THROW(ping.Send(IPAddress::Loopback, -1), System::ArgumentOutOfRangeException);
}

TEST(PingTests, Send_BufferTooLarge_Throws) {
    Ping ping;
    std::vector<SharpRuntime::bytecs> buffer(65501);
    EXPECT_THROW(ping.Send(IPAddress::Loopback, 5000, buffer), System::ArgumentException);
}

TEST(PingTests, Send_AnyAddress_Throws) {
    Ping ping;
    EXPECT_THROW(ping.Send(IPAddress::Any), System::ArgumentException);
}

TEST(PingTests, Send_EmptyHostName_Throws) {
    Ping ping;
    EXPECT_THROW(ping.Send(std::string("")), System::ArgumentException);
}

TEST(PingTests, SendPingAsync_Loopback_Succeeds) {
    Ping ping;
    auto task = ping.SendPingAsync(IPAddress::Loopback);
    PingReply reply = task.getResultProperty();
    EXPECT_EQ(reply.getStatusProperty(), IPStatus::Success);
}

// --- Ticket #2189 (SR-AUD-254): the wrapper must not slice its cause -------------------------
//
// These tests need a send that FAILS, which is orthogonal to #1962: whether the runtime opens a
// SOCK_DGRAM or a SOCK_RAW ICMP socket, whatever the core throws must reach the caller intact.

TEST(PingTests, Send_WrappedFailure_InnerExceptionIsNotSlicedToStdException) {
    std::exception_ptr raised = firstRefusedSend();
    if (!raised) {
        GTEST_SKIP() << "every candidate ICMP send succeeded on this host, so there is no wrapped "
                        "failure to inspect";
    }
    try {
        std::rethrow_exception(raised);
    } catch (const PingException& pe) {
        std::exception_ptr inner = pe.getInnerExceptionProperty();
        ASSERT_TRUE(static_cast<bool>(inner)) << "the wrapper dropped its cause entirely";
        // The exact concrete type depends on which candidate failed and how; what must never
        // happen again is the cause arriving as the std::exception BASE SUBOBJECT.
        EXPECT_NE(dynamicTypeName(inner), typeid(std::exception).name())
            << "inner exception was sliced to a bare std::exception";
    } catch (const std::exception& e) {
        FAIL() << "expected PingException, got " << typeid(e).name() << ": " << e.what();
    }
}

TEST(PingTests, Send_WrappedFailure_InnerExceptionKeepsItsOwnMessage) {
    std::exception_ptr raised = firstRefusedSend();
    if (!raised) {
        GTEST_SKIP() << "every candidate ICMP send succeeded on this host";
    }
    try {
        std::rethrow_exception(raised);
    } catch (const PingException& pe) {
        std::exception_ptr inner = pe.getInnerExceptionProperty();
        ASSERT_TRUE(static_cast<bool>(inner));
        try {
            std::rethrow_exception(inner);
        } catch (const std::exception& e) {
            // "std::exception" is exactly what a sliced std::exception's what() returns.
            EXPECT_NE(std::string(e.what()), "std::exception")
                << "the cause's own message was replaced by the base class's";
            EXPECT_FALSE(std::string(e.what()).empty());
        }
    }
}

TEST(PingTests, Send_WrappedFailure_NetworkInformationExceptionKeepsItsNativeErrorCode) {
    std::exception_ptr raised = firstRefusedSend();
    if (!raised) {
        GTEST_SKIP() << "every candidate ICMP send succeeded on this host";
    }
    try {
        std::rethrow_exception(raised);
    } catch (const PingException& pe) {
        std::exception_ptr inner = pe.getInnerExceptionProperty();
        ASSERT_TRUE(static_cast<bool>(inner));
        try {
            std::rethrow_exception(inner);
        } catch (const NetworkInformationException& nie) {
            // A sliced cause could not be caught here at all, so reaching this handler is itself
            // half the assertion; the code proves the payload survived too.
            EXPECT_NE(nie.getErrorCodeProperty(), 0)
                << "the native error code did not survive the wrapper";
        } catch (const std::exception& e) {
            FAIL() << "inner exception was not a NetworkInformationException but " << typeid(e).name();
        }
    }
}

TEST(PingTests, Send_WrappedFailure_OuterMessageIsUnchanged) {
    std::exception_ptr raised = firstRefusedSend();
    if (!raised) {
        GTEST_SKIP() << "every candidate ICMP send succeeded on this host";
    }
    try {
        std::rethrow_exception(raised);
    } catch (const PingException& pe) {
        EXPECT_EQ(std::string(pe.what()),
                  "An exception occurred while sending or receiving the ICMP message.");
    }
}

// Ticket #2192 (deferred verification): a resolver failure is raised OUTSIDE the wrapper, so it
// escapes as a SocketException rather than as a PingException, while the module's own
// "Could not resolve host name or address." PingException sits on a branch Dns::GetHostAddresses
// makes practically unreachable. Which of the two .NET produces cannot be settled in this
// container. Pinning the current answer is what stops it from changing silently.
TEST(PingTests, Send_UnresolvableHost_ThrowsSocketExceptionUnwrapped_PIN2192) {
    Ping ping;
    EXPECT_THROW(static_cast<void>(ping.Send(std::string("no-such-host.invalid.example"))),
                 System::Net::Sockets::SocketException);
}
