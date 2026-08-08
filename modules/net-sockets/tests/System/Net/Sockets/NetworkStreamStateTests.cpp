// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// #2136 / SR-AUD-265 -- NetworkStream took any int as a socket, and a CLOSED stream silently
// accepted writes. Causes NS-A (a descriptor-owning type that validates nothing) and NS-B (a
// recorded lifecycle state that no member enforces).
//
// Two instruments matter here and only one of them is a sanitizer:
//   * the exception each rejected descriptor shape produces, and
//   * /proc/self/fd, because a rejected constructor must NOT close a descriptor the caller
//     still owns. LSan cannot answer that -- a leaked descriptor is not a leaked allocation --
//     so the accounting is direct, with a deliberately leaked descriptor as the control that
//     proves the instrument can see a difference at all.
#include <gtest/gtest.h>

#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IO/IOException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Net/Sockets/NetworkStream.hpp"

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)

#  include <arpa/inet.h>
#  include <dirent.h>
#  include <fcntl.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>

#  include <functional>
#  include <string>

using System::Net::Sockets::NetworkStream;

namespace {

// Counts open descriptors by reading /proc/self/fd directly. The directory handle opendir() itself
// holds is one of the entries, so it is subtracted along with "." and "..".
int OpenDescriptorCount() {
    DIR* dir = ::opendir("/proc/self/fd");
    if (dir == nullptr) return -1;
    int entries = 0;
    while (::readdir(dir) != nullptr) ++entries;
    ::closedir(dir);
    return entries - 3;
}

// A connected, blocking, stream-oriented socket pair -- the shape NetworkStream is meant to wrap,
// and the shape TcpClient::GetStream() hands it.
struct StreamPair {
    int fds[2] = {-1, -1};
    StreamPair() { EXPECT_EQ(0, ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds)); }
    ~StreamPair() {
        if (fds[0] >= 0) ::close(fds[0]);
        if (fds[1] >= 0) ::close(fds[1]);
    }
    int release(int i) { int f = fds[i]; fds[i] = -1; return f; }
};

std::string MessageOf(const std::function<void()>& call) {
    try {
        call();
    } catch (const std::exception& e) {
        return e.what();
    }
    return "<did not throw>";
}

}  // namespace

// ===========================================================================================
// Cause NS-A -- construction validates the descriptor
// ===========================================================================================

// A descriptor that is not a socket handle at all is an argument-domain error about the
// parameter, and says so by name.
TEST(NetworkStreamConstructionTests, ADescriptorThatIsNotASocketHandleIsRejectedByName) {
    const int closedFd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(closedFd, 0);
    ::close(closedFd);

    for (const int fd : {-1, -2, 999999, closedFd}) {
        try {
            NetworkStream stream(fd);
            ADD_FAILURE() << "fd " << fd << " constructed a NetworkStream";
        } catch (const System::ArgumentException& e) {
            EXPECT_EQ(e.getParamNameProperty(), "fd") << "fd " << fd;
        } catch (const std::exception& e) {
            ADD_FAILURE() << "fd " << fd << " threw the wrong type: " << e.what();
        }
    }
}

// An open handle that is not a socket -- the pipe end the review measured, and a regular file.
// Neither is a socket in any sense, and .NET cannot even express the mistake because its
// constructor takes a Socket.
TEST(NetworkStreamConstructionTests, AnOpenNonSocketDescriptorIsRejected) {
    int pipeFds[2] = {-1, -1};
    ASSERT_EQ(0, ::pipe(pipeFds));
    const int fileFd = ::open("/dev/null", O_RDWR);
    ASSERT_GE(fileFd, 0);

    for (const int fd : {pipeFds[0], pipeFds[1], fileFd}) {
        try {
            NetworkStream stream(fd);
            ADD_FAILURE() << "non-socket fd " << fd << " constructed a NetworkStream";
        } catch (const System::ArgumentException& e) {
            EXPECT_EQ(e.getParamNameProperty(), "fd");
            EXPECT_NE(std::string(e.what()).find("not a socket"), std::string::npos) << e.what();
        }
    }

    // The rejected descriptors are still the caller's, and still work.
    const char byte = 'x';
    EXPECT_EQ(1, ::write(pipeFds[1], &byte, 1));
    char read_back = 0;
    EXPECT_EQ(1, ::read(pipeFds[0], &read_back, 1));
    EXPECT_EQ('x', read_back);
    EXPECT_EQ(1, ::write(fileFd, &byte, 1));

    ::close(pipeFds[0]);
    ::close(pipeFds[1]);
    ::close(fileFd);
}

// A socket, but not one a Stream can wrap: .NET's three IOException rules, each measured on the
// descriptor shape that triggers exactly it.
TEST(NetworkStreamConstructionTests, ASocketInTheWrongStateIsRejectedWithItsOwnMessage) {
    // non-stream: a datagram socket pair is connected and blocking, so only the type is wrong.
    int dgram[2] = {-1, -1};
    ASSERT_EQ(0, ::socketpair(AF_UNIX, SOCK_DGRAM, 0, dgram));
    EXPECT_NE(MessageOf([&] { NetworkStream s(dgram[0]); }).find("non-stream oriented"),
              std::string::npos);
    EXPECT_THROW(NetworkStream s(dgram[0]), System::IO::IOException);

    // non-connected: a fresh TCP socket is a blocking stream socket that has no peer.
    const int unconnected = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(unconnected, 0);
    EXPECT_NE(MessageOf([&] { NetworkStream s(unconnected); }).find("non-connected"),
              std::string::npos);
    EXPECT_THROW(NetworkStream s(unconnected), System::IO::IOException);

    // non-connected, second shape: a LISTENING socket. It is a stream socket with no peer, so it
    // is rejected for the same reason -- worth its own case because "listening" reads like a
    // usable state until you ask which peer the stream would talk to.
    const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(listener, 0);
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    ASSERT_EQ(0, ::bind(listener, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)));
    ASSERT_EQ(0, ::listen(listener, 4));
    EXPECT_NE(MessageOf([&] { NetworkStream s(listener); }).find("non-connected"),
              std::string::npos);

    // non-blocking: a connected stream socket with O_NONBLOCK set. Stream.Read/Write semantics
    // are incompatible with it, which is why .NET refuses rather than coping.
    StreamPair nonBlocking;
    ASSERT_GE(nonBlocking.fds[0], 0);
    ASSERT_EQ(0, ::fcntl(nonBlocking.fds[0], F_SETFL,
                         ::fcntl(nonBlocking.fds[0], F_GETFL) | O_NONBLOCK));
    EXPECT_NE(MessageOf([&] { NetworkStream s(nonBlocking.fds[0]); }).find("non-blocking"),
              std::string::npos);
    EXPECT_THROW(NetworkStream s(nonBlocking.fds[0]), System::IO::IOException);

    ::close(dgram[0]);
    ::close(dgram[1]);
    ::close(unconnected);
    ::close(listener);
}

// THE CONTROL. Every shape a NetworkStream is actually handed still constructs, including the
// one TcpClient::GetStream() produces (a dup of a connected loopback socket) and a socket that
// has been shut down but not closed.
TEST(NetworkStreamConstructionTests, THECONTROLAConnectedBlockingStreamSocketStillConstructs) {
    StreamPair pair;
    ASSERT_GE(pair.fds[0], 0);
    EXPECT_NO_THROW(NetworkStream s(pair.release(0)));

    // A real loopback TCP connection, and a dup() of it -- GetStream()'s exact shape.
    const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(listener, 0);
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    ASSERT_EQ(0, ::bind(listener, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)));
    socklen_t len = sizeof(addr);
    ASSERT_EQ(0, ::getsockname(listener, reinterpret_cast<struct sockaddr*>(&addr), &len));
    ASSERT_EQ(0, ::listen(listener, 4));
    const int client = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(client, 0);
    ASSERT_EQ(0, ::connect(client, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)));
    const int server = ::accept(listener, nullptr, nullptr);
    ASSERT_GE(server, 0);
    const int duplicated = ::dup(client);
    ASSERT_GE(duplicated, 0);

    EXPECT_NO_THROW(NetworkStream s(duplicated));
    EXPECT_NO_THROW(NetworkStream s(server));

    // A shut-down but still open connection stays wrappable: shutdown() changes neither the
    // socket type nor the peer binding, and .NET's Socket.Connected is likewise a cached flag
    // that shutdown does not clear. Measured, then pinned, so nobody has to re-derive it.
    StreamPair shutPair;
    ASSERT_GE(shutPair.fds[0], 0);
    ASSERT_EQ(0, ::shutdown(shutPair.fds[0], SHUT_RDWR));
    EXPECT_NO_THROW(NetworkStream s(shutPair.release(0)));

    ::close(listener);
    ::close(client);
}

// The AC's hard half: a rejected constructor must not close the caller's descriptor, and the
// instrument used to prove it must be able to see a leak when there is one.
TEST(NetworkStreamConstructionTests, ARejectedConstructionNeitherClosesNorLeaksADescriptor) {
    int pipeFds[2] = {-1, -1};
    ASSERT_EQ(0, ::pipe(pipeFds));
    int dgram[2] = {-1, -1};
    ASSERT_EQ(0, ::socketpair(AF_UNIX, SOCK_DGRAM, 0, dgram));
    const int unconnected = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(unconnected, 0);
    const int closedFd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(closedFd, 0);
    ::close(closedFd);

    // Baseline AFTER every fixture descriptor exists, so setup is not counted as drift.
    const int before = OpenDescriptorCount();
    ASSERT_GT(before, 0);

    for (int repeat = 0; repeat < 64; ++repeat) {
        EXPECT_THROW(NetworkStream s(-1), System::ArgumentException);
        EXPECT_THROW(NetworkStream s(closedFd), System::ArgumentException);
        EXPECT_THROW(NetworkStream s(pipeFds[0]), System::ArgumentException);
        EXPECT_THROW(NetworkStream s(dgram[0]), System::IO::IOException);
        EXPECT_THROW(NetworkStream s(unconnected), System::IO::IOException);
    }

    const int after = OpenDescriptorCount();
    EXPECT_EQ(before, after) << "320 rejected constructions changed the descriptor count";

    // Every rejected descriptor is still open and still the caller's: closing it now must
    // succeed. If a rejected constructor had closed it, this would fail with EBADF.
    EXPECT_EQ(0, ::close(pipeFds[0])) << "the constructor closed a descriptor it rejected";
    EXPECT_EQ(0, ::close(pipeFds[1]));
    EXPECT_EQ(0, ::close(dgram[0])) << "the constructor closed a descriptor it rejected";
    EXPECT_EQ(0, ::close(dgram[1]));
    EXPECT_EQ(0, ::close(unconnected)) << "the constructor closed a descriptor it rejected";
}

// THE DISCRIMINATION CONTROL for the instrument above. A deliberately leaked descriptor must
// make OpenDescriptorCount() move; without this, "the count did not change" could equally mean
// "the count never changes".
TEST(NetworkStreamConstructionTests, THECONTROLTheDescriptorInstrumentDetectsADeliberateLeak) {
    const int before = OpenDescriptorCount();
    const int leaked = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(leaked, 0);
    EXPECT_EQ(before + 1, OpenDescriptorCount());
    ::close(leaked);
    EXPECT_EQ(before, OpenDescriptorCount());
}

// An accepted construction takes ownership, so the descriptor count returns to its baseline when
// the stream is destroyed -- the other direction of the same accounting.
TEST(NetworkStreamConstructionTests, AnAcceptedConstructionOwnsAndReleasesExactlyOneDescriptor) {
    StreamPair pair;
    ASSERT_GE(pair.fds[0], 0);
    const int before = OpenDescriptorCount();
    {
        NetworkStream stream(pair.release(0));
        EXPECT_EQ(before, OpenDescriptorCount()) << "construction must not open a descriptor";
    }
    EXPECT_EQ(before - 1, OpenDescriptorCount()) << "destruction must close the owned descriptor";
}

// ===========================================================================================
// Cause NS-B -- a closed stream throws instead of reporting success
// ===========================================================================================

TEST(NetworkStreamClosedStateTests, AClosedStreamRefusesBothReadAndWrite) {
    StreamPair pair;
    ASSERT_GE(pair.fds[0], 0);
    NetworkStream stream(pair.release(0));
    stream.Close();

    SharpRuntime::bytecs buffer[4] = {1, 2, 3, 4};
    // Read used to answer 0 -- a clean end-of-stream, as far as any caller could tell.
    EXPECT_THROW((void)stream.Read(buffer, 0, 4), System::ObjectDisposedException);
    // Write used to return normally having written nothing. This is the defect that matters:
    // Write is `void`, so silence is the only thing a caller ever sees.
    EXPECT_THROW(stream.Write(buffer, 0, 4), System::ObjectDisposedException);
    // A zero-length transfer is refused too: the state, not the size, is what is wrong.
    EXPECT_THROW((void)stream.Read(buffer, 0, 0), System::ObjectDisposedException);
    EXPECT_THROW(stream.Write(buffer, 0, 0), System::ObjectDisposedException);
}

TEST(NetworkStreamClosedStateTests, EveryClosedRejectionCarriesTheSameClosedStreamMessage) {
    StreamPair pair;
    ASSERT_GE(pair.fds[0], 0);
    NetworkStream stream(pair.release(0));
    stream.Close();

    SharpRuntime::bytecs buffer[4] = {};
    const std::string expected = "Cannot access a closed Stream.";
    EXPECT_NE(MessageOf([&] { (void)stream.Read(buffer, 0, 4); }).find(expected), std::string::npos);
    EXPECT_NE(MessageOf([&] { stream.Write(buffer, 0, 4); }).find(expected), std::string::npos);
}

// PIN of the ORDER, which is the part a repair can silently get wrong. A caller who passes a bad
// argument to a closed stream has two defects; the one under their control is the argument, so
// that is what they hear about. Both checks fire -- neither hides the other.
TEST(NetworkStreamClosedStateTests, THEPINArgumentValidationStillPrecedesTheClosedCheck) {
    StreamPair pair;
    ASSERT_GE(pair.fds[0], 0);
    NetworkStream stream(pair.release(0));
    stream.Close();

    SharpRuntime::bytecs buffer[4] = {};
    EXPECT_THROW((void)stream.Read(buffer, -1, 1), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)stream.Read(buffer, 0, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(stream.Write(buffer, -1, 1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(stream.Write(buffer, 0, -1), System::ArgumentOutOfRangeException);
}

// THE CONTROL for the closed check: an OPEN stream still reads and writes, and still reports its
// capabilities. Without this, "everything throws" would pass the tests above.
TEST(NetworkStreamClosedStateTests, THECONTROLAnOpenStreamStillTransfersBytes) {
    StreamPair pair;
    ASSERT_GE(pair.fds[0], 0);
    ASSERT_GE(pair.fds[1], 0);
    NetworkStream writer(pair.release(0));
    NetworkStream reader(pair.release(1));

    EXPECT_TRUE(writer.getCanWriteProperty());
    EXPECT_TRUE(reader.getCanReadProperty());

    SharpRuntime::bytecs out[3] = {0xAA, 0xBB, 0xCC};
    EXPECT_NO_THROW(writer.Write(out, 0, 3));
    EXPECT_NO_THROW(writer.Write(out, 0, 0)) << "a zero-length write on an open stream is a no-op";

    SharpRuntime::bytecs in[3] = {};
    EXPECT_EQ(3, reader.Read(in, 0, 3));
    EXPECT_EQ(0xAA, in[0]);
    EXPECT_EQ(0xCC, in[2]);
    EXPECT_EQ(0, reader.Read(in, 0, 0)) << "a zero-length read on an open stream still answers 0";
}

TEST(NetworkStreamClosedStateTests, THEPINCloseIsIdempotentAndTheCapabilitiesStayFalse) {
    StreamPair pair;
    ASSERT_GE(pair.fds[0], 0);
    NetworkStream stream(pair.release(0));

    EXPECT_NO_THROW(stream.Close());
    EXPECT_NO_THROW(stream.Close()) << "a second Close() must stay a no-op";
    EXPECT_FALSE(stream.getCanReadProperty());
    EXPECT_FALSE(stream.getCanWriteProperty());
}

// PIN: getLengthProperty answers "does not support seeking" rather than a fabricated length, and
// it answers it the same way before and after Close(). It is NOT folded into the closed-state
// contract, because "this stream cannot seek" is true of an open one too.
TEST(NetworkStreamClosedStateTests, THEPINGetLengthStillThrowsItsSeekMessageOpenAndClosed) {
    StreamPair pair;
    ASSERT_GE(pair.fds[0], 0);
    NetworkStream stream(pair.release(0));

    EXPECT_THROW((void)stream.getLengthProperty(), System::NotSupportedException);
    EXPECT_NE(MessageOf([&] { (void)stream.getLengthProperty(); }).find("does not support seeking"),
              std::string::npos);

    stream.Close();
    EXPECT_THROW((void)stream.getLengthProperty(), System::NotSupportedException);
}

#endif  // !_WIN32 && !__EMSCRIPTEN__
