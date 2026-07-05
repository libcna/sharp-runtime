// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for System::IO enums, exceptions, and structural option types.
// Functional I/O (File, FileStream, BinaryReader/Writer, StreamReader/Writer, etc.)
// is covered in IOStreamTests.cpp.
#include <gtest/gtest.h>
#include <string>
#include "System/IO/FileMode.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileShare.hpp"
#include "System/IO/FileAttributes.hpp"
#include "System/IO/FileOptions.hpp"
#include "System/IO/SeekOrigin.hpp"
#include "System/IO/SearchOption.hpp"
#include "System/IO/SearchTarget.hpp"
#include "System/IO/MatchCasing.hpp"
#include "System/IO/MatchType.hpp"
#include "System/IO/HandleInheritability.hpp"
#include "System/IO/UnixFileMode.hpp"
#include "System/IO/IOException.hpp"
#include "System/IO/FileNotFoundException.hpp"
#include "System/IO/DirectoryNotFoundException.hpp"
#include "System/IO/EndOfStreamException.hpp"
#include "System/IO/PathTooLongException.hpp"
#include "System/IO/FileLoadException.hpp"
#include "System/IO/InvalidDataException.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageScope.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageException.hpp"
#include "System/IO/EnumerationOptions.hpp"
#include "System/IO/FileStreamOptions.hpp"
#include "System/IO/DriveInfo.hpp"

using System::IO::FileMode;
using System::IO::FileAccess;
using System::IO::FileShare;
using System::IO::FileAttributes;
using System::IO::FileOptions;
using System::IO::SeekOrigin;
using System::IO::SearchOption;
using System::IO::SearchTarget;
using System::IO::MatchCasing;
using System::IO::MatchType;
using System::IO::HandleInheritability;
using System::IO::UnixFileMode;
using System::IO::IOException;
using System::IO::FileNotFoundException;
using System::IO::DirectoryNotFoundException;
using System::IO::EndOfStreamException;
using System::IO::PathTooLongException;
using System::IO::FileLoadException;
using System::IO::InvalidDataException;
using System::IO::IsolatedStorage::IsolatedStorageScope;
using System::IO::IsolatedStorage::IsolatedStorageException;
using System::IO::EnumerationOptions;
using System::IO::FileStreamOptions;
using System::IO::DriveType;
using System::IO::DriveInfo;

// ===========================================================================
// FileMode
// ===========================================================================

TEST(FileModeTests, CreateNew_IsOne) {
    EXPECT_EQ(static_cast<int>(FileMode::CreateNew), 1);
}

TEST(FileModeTests, Open_IsThree) {
    EXPECT_EQ(static_cast<int>(FileMode::Open), 3);
}

TEST(FileModeTests, Append_IsSix) {
    EXPECT_EQ(static_cast<int>(FileMode::Append), 6);
}

// ===========================================================================
// FileAccess
// ===========================================================================

TEST(FileAccessTests, Read_IsOne) {
    EXPECT_EQ(static_cast<int>(FileAccess::Read), 1);
}

TEST(FileAccessTests, Write_IsTwo) {
    EXPECT_EQ(static_cast<int>(FileAccess::Write), 2);
}

TEST(FileAccessTests, ReadWrite_IsThree) {
    EXPECT_EQ(static_cast<int>(FileAccess::ReadWrite), 3);
}

// ===========================================================================
// FileShare
// ===========================================================================

TEST(FileShareTests, None_IsZero) {
    EXPECT_EQ(static_cast<int>(FileShare::None), 0);
}

TEST(FileShareTests, ReadWrite_IsThree) {
    EXPECT_EQ(static_cast<int>(FileShare::ReadWrite), 3);
}

TEST(FileShareTests, Inheritable_Is16) {
    EXPECT_EQ(static_cast<int>(FileShare::Inheritable), 16);
}

// ===========================================================================
// FileAttributes
// ===========================================================================

TEST(FileAttributesTests, None_IsZero) {
    EXPECT_EQ(static_cast<int>(FileAttributes::None), 0);
}

TEST(FileAttributesTests, ReadOnly_IsOne) {
    EXPECT_EQ(static_cast<int>(FileAttributes::ReadOnly), 1);
}

TEST(FileAttributesTests, Hidden_IsTwo) {
    EXPECT_EQ(static_cast<int>(FileAttributes::Hidden), 2);
}

TEST(FileAttributesTests, Operator_OR) {
    auto r = FileAttributes::ReadOnly | FileAttributes::Hidden;
    EXPECT_EQ(static_cast<int>(r), 3);
}

TEST(FileAttributesTests, Operator_AND) {
    auto r = (FileAttributes::ReadOnly | FileAttributes::Hidden) & FileAttributes::ReadOnly;
    EXPECT_EQ(static_cast<int>(r), 1);
}

// ===========================================================================
// FileOptions
// ===========================================================================

TEST(FileOptionsTests, None_IsZero) {
    EXPECT_EQ(static_cast<int>(FileOptions::None), 0);
}

TEST(FileOptionsTests, RandomAccess_Value) {
    EXPECT_EQ(static_cast<int>(FileOptions::RandomAccess), 0x10000000);
}

TEST(FileOptionsTests, Operator_OR) {
    auto r = FileOptions::RandomAccess | FileOptions::DeleteOnClose;
    EXPECT_EQ(static_cast<int>(r), 0x10000000 | 0x04000000);
}

// ===========================================================================
// SeekOrigin
// ===========================================================================

TEST(SeekOriginTests, Begin_IsZero) {
    EXPECT_EQ(static_cast<int>(SeekOrigin::Begin), 0);
}

TEST(SeekOriginTests, Current_IsOne) {
    EXPECT_EQ(static_cast<int>(SeekOrigin::Current), 1);
}

TEST(SeekOriginTests, End_IsTwo) {
    EXPECT_EQ(static_cast<int>(SeekOrigin::End), 2);
}

// ===========================================================================
// SearchOption
// ===========================================================================

TEST(SearchOptionTests, TopDirectoryOnly_IsZero) {
    EXPECT_EQ(static_cast<int>(SearchOption::TopDirectoryOnly), 0);
}

TEST(SearchOptionTests, AllDirectories_IsOne) {
    EXPECT_EQ(static_cast<int>(SearchOption::AllDirectories), 1);
}

// ===========================================================================
// SearchTarget
// ===========================================================================

TEST(SearchTargetTests, Files_IsOne) {
    EXPECT_EQ(static_cast<int>(SearchTarget::Files), 1);
}

TEST(SearchTargetTests, Directories_IsTwo) {
    EXPECT_EQ(static_cast<int>(SearchTarget::Directories), 2);
}

TEST(SearchTargetTests, Both_IsThree) {
    EXPECT_EQ(static_cast<int>(SearchTarget::Both), 3);
}

// ===========================================================================
// MatchCasing
// ===========================================================================

TEST(MatchCasingTests, PlatformDefault_IsZero) {
    EXPECT_EQ(static_cast<int>(MatchCasing::PlatformDefault), 0);
}

TEST(MatchCasingTests, CaseSensitive_IsOne) {
    EXPECT_EQ(static_cast<int>(MatchCasing::CaseSensitive), 1);
}

TEST(MatchCasingTests, CaseInsensitive_IsTwo) {
    EXPECT_EQ(static_cast<int>(MatchCasing::CaseInsensitive), 2);
}

// ===========================================================================
// MatchType
// ===========================================================================

TEST(MatchTypeTests, Simple_IsZero) {
    EXPECT_EQ(static_cast<int>(MatchType::Simple), 0);
}

TEST(MatchTypeTests, Win32_IsOne) {
    EXPECT_EQ(static_cast<int>(MatchType::Win32), 1);
}

// ===========================================================================
// HandleInheritability
// ===========================================================================

TEST(HandleInheritabilityTests, None_IsZero) {
    EXPECT_EQ(static_cast<int>(HandleInheritability::None), 0);
}

TEST(HandleInheritabilityTests, Inheritable_IsOne) {
    EXPECT_EQ(static_cast<int>(HandleInheritability::Inheritable), 1);
}

// ===========================================================================
// UnixFileMode
// ===========================================================================

TEST(UnixFileModeTests, None_IsZero) {
    EXPECT_EQ(static_cast<int>(UnixFileMode::None), 0);
}

TEST(UnixFileModeTests, UserRead_IsOctal400) {
    EXPECT_EQ(static_cast<int>(UnixFileMode::UserRead), 0400);
}

TEST(UnixFileModeTests, UserWrite_IsOctal200) {
    EXPECT_EQ(static_cast<int>(UnixFileMode::UserWrite), 0200);
}

TEST(UnixFileModeTests, Operator_OR) {
    auto r = UnixFileMode::UserRead | UnixFileMode::UserWrite;
    EXPECT_EQ(static_cast<int>(r), 0400 | 0200);
}

TEST(UnixFileModeTests, Operator_AND) {
    auto r = (UnixFileMode::UserRead | UnixFileMode::UserWrite) & UnixFileMode::UserRead;
    EXPECT_EQ(static_cast<int>(r), 0400);
}

// ===========================================================================
// IOException
// ===========================================================================

TEST(IOExceptionTests, DefaultCtor_NoThrow) {
    EXPECT_NO_THROW(IOException{});
}

TEST(IOExceptionTests, CharPtrCtor_WhatContainsMessage) {
    IOException ex("disk full");
    EXPECT_NE(std::string(ex.what()).find("disk full"), std::string::npos);
}

TEST(IOExceptionTests, StringCtor_WhatContainsMessage) {
    IOException ex(std::string("read error"));
    EXPECT_NE(std::string(ex.what()).find("read error"), std::string::npos);
}

TEST(IOExceptionTests, IsA_SystemException) {
    EXPECT_THROW(throw IOException("err"), System::SystemException);
}

// ===========================================================================
// FileNotFoundException
// ===========================================================================

TEST(FileNotFoundExceptionTests, DefaultCtor_NoThrow) {
    EXPECT_NO_THROW(FileNotFoundException{});
}

TEST(FileNotFoundExceptionTests, MessageCtor_WhatContainsMessage) {
    FileNotFoundException ex("file not found");
    EXPECT_NE(std::string(ex.what()).find("file not found"), std::string::npos);
}

TEST(FileNotFoundExceptionTests, MessageFilenameCtor_WhatContainsMessage) {
    FileNotFoundException ex("not found", "/tmp/missing.txt");
    EXPECT_NE(std::string(ex.what()).find("not found"), std::string::npos);
}

TEST(FileNotFoundExceptionTests, getFileNameProperty_ReturnsFilename) {
    FileNotFoundException ex("not found", "/tmp/missing.txt");
    EXPECT_EQ(ex.getFileNameProperty(), "/tmp/missing.txt");
}

TEST(FileNotFoundExceptionTests, IsA_IOException) {
    EXPECT_THROW(throw FileNotFoundException("err"), IOException);
}

// ===========================================================================
// DirectoryNotFoundException
// ===========================================================================

TEST(DirectoryNotFoundExceptionTests, DefaultCtor_NoThrow) {
    EXPECT_NO_THROW(DirectoryNotFoundException{});
}

TEST(DirectoryNotFoundExceptionTests, MessageCtor_WhatContainsMessage) {
    DirectoryNotFoundException ex("no such dir");
    EXPECT_NE(std::string(ex.what()).find("no such dir"), std::string::npos);
}

TEST(DirectoryNotFoundExceptionTests, MessageAndPathCtor_StoresDirectoryPath) {
    DirectoryNotFoundException ex("no such dir", "/tmp/missing");
    EXPECT_EQ(ex.getDirectoryPathProperty(), "/tmp/missing");
}

TEST(DirectoryNotFoundExceptionTests, MessageAndInnerCtor_NoThrow) {
    EXPECT_NO_THROW(DirectoryNotFoundException("wrapped", std::exception_ptr{}));
}

// ===========================================================================
// EndOfStreamException
// ===========================================================================

TEST(EndOfStreamExceptionTests, DefaultCtor_NoThrow) {
    EXPECT_NO_THROW(EndOfStreamException{});
}

TEST(EndOfStreamExceptionTests, MessageCtor_WhatContainsMessage) {
    EndOfStreamException ex("end of stream");
    EXPECT_NE(std::string(ex.what()).find("end of stream"), std::string::npos);
}

// ===========================================================================
// PathTooLongException
// ===========================================================================

TEST(PathTooLongExceptionTests, DefaultCtor_WhatNotEmpty) {
    PathTooLongException ex;
    EXPECT_FALSE(std::string(ex.what()).empty());
}

TEST(PathTooLongExceptionTests, MessageCtor_WhatContainsMessage) {
    PathTooLongException ex("path is too long");
    EXPECT_NE(std::string(ex.what()).find("path is too long"), std::string::npos);
}

// ===========================================================================
// FileLoadException
// ===========================================================================

TEST(FileLoadExceptionTests, DefaultCtor_WhatNotEmpty) {
    FileLoadException ex;
    EXPECT_FALSE(std::string(ex.what()).empty());
}

TEST(FileLoadExceptionTests, MessageCtor_WhatContainsMessage) {
    FileLoadException ex("cannot load file");
    EXPECT_NE(std::string(ex.what()).find("cannot load file"), std::string::npos);
}

TEST(FileLoadExceptionTests, MessageFilenameCtor_getFileNameProperty) {
    FileLoadException ex("failed", "bad.dll");
    EXPECT_EQ(ex.getFileNameProperty(), "bad.dll");
}

// ===========================================================================
// InvalidDataException
// ===========================================================================

TEST(InvalidDataExceptionTests, DefaultCtor_NoThrow) {
    EXPECT_NO_THROW(InvalidDataException{});
}

TEST(InvalidDataExceptionTests, MessageCtor_WhatContainsMessage) {
    InvalidDataException ex("corrupt data");
    EXPECT_NE(std::string(ex.what()).find("corrupt data"), std::string::npos);
}

// ===========================================================================
// IsolatedStorageScope
// ===========================================================================

TEST(IsolatedStorageScopeTests, None_IsZero) {
    EXPECT_EQ(static_cast<int>(IsolatedStorageScope::None), 0x00);
}

TEST(IsolatedStorageScopeTests, User_IsOne) {
    EXPECT_EQ(static_cast<int>(IsolatedStorageScope::User), 0x01);
}

TEST(IsolatedStorageScopeTests, Assembly_IsFour) {
    EXPECT_EQ(static_cast<int>(IsolatedStorageScope::Assembly), 0x04);
}

TEST(IsolatedStorageScopeTests, Machine_Is16) {
    EXPECT_EQ(static_cast<int>(IsolatedStorageScope::Machine), 0x10);
}

TEST(IsolatedStorageScopeTests, Operator_OR) {
    auto r = IsolatedStorageScope::User | IsolatedStorageScope::Assembly;
    EXPECT_EQ(static_cast<int>(r), 0x05);
}

// ===========================================================================
// IsolatedStorageException
// ===========================================================================

TEST(IsolatedStorageExceptionTests, MessageCtor_WhatContainsMessage) {
    IsolatedStorageException ex("quota exceeded");
    EXPECT_NE(std::string(ex.what()).find("quota exceeded"), std::string::npos);
}

TEST(IsolatedStorageExceptionTests, IsA_Exception) {
    EXPECT_THROW(throw IsolatedStorageException("err"), System::Exception);
}

// ===========================================================================
// EnumerationOptions
// ===========================================================================

TEST(EnumerationOptionsTests, DefaultCtor_NoThrow) {
    EXPECT_NO_THROW(EnumerationOptions{});
}

TEST(EnumerationOptionsTests, RecurseSubdirectories_DefaultFalse) {
    EnumerationOptions opts;
    EXPECT_FALSE(opts.RecurseSubdirectories);
}

TEST(EnumerationOptionsTests, IgnoreInaccessible_DefaultTrue) {
    EnumerationOptions opts;
    EXPECT_TRUE(opts.IgnoreInaccessible);
}

TEST(EnumerationOptionsTests, ReturnSpecialDirectories_DefaultFalse) {
    EnumerationOptions opts;
    EXPECT_FALSE(opts.ReturnSpecialDirectories);
}

// ===========================================================================
// FileStreamOptions
// ===========================================================================

TEST(FileStreamOptionsTests, DefaultCtor_NoThrow) {
    EXPECT_NO_THROW(FileStreamOptions{});
}

TEST(FileStreamOptionsTests, DefaultMode_IsOpen) {
    FileStreamOptions opts;
    EXPECT_EQ(opts.Mode, FileMode::Open);
}

TEST(FileStreamOptionsTests, DefaultAccess_IsRead) {
    FileStreamOptions opts;
    EXPECT_EQ(opts.Access, FileAccess::Read);
}

TEST(FileStreamOptionsTests, DefaultShare_IsRead) {
    FileStreamOptions opts;
    EXPECT_EQ(opts.Share, FileShare::Read);
}

// ===========================================================================
// DriveType
// ===========================================================================

TEST(DriveTypeTests, Unknown_IsZero) {
    EXPECT_EQ(static_cast<int>(DriveType::Unknown), 0);
}

TEST(DriveTypeTests, Fixed_IsThree) {
    EXPECT_EQ(static_cast<int>(DriveType::Fixed), 3);
}

TEST(DriveTypeTests, CDRom_IsFive) {
    EXPECT_EQ(static_cast<int>(DriveType::CDRom), 5);
}

// ===========================================================================
// DriveInfo
// ===========================================================================

TEST(DriveInfoTests, Constructor_StoresName) {
    DriveInfo d("/");
    EXPECT_EQ(d.getNameProperty(), "/");
}

TEST(DriveInfoTests, IsReady_RootDirExists) {
    DriveInfo d("/");
    EXPECT_TRUE(d.getIsReadyProperty());
}

TEST(DriveInfoTests, DriveType_IsFixed) {
    DriveInfo d("/");
    EXPECT_EQ(d.getDriveTypeProperty(), DriveType::Fixed);
}

TEST(DriveInfoTests, ToString_ReturnsName) {
    DriveInfo d("/");
    EXPECT_EQ(d.ToString(), "/");
}

TEST(DriveInfoTests, GetDrives_NotEmpty) {
    auto drives = DriveInfo::GetDrives();
    EXPECT_FALSE(drives.empty());
}
