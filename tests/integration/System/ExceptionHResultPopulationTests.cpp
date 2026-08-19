// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include <gtest/gtest.h>

#include <exception>
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"
#include "System/ComponentModel/Win32Exception.hpp"
#include "System/Diagnostics/UnreachableException.hpp"
#include "System/Globalization/CultureNotFoundException.hpp"
#include "System/IO/Compression/ZLibException.hpp"
#include "System/IO/FileFormatException.hpp"
#include "System/IO/InvalidDataException.hpp"
#include "System/IO/PathTooLongException.hpp"
#include "System/Net/CookieException.hpp"
#include "System/Net/Http/HttpIOException.hpp"
#include "System/Net/Http/HttpProtocolException.hpp"
#include "System/Net/Http/HttpRequestException.hpp"
#include "System/Net/NetworkInformation/NetworkInformationException.hpp"
#include "System/Net/NetworkInformation/PingException.hpp"
#include "System/Net/ProtocolViolationException.hpp"
#include "System/Net/Sockets/SocketException.hpp"
#include "System/Net/WebException.hpp"
#include "System/Net/WebSockets/WebSocketException.hpp"
#include "System/Runtime/AmbiguousImplementationException.hpp"
#include "System/Runtime/InteropServices/ExternalException.hpp"
#include "System/Security/Authentication/AuthenticationException.hpp"
#include "System/Security/Authentication/InvalidCredentialException.hpp"
#include "System/Security/Cryptography/AuthenticationTagMismatchException.hpp"
#include "System/Security/Cryptography/CryptographicUnexpectedOperationException.hpp"
#include "System/Security/VerificationException.hpp"
#include "System/Text/Json/JsonException.hpp"
#include "System/Text/RegularExpressions/RegexMatchTimeoutException.hpp"
#include "System/Text/RegularExpressions/RegexParseException.hpp"
#include "System/Threading/AbandonedMutexException.hpp"
#include "System/Threading/BarrierPostPhaseException.hpp"
#include "System/Threading/Channels/ChannelClosedException.hpp"
#include "System/Threading/LockRecursionException.hpp"
#include "System/Threading/SemaphoreFullException.hpp"
#include "System/Threading/SynchronizationLockException.hpp"
#include "System/Threading/Tasks/TaskCanceledException.hpp"
#include "System/Threading/Tasks/TaskSchedulerException.hpp"
#include "System/Threading/ThreadAbortException.hpp"
#include "System/Threading/ThreadInterruptedException.hpp"
#include "System/Threading/ThreadStartException.hpp"
#include "System/Threading/ThreadStateException.hpp"
#include "System/Threading/WaitHandleCannotBeOpenedException.hpp"
#include "System/InvalidTimeZoneException.hpp"
#include "System/TimeZoneNotFoundException.hpp"
#include "System/UriFormatException.hpp"
#include "System/Xml/Schema/XmlSchemaValidationException.hpp"

namespace {

using SharpRuntime::intcs;

constexpr intcs kCorEException = static_cast<intcs>(0x80131500u);
constexpr intcs kCorESystem = static_cast<intcs>(0x80131501u);
constexpr intcs kCorEApplication = static_cast<intcs>(0x80131600u);
constexpr intcs kCorEArgument = static_cast<intcs>(0x80070057u);
constexpr intcs kCorEFormat = static_cast<intcs>(0x80131537u);
constexpr intcs kCorEInvalidOperation = static_cast<intcs>(0x80131509u);
constexpr intcs kCorEIo = static_cast<intcs>(0x80131620u);
constexpr intcs kCorEOperationCanceled = static_cast<intcs>(0x8013153Bu);
constexpr intcs kCorETimeout = static_cast<intcs>(0x80131505u);
constexpr intcs kXmlSchema = static_cast<intcs>(0x80131941u);
constexpr intcs kEFail = static_cast<intcs>(0x80004005u);

std::exception_ptr innerException() {
    return std::make_exception_ptr(System::Exception("inner"));
}

template <typename T>
void expectHResult(const T& exception, intcs expected) {
    EXPECT_EQ(exception.getHResultProperty(), expected);
}

} // namespace

TEST(ExceptionHResultPopulationTests, KeyNotFoundException_EveryRepresentedPublicConstructor) {
    constexpr intcs expected = static_cast<intcs>(0x80131577u); // COR_E_KEYNOTFOUND
    expectHResult(System::Collections::Generic::KeyNotFoundException(), expected);
    expectHResult(System::Collections::Generic::KeyNotFoundException(std::string("m")), expected);
    expectHResult(System::Collections::Generic::KeyNotFoundException(std::string("m"), innerException()), expected);
}

TEST(ExceptionHResultPopulationTests, PathTooLongException_EveryRepresentedPublicConstructor) {
    constexpr intcs expected = static_cast<intcs>(0x800700CEu); // COR_E_PATHTOOLONG
    expectHResult(System::IO::PathTooLongException(), expected);
    expectHResult(System::IO::PathTooLongException(std::string("m")), expected);
    expectHResult(System::IO::PathTooLongException(std::string("m"), innerException()), expected);
}

TEST(ExceptionHResultPopulationTests, AmbiguousImplementationException_EveryRepresentedPublicConstructor) {
    constexpr intcs expected = static_cast<intcs>(0x8013106Au); // COR_E_AMBIGUOUSIMPLEMENTATION
    expectHResult(System::Runtime::AmbiguousImplementationException(), expected);
    expectHResult(System::Runtime::AmbiguousImplementationException(std::string("m")), expected);
}

TEST(ExceptionHResultPopulationTests, ExternalException_EveryRepresentedPublicConstructor) {
    expectHResult(System::Runtime::InteropServices::ExternalException(), kEFail);
    expectHResult(System::Runtime::InteropServices::ExternalException(std::string("m")), kEFail);
    expectHResult(System::Runtime::InteropServices::ExternalException(std::string("m"), innerException()), kEFail);
}

TEST(ExceptionHResultPopulationTests, VerificationException_EveryRepresentedPublicConstructor) {
    constexpr intcs expected = static_cast<intcs>(0x8013150Du); // COR_E_VERIFICATION
    expectHResult(System::Security::VerificationException(), expected);
    expectHResult(System::Security::VerificationException(std::string("m")), expected);
}

TEST(ExceptionHResultPopulationTests, AbandonedMutexException_EveryRepresentedPublicConstructor) {
    constexpr intcs expected = static_cast<intcs>(0x8013152Du); // COR_E_ABANDONEDMUTEX
    expectHResult(System::Threading::AbandonedMutexException(), expected);
    expectHResult(System::Threading::AbandonedMutexException(std::string("m")), expected);
    expectHResult(System::Threading::AbandonedMutexException(std::string("m"), innerException()), expected);
    expectHResult(System::Threading::AbandonedMutexException(3, nullptr), expected);
    expectHResult(System::Threading::AbandonedMutexException(std::string("m"), 3, nullptr), expected);
    expectHResult(System::Threading::AbandonedMutexException(std::string("m"), innerException(), 3, nullptr), expected);
}

TEST(ExceptionHResultPopulationTests, SynchronizationLockException_EveryRepresentedPublicConstructor) {
    constexpr intcs expected = static_cast<intcs>(0x80131518u); // COR_E_SYNCHRONIZATIONLOCK
    expectHResult(System::Threading::SynchronizationLockException(), expected);
    expectHResult(System::Threading::SynchronizationLockException(std::string("m")), expected);
    expectHResult(System::Threading::SynchronizationLockException(std::string("m"), innerException()), expected);
}

TEST(ExceptionHResultPopulationTests, ThreadAbortException_EveryRepresentedPublicConstructor) {
    constexpr intcs expected = static_cast<intcs>(0x80131530u); // COR_E_THREADABORTED
    expectHResult(System::Threading::ThreadAbortException(), expected);
    expectHResult(System::Threading::ThreadAbortException(std::string("m")), expected);
}

TEST(ExceptionHResultPopulationTests, ThreadInterruptedException_EveryRepresentedPublicConstructor) {
    constexpr intcs expected = static_cast<intcs>(0x80131519u); // COR_E_THREADINTERRUPTED
    expectHResult(System::Threading::ThreadInterruptedException(), expected);
    expectHResult(System::Threading::ThreadInterruptedException(std::string("m")), expected);
    expectHResult(System::Threading::ThreadInterruptedException(std::string("m"), innerException()), expected);
}

TEST(ExceptionHResultPopulationTests, ThreadStartException_EveryExposedConstructor) {
    // #1958/SR-AUD-196 reduced this type to .NET's own two constructors: () and (reason). .NET
    // has NO message-taking constructor -- both of its own pass the fixed
    // SR.Arg_ThreadStartException (ThreadStartException.cs:13-24) -- so the two message-taking
    // ones this port had invented are gone. The HResult must still be set by both survivors.
    constexpr intcs expected = static_cast<intcs>(0x80131525u); // COR_E_THREADSTART
    expectHResult(System::Threading::ThreadStartException(), expected);
    expectHResult(System::Threading::ThreadStartException(innerException()), expected);
}

TEST(ExceptionHResultPopulationTests, ThreadStateException_EveryRepresentedPublicConstructor) {
    constexpr intcs expected = static_cast<intcs>(0x80131520u); // COR_E_THREADSTATE
    expectHResult(System::Threading::ThreadStateException(), expected);
    expectHResult(System::Threading::ThreadStateException(std::string("m")), expected);
    expectHResult(System::Threading::ThreadStateException(std::string("m"), innerException()), expected);
}

TEST(ExceptionHResultPopulationTests, WaitHandleCannotBeOpenedException_EveryRepresentedPublicConstructor) {
    constexpr intcs expected = static_cast<intcs>(0x8013152Cu); // COR_E_WAITHANDLECANNOTBEOPENED
    expectHResult(System::Threading::WaitHandleCannotBeOpenedException(), expected);
    expectHResult(System::Threading::WaitHandleCannotBeOpenedException(std::string("m")), expected);
    expectHResult(System::Threading::WaitHandleCannotBeOpenedException(std::string("m"), innerException()), expected);
}

TEST(ExceptionHResultPopulationTests, Win32Family_InheritsExternalEFailForRepresentedConstructors) {
    expectHResult(System::ComponentModel::Win32Exception(5), kEFail);
    expectHResult(System::ComponentModel::Win32Exception(5, "m"), kEFail);
    expectHResult(System::Net::NetworkInformation::NetworkInformationException(5), kEFail);
    expectHResult(System::Net::Sockets::SocketException(5), kEFail);
    expectHResult(System::Net::WebSockets::WebSocketException(System::Net::WebSockets::WebSocketError::HeaderError), kEFail);
}

TEST(ExceptionHResultPopulationTests, PureReferenceInheritancePopulation_IsPinned) {
    expectHResult(System::Diagnostics::UnreachableException(), kCorEException);
    expectHResult(System::Globalization::CultureNotFoundException(), kCorEArgument);
    expectHResult(System::IO::Compression::ZLibException(), kCorEIo);
    expectHResult(System::IO::FileFormatException(), kCorEFormat);
    expectHResult(System::IO::InvalidDataException(), kCorESystem);
    expectHResult(System::Net::Http::HttpIOException(System::Net::Http::HttpRequestError::ConnectionError), kCorEIo);
    expectHResult(System::Net::Http::HttpProtocolException(1, "m", nullptr), kCorEIo);
    expectHResult(System::Net::NetworkInformation::PingException("m"), kCorEInvalidOperation);
    expectHResult(System::Net::CookieException(), kCorEFormat);
    expectHResult(System::Net::ProtocolViolationException(), kCorEInvalidOperation);
    expectHResult(System::Security::Cryptography::AuthenticationTagMismatchException(), kCorESystem);
    expectHResult(System::Security::Cryptography::CryptographicUnexpectedOperationException(), kCorESystem);
    expectHResult(System::Security::Authentication::AuthenticationException(), kCorESystem);
    expectHResult(System::Security::Authentication::InvalidCredentialException(), kCorESystem);
    expectHResult(System::Text::Json::JsonException(), kCorEException);
    expectHResult(System::Text::RegularExpressions::RegexMatchTimeoutException(), kCorETimeout);
    expectHResult(System::Text::RegularExpressions::RegexParseException(
                      System::Text::RegularExpressions::RegexParseError::Unknown, "m"),
                  kCorEArgument);
    expectHResult(System::Threading::Channels::ChannelClosedException(), kCorEInvalidOperation);
    expectHResult(System::Threading::Tasks::TaskCanceledException(), kCorEOperationCanceled);
    expectHResult(System::Threading::Tasks::TaskSchedulerException(), kCorEException);
    expectHResult(System::Threading::BarrierPostPhaseException(), kCorEException);
    expectHResult(System::Threading::LockRecursionException(), kCorEException);
    expectHResult(System::Threading::SemaphoreFullException(), kCorESystem);
    expectHResult(System::InvalidTimeZoneException(), kCorEException);
    expectHResult(System::TimeZoneNotFoundException(), kCorEException);
    expectHResult(System::UriFormatException(), kCorEFormat);
    expectHResult(System::Xml::Schema::XmlSchemaValidationException(), kXmlSchema);
}

TEST(ExceptionHResultPopulationTests, ConditionalPropagationTypes_KeepReferenceBaseValueWithoutInnerCause) {
    expectHResult(System::Net::Http::HttpRequestException(), kCorEException);
    expectHResult(System::Net::WebException(), kCorEInvalidOperation);
}
