// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Functional tests for System::IO: Path, File, FileInfo, Directory, DirectoryInfo,
// BinaryReader/Writer, StreamReader/Writer, BufferedStream, FileStream, IsolatedStorageFile.
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cstdint>
#include "System/IO/Path.hpp"
#include "System/IO/File.hpp"
#include "System/IO/FileInfo.hpp"
#include "System/IO/Directory.hpp"
#include "System/IO/DirectoryInfo.hpp"
#include "System/IO/BinaryReader.hpp"
#include "System/IO/BinaryWriter.hpp"
#include "System/IO/StreamReader.hpp"
#include "System/IO/StreamWriter.hpp"
#include "System/IO/BufferedStream.hpp"
#include "System/IO/FileStream.hpp"
#include "System/IO/MemoryStream.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileNotFoundException.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageFile.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageFileStream.hpp"

using System::IO::Path;
using System::IO::File;
using System::IO::FileInfo;
using System::IO::Directory;
using System::IO::DirectoryInfo;
using System::IO::BinaryReader;
using System::IO::BinaryWriter;
using System::IO::StreamReader;
using System::IO::StreamWriter;
using System::IO::BufferedStream;
using System::IO::FileStream;
using System::IO::MemoryStream;
using System::IO::FileMode;
using System::IO::FileAccess;
using System::IO::IsolatedStorage::IsolatedStorageFile;

static std::string tf(const char* name) {
    return std::string("/tmp/sharp_rt_io_") + name;
}

// ===========================================================================
// Path
// ===========================================================================

TEST(PathTests, DirectorySeparatorChar_IsForwardSlash) {
    EXPECT_EQ(Path::DirectorySeparatorChar, '/');
}

TEST(PathTests, Combine_TwoParts) {
    EXPECT_EQ(Path::Combine("/foo", "bar"), "/foo/bar");
}

TEST(PathTests, Combine_TwoParts_TrailingSlash) {
    EXPECT_EQ(Path::Combine("/foo/", "bar"), "/foo/bar");
}

TEST(PathTests, Combine_ThreeParts) {
    std::string r = Path::Combine("/a", "b", "c");
    EXPECT_NE(r.find("a"), std::string::npos);
    EXPECT_NE(r.find("b"), std::string::npos);
    EXPECT_NE(r.find("c"), std::string::npos);
}

TEST(PathTests, GetFileName_WithDirectory) {
    EXPECT_EQ(Path::GetFileName("/foo/bar.txt"), "bar.txt");
}

TEST(PathTests, GetFileName_NoDirectory) {
    EXPECT_EQ(Path::GetFileName("file.txt"), "file.txt");
}

TEST(PathTests, GetFileNameWithoutExtension) {
    EXPECT_EQ(Path::GetFileNameWithoutExtension("/foo/bar.txt"), "bar");
}

TEST(PathTests, GetExtension_WithDot) {
    EXPECT_EQ(Path::GetExtension("file.txt"), ".txt");
}

TEST(PathTests, GetExtension_NoDot) {
    EXPECT_EQ(Path::GetExtension("readme"), "");
}

TEST(PathTests, GetDirectoryName_AbsolutePath) {
    EXPECT_EQ(Path::GetDirectoryName("/foo/bar.txt"), "/foo");
}

TEST(PathTests, HasExtension_True) {
    EXPECT_TRUE(Path::HasExtension("file.txt"));
}

TEST(PathTests, HasExtension_False) {
    EXPECT_FALSE(Path::HasExtension("readme"));
}

TEST(PathTests, IsPathRooted_AbsolutePath) {
    EXPECT_TRUE(Path::IsPathRooted("/foo/bar"));
}

TEST(PathTests, IsPathRooted_RelativePath) {
    EXPECT_FALSE(Path::IsPathRooted("relative/path"));
}

TEST(PathTests, ChangeExtension_ReplacesExt) {
    std::string r = Path::ChangeExtension("file.txt", ".md");
    EXPECT_NE(r.find(".md"), std::string::npos);
    EXPECT_EQ(r.find(".txt"), std::string::npos);
}

// ===========================================================================
// File
// ===========================================================================

TEST(FileTests, Exists_ExistingFile_True) {
    std::string p = tf("exists_true.txt");
    File::WriteAllText(p, "x");
    EXPECT_TRUE(File::Exists(p));
    File::Delete(p);
}

TEST(FileTests, Exists_NonExistentFile_False) {
    EXPECT_FALSE(File::Exists(tf("nonexistent_xyz_abc.txt")));
}

TEST(FileTests, WriteAllText_ReadAllText_Roundtrip) {
    std::string p = tf("rw_text.txt");
    File::WriteAllText(p, "hello world");
    EXPECT_EQ(File::ReadAllText(p), "hello world");
    File::Delete(p);
}

TEST(FileTests, WriteAllText_Overwrites) {
    std::string p = tf("overwrite.txt");
    File::WriteAllText(p, "first");
    File::WriteAllText(p, "second");
    EXPECT_EQ(File::ReadAllText(p), "second");
    File::Delete(p);
}

TEST(FileTests, WriteAllLines_ReadAllLines_Roundtrip) {
    std::string p = tf("alllines.txt");
    std::vector<std::string> lines = {"line one", "line two", "line three"};
    File::WriteAllLines(p, lines);
    auto read = File::ReadAllLines(p);
    ASSERT_EQ(read.size(), 3u);
    EXPECT_EQ(read[0], "line one");
    EXPECT_EQ(read[2], "line three");
    File::Delete(p);
}

TEST(FileTests, WriteAllBytes_ReadAllBytes_Roundtrip) {
    std::string p = tf("allbytes.bin");
    std::vector<uint8_t> bytes = {10, 20, 30, 40, 50};
    File::WriteAllBytes(p, bytes);
    auto read = File::ReadAllBytes(p);
    ASSERT_EQ(read.size(), 5u);
    EXPECT_EQ(read[0], 10u);
    EXPECT_EQ(read[4], 50u);
    File::Delete(p);
}

TEST(FileTests, AppendAllText_Appends) {
    std::string p = tf("append.txt");
    File::WriteAllText(p, "hello ");
    File::AppendAllText(p, "world");
    EXPECT_EQ(File::ReadAllText(p), "hello world");
    File::Delete(p);
}

TEST(FileTests, Delete_RemovesFile) {
    std::string p = tf("delete_me.txt");
    File::WriteAllText(p, "temp");
    File::Delete(p);
    EXPECT_FALSE(File::Exists(p));
}

TEST(FileTests, Copy_CreatesDestination) {
    std::string src = tf("copy_src.txt");
    std::string dst = tf("copy_dst.txt");
    File::WriteAllText(src, "copy content");
    File::Copy(src, dst, true);
    EXPECT_TRUE(File::Exists(dst));
    EXPECT_EQ(File::ReadAllText(dst), "copy content");
    File::Delete(src);
    File::Delete(dst);
}

TEST(FileTests, Move_RenamesFile) {
    std::string src = tf("move_src.txt");
    std::string dst = tf("move_dst.txt");
    File::WriteAllText(src, "move content");
    File::Move(src, dst);
    EXPECT_FALSE(File::Exists(src));
    EXPECT_TRUE(File::Exists(dst));
    EXPECT_EQ(File::ReadAllText(dst), "move content");
    File::Delete(dst);
}

TEST(FileTests, ReadAllText_ThrowsForNonExistentFile) {
    EXPECT_THROW((void)File::ReadAllText(tf("missing_xyz_abc.txt")),
                 System::IO::FileNotFoundException);
}

// ===========================================================================
// FileInfo
// ===========================================================================

TEST(FileInfoTests, Constructor_NoThrow) {
    EXPECT_NO_THROW(FileInfo{tf("fi_nofile.txt")});
}

TEST(FileInfoTests, getNameProperty_ReturnsFilename) {
    FileInfo fi("/tmp/sharp_rt_io_name_test.txt");
    EXPECT_EQ(fi.getNameProperty(), "sharp_rt_io_name_test.txt");
}

TEST(FileInfoTests, getExtensionProperty_ReturnsExt) {
    FileInfo fi("/foo/bar.txt");
    EXPECT_EQ(fi.getExtensionProperty(), ".txt");
}

TEST(FileInfoTests, getExistsProperty_ExistingFile_True) {
    std::string p = tf("fi_exists.txt");
    File::WriteAllText(p, "hi");
    FileInfo fi(p);
    EXPECT_TRUE(fi.getExistsProperty());
    File::Delete(p);
}

TEST(FileInfoTests, getExistsProperty_NonExistent_False) {
    FileInfo fi(tf("fi_no_such_xyz.txt"));
    EXPECT_FALSE(fi.getExistsProperty());
}

TEST(FileInfoTests, getLengthProperty_MatchesContent) {
    std::string p = tf("fi_length.txt");
    File::WriteAllText(p, "hello");
    FileInfo fi(p);
    EXPECT_EQ(fi.getLengthProperty(), 5LL);
    File::Delete(p);
}

TEST(FileInfoTests, getFullNameProperty_IsAbsolute) {
    FileInfo fi(tf("fi_fullname.txt"));
    std::string full = fi.getFullNameProperty();
    EXPECT_EQ(full[0], '/');
}

// ===========================================================================
// Directory + DirectoryInfo
// ===========================================================================

TEST(DirectoryTests, Exists_ExistingDir_True) {
    EXPECT_TRUE(Directory::Exists("/tmp"));
}

TEST(DirectoryTests, Exists_NonExistentDir_False) {
    EXPECT_FALSE(Directory::Exists(tf("no_such_dir_xyz_abc")));
}

TEST(DirectoryTests, CreateDirectory_Delete_Roundtrip) {
    std::string d = tf("mkdir_test");
    Directory::CreateDirectory(d);
    EXPECT_TRUE(Directory::Exists(d));
    Directory::Delete(d);
    EXPECT_FALSE(Directory::Exists(d));
}

TEST(DirectoryTests, GetCurrentDirectory_NotEmpty) {
    EXPECT_FALSE(Directory::GetCurrentDirectory().empty());
}

TEST(DirectoryInfoTests, Constructor_NoThrow) {
    EXPECT_NO_THROW(DirectoryInfo{"/tmp"});
}

TEST(DirectoryInfoTests, getExistsProperty_Tmp_True) {
    DirectoryInfo di("/tmp");
    EXPECT_TRUE(di.getExistsProperty());
}

TEST(DirectoryInfoTests, getExistsProperty_NonExistent_False) {
    DirectoryInfo di(tf("di_no_such_xyz"));
    EXPECT_FALSE(di.getExistsProperty());
}

TEST(DirectoryInfoTests, Create_Delete_Roundtrip) {
    std::string d = tf("di_mkdir");
    DirectoryInfo di(d);
    di.Create();
    EXPECT_TRUE(di.getExistsProperty());
    di.Delete();
    EXPECT_FALSE(di.getExistsProperty());
}

// ===========================================================================
// BinaryWriter + BinaryReader
// ===========================================================================

TEST(BinaryReaderWriterTests, WriteRead_Int32_Roundtrip) {
    MemoryStream ms;
    BinaryWriter bw(&ms, true);
    bw.Write((int32_t)123456);
    bw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    BinaryReader br(&ms2, true);
    EXPECT_EQ(br.ReadInt32(), 123456);
}

TEST(BinaryReaderWriterTests, WriteRead_NegativeInt32_Roundtrip) {
    MemoryStream ms;
    BinaryWriter bw(&ms, true);
    bw.Write((int32_t)-42);
    bw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    BinaryReader br(&ms2, true);
    EXPECT_EQ(br.ReadInt32(), -42);
}

TEST(BinaryReaderWriterTests, WriteRead_Int16_Roundtrip) {
    MemoryStream ms;
    BinaryWriter bw(&ms, true);
    bw.Write((int16_t)1000);
    bw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    BinaryReader br(&ms2, true);
    EXPECT_EQ(br.ReadInt16(), 1000);
}

TEST(BinaryReaderWriterTests, WriteRead_UInt16_Roundtrip) {
    MemoryStream ms;
    BinaryWriter bw(&ms, true);
    bw.Write((uint16_t)65000u);
    bw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    BinaryReader br(&ms2, true);
    EXPECT_EQ(br.ReadUInt16(), 65000u);
}

TEST(BinaryReaderWriterTests, WriteRead_Int64_Roundtrip) {
    MemoryStream ms;
    BinaryWriter bw(&ms, true);
    bw.Write((int64_t)9876543210LL);
    bw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    BinaryReader br(&ms2, true);
    EXPECT_EQ(br.ReadInt64(), 9876543210LL);
}

TEST(BinaryReaderWriterTests, WriteRead_Byte_Roundtrip) {
    MemoryStream ms;
    BinaryWriter bw(&ms, true);
    bw.Write((uint8_t)0xAB);
    bw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    BinaryReader br(&ms2, true);
    EXPECT_EQ(br.ReadByte(), 0xABu);
}

TEST(BinaryReaderWriterTests, WriteRead_Boolean_True_Roundtrip) {
    MemoryStream ms;
    BinaryWriter bw(&ms, true);
    bw.Write(true);
    bw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    BinaryReader br(&ms2, true);
    EXPECT_TRUE(br.ReadBoolean());
}

TEST(BinaryReaderWriterTests, WriteRead_Boolean_False_Roundtrip) {
    MemoryStream ms;
    BinaryWriter bw(&ms, true);
    bw.Write(false);
    bw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    BinaryReader br(&ms2, true);
    EXPECT_FALSE(br.ReadBoolean());
}

TEST(BinaryReaderWriterTests, WriteRead_Single_Roundtrip) {
    MemoryStream ms;
    BinaryWriter bw(&ms, true);
    bw.Write(3.14f);
    bw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    BinaryReader br(&ms2, true);
    EXPECT_FLOAT_EQ(br.ReadSingle(), 3.14f);
}

TEST(BinaryReaderWriterTests, WriteRead_Double_Roundtrip) {
    MemoryStream ms;
    BinaryWriter bw(&ms, true);
    bw.Write(2.718281828);
    bw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    BinaryReader br(&ms2, true);
    EXPECT_DOUBLE_EQ(br.ReadDouble(), 2.718281828);
}

TEST(BinaryReaderWriterTests, WriteRead_String_Roundtrip) {
    MemoryStream ms;
    BinaryWriter bw(&ms, true);
    bw.Write(std::string("hello sharp"));
    bw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    BinaryReader br(&ms2, true);
    EXPECT_EQ(br.ReadString(), "hello sharp");
}

TEST(BinaryReaderWriterTests, BinaryWriter_BaseStreamProperty) {
    MemoryStream ms;
    BinaryWriter bw(&ms, true);
    EXPECT_EQ(bw.getBaseStreamProperty(), &ms);
}

// ===========================================================================
// StreamWriter + StreamReader
// ===========================================================================

TEST(StreamWriterReaderTests, WriteString_ReadToEnd_Roundtrip) {
    MemoryStream ms;
    StreamWriter sw(&ms, true);
    sw.Write(std::string("sharp runtime"));
    sw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    StreamReader sr(&ms2);
    EXPECT_EQ(sr.ReadToEnd(), "sharp runtime");
}

TEST(StreamWriterReaderTests, WriteLine_AppendsNewline) {
    MemoryStream ms;
    StreamWriter sw(&ms, true);
    sw.WriteLine("test");
    sw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    StreamReader sr(&ms2);
    std::string result = sr.ReadToEnd();
    EXPECT_NE(result.find("test"), std::string::npos);
    EXPECT_NE(result.find('\n'), std::string::npos);
}

TEST(StreamWriterReaderTests, WriteInt_Roundtrip) {
    MemoryStream ms;
    StreamWriter sw(&ms, true);
    sw.Write((int32_t)42);
    sw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    StreamReader sr(&ms2);
    EXPECT_EQ(sr.ReadToEnd(), "42");
}

TEST(StreamWriterReaderTests, WriteBool_True) {
    MemoryStream ms;
    StreamWriter sw(&ms, true);
    sw.Write(true);
    sw.Flush();
    auto buf = ms.ToArray();
    MemoryStream ms2(buf.data(), (int32_t)buf.size());
    StreamReader sr(&ms2);
    EXPECT_FALSE(sr.ReadToEnd().empty());
}

TEST(StreamWriterReaderTests, WriteToFile_ReadBackWithFile) {
    std::string p = tf("sw_file.txt");
    {
        StreamWriter sw(p);
        sw.Write(std::string("from file writer"));
        sw.Close();
    }
    EXPECT_EQ(File::ReadAllText(p), "from file writer");
    File::Delete(p);
}

TEST(StreamWriterReaderTests, StreamWriter_BaseStreamProperty) {
    MemoryStream ms;
    StreamWriter sw(&ms, true);
    EXPECT_EQ(sw.getBaseStreamProperty(), &ms);
}

// ===========================================================================
// BufferedStream
// ===========================================================================

TEST(BufferedStreamTests, Constructor_NoThrow) {
    MemoryStream ms;
    EXPECT_NO_THROW(BufferedStream{&ms});
}

TEST(BufferedStreamTests, DelegatesWrite_AndRead) {
    MemoryStream ms;
    BufferedStream bs(&ms);
    uint8_t writeData[] = {1, 2, 3, 4};
    bs.Write(writeData, 0, 4);
    EXPECT_EQ(ms.getLengthProperty(), 4);
}

TEST(BufferedStreamTests, DelegatesGetCanWriteProperty) {
    MemoryStream ms;
    BufferedStream bs(&ms);
    EXPECT_EQ(bs.getCanWriteProperty(), ms.getCanWriteProperty());
}

TEST(BufferedStreamTests, DelegatesGetLengthProperty) {
    MemoryStream ms;
    uint8_t data[] = {10, 20};
    ms.Write(data, 0, 2);
    BufferedStream bs(&ms);
    EXPECT_EQ(bs.getLengthProperty(), 2);
}

// ===========================================================================
// FileStream
// ===========================================================================

TEST(FileStreamTests, CreateAndOpen_WriteReadRoundtrip) {
    std::string p = tf("fstream_rw.bin");
    {
        FileStream fs(p, FileMode::Create, FileAccess::Write);
        uint8_t data[] = {10, 20, 30, 40};
        fs.Write(data, 0, 4);
        fs.Close();
    }
    {
        FileStream fs(p, FileMode::Open, FileAccess::Read);
        EXPECT_EQ(fs.getLengthProperty(), 4);
        uint8_t buf[4] = {};
        fs.Read(buf, 0, 4);
        EXPECT_EQ(buf[0], 10u);
        EXPECT_EQ(buf[3], 40u);
        fs.Close();
    }
    File::Delete(p);
}

TEST(FileStreamTests, CreateMode_CanWrite) {
    std::string p = tf("fstream_write.bin");
    FileStream fs(p, FileMode::Create, FileAccess::Write);
    EXPECT_TRUE(fs.getCanWriteProperty());
    fs.Close();
    File::Delete(p);
}

TEST(FileStreamTests, OpenMode_CanRead) {
    std::string p = tf("fstream_read.bin");
    File::WriteAllText(p, "abc");
    FileStream fs(p, FileMode::Open, FileAccess::Read);
    EXPECT_TRUE(fs.getCanReadProperty());
    fs.Close();
    File::Delete(p);
}

TEST(FileStreamTests, IsOpen_AfterConstruct_True) {
    std::string p = tf("fstream_open.bin");
    FileStream fs(p, FileMode::Create);
    EXPECT_TRUE(fs.IsOpen());
    fs.Close();
    File::Delete(p);
}

TEST(FileStreamTests, getLengthProperty_AfterWrite) {
    std::string p = tf("fstream_len.bin");
    {
        FileStream fs(p, FileMode::Create, FileAccess::Write);
        uint8_t data[] = {1, 2, 3, 4, 5};
        fs.Write(data, 0, 5);
        fs.Close();
    }
    FileStream fs(p, FileMode::Open);
    EXPECT_EQ(fs.getLengthProperty(), 5);
    fs.Close();
    File::Delete(p);
}

TEST(FileStreamTests, WriteByte_Flush_NoThrow) {
    std::string p = tf("fstream_wb.bin");
    FileStream fs(p, FileMode::Create, FileAccess::Write);
    EXPECT_NO_THROW(fs.WriteByte(0xFF));
    EXPECT_NO_THROW(fs.Flush());
    fs.Close();
    File::Delete(p);
}

// ===========================================================================
// IsolatedStorageFile
// ===========================================================================

TEST(IsolatedStorageFileTests, GetUserStoreForApplication_NoThrow) {
    auto store = IsolatedStorageFile::GetUserStoreForApplication();
    EXPECT_FALSE(store.getRootDirectoryProperty().string().empty());
}

TEST(IsolatedStorageFileTests, FileExists_False_ForNonExistent) {
    auto store = IsolatedStorageFile::GetUserStoreForApplication();
    EXPECT_FALSE(store.FileExists("sharp_rt_nonexistent_xyzxyz_12345.dat"));
}

TEST(IsolatedStorageFileTests, OpenFile_WriteAndDelete_Roundtrip) {
    auto store = IsolatedStorageFile::GetUserStoreForApplication();
    const std::string fname = "sharp_rt_iso_test_file.dat";
    {
        auto stream = store.OpenFile(fname, FileMode::Create);
        uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
        stream.Write(data, 0, 4);
        stream.Close();
    }
    EXPECT_TRUE(store.FileExists(fname));
    store.DeleteFile(fname);
    EXPECT_FALSE(store.FileExists(fname));
}
