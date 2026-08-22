// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for Socket's async-start exception seam. Production declares and
// befriends SocketAsyncStartAccess<Socket>, but never defines it; the sole specialization lives
// in SocketsGatedBehaviourPins.cpp. Ordinary consumers cannot inject a failure or inspect the
// private in-flight count.
//
// NEGATIVE-FIXTURE: component=Net.Sockets

#include "System/Net/Sockets/Socket.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Net::Sockets::AddressFamily;
using System::Net::Sockets::ProtocolType;
using System::Net::Sockets::Socket;
using System::Net::Sockets::SocketType;

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(socket-async-start-seam-incomplete): incomplete type
    //     | used in nested name specifier
    SharpRuntime::Testing::SocketAsyncStartAccess<Socket>::setBeforeTaskHook(nullptr);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(socket-async-start-hook-private): is private within this context
    //     | private member
    Socket::beforeAsyncTaskTestHook_.store(nullptr);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(socket-async-count-private): is private within this context
    //     | private member
    Socket socket(AddressFamily::InterNetwork, SocketType::Dgram, ProtocolType::Udp);
    return socket.asyncOperationCountForTesting();
#endif

    return 0;
}
