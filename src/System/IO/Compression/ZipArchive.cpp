// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/Compression/ZipArchive.hpp"
#include "System/IO/MemoryStream.hpp"
#include <miniz/miniz.h>
#include <stdexcept>
#include <cstring>
#include <algorithm>

namespace System::IO::Compression {

// ---------------------------------------------------------------------------
// Internal write-back stream (create/update mode)
// ---------------------------------------------------------------------------

class ZipEntryWriteStream final : public System::IO::Stream {
    std::shared_ptr<std::vector<SharpRuntime::bytecs>> buf_;
public:
    explicit ZipEntryWriteStream(std::shared_ptr<std::vector<SharpRuntime::bytecs>> b)
        : buf_(std::move(b)) {}

    SharpRuntime::intcs Read(SharpRuntime::bytecs*, SharpRuntime::intcs, SharpRuntime::intcs) override { return 0; }
    void Write(const SharpRuntime::bytecs* data, SharpRuntime::intcs offset, SharpRuntime::intcs count) override {
        buf_->insert(buf_->end(), data + offset, data + offset + count);
    }
    [[nodiscard]] SharpRuntime::intcs getLengthProperty() const override {
        return static_cast<SharpRuntime::intcs>(buf_->size());
    }
    [[nodiscard]] bool getCanReadProperty()  const override { return false; }
    [[nodiscard]] bool getCanWriteProperty() const override { return true;  }
    void Flush() override {}
    void Close() override {}
};

// ---------------------------------------------------------------------------
// Opaque state structs
// ---------------------------------------------------------------------------

struct PendingEntry {
    std::string                                         name;
    std::shared_ptr<std::vector<SharpRuntime::bytecs>> data;
};

struct ZipArchiveState {
    mz_zip_archive          zip{};
    bool                    readerOpen   = false;
    ZipArchiveMode          mode         = ZipArchiveMode::Read;
    std::string             filePath;
    std::vector<SharpRuntime::bytecs> memBuf;      // backing buffer for stream-based read
    std::vector<PendingEntry>         pending;     // entries queued for write
    bool                    disposed     = false;
};

struct ZipArchiveEntryState {
    std::shared_ptr<ZipArchiveState>                   archive;
    mz_uint                                            index    = 0;
    std::string                                        fullName;
    std::string                                        name;
    long long                                          length   = 0;
    // Create/update mode:
    bool                                               isWrite  = false;
    std::shared_ptr<std::vector<SharpRuntime::bytecs>> writeBuf;
};

// ---------------------------------------------------------------------------
// ZipArchiveEntry
// ---------------------------------------------------------------------------

ZipArchiveEntry::ZipArchiveEntry(std::shared_ptr<ZipArchiveEntryState> s)
    : state_(std::move(s)) {}

bool ZipArchiveEntry::IsValid() const { return state_ != nullptr; }

std::string ZipArchiveEntry::getNameProperty() const {
    return state_ ? state_->name : std::string{};
}
std::string ZipArchiveEntry::getFullNameProperty() const {
    return state_ ? state_->fullName : std::string{};
}
long long ZipArchiveEntry::getLengthProperty() const {
    return state_ ? state_->length : 0LL;
}

System::IO::Stream* ZipArchiveEntry::Open() {
    if (!state_) throw std::runtime_error("ZipArchiveEntry::Open: invalid entry");

    if (state_->isWrite) {
        // Write-mode: return a stream that writes into our pending buffer
        return new ZipEntryWriteStream(state_->writeBuf);
    }

    // Read-mode: extract via miniz
    auto& arc = *state_->archive;
    if (!arc.readerOpen)
        throw std::runtime_error("ZipArchiveEntry::Open: archive not open for reading");

    size_t outSize = 0;
    void* raw = mz_zip_reader_extract_to_heap(&arc.zip, state_->index, &outSize, 0);
    if (!raw)
        throw std::runtime_error("ZipArchiveEntry::Open: extract failed for " + state_->fullName);

    auto* ms = new System::IO::MemoryStream(
        reinterpret_cast<SharpRuntime::bytecs*>(raw),
        static_cast<SharpRuntime::intcs>(outSize));
    mz_free(raw);
    return ms;
}

void ZipArchiveEntry::Delete() {
    if (!state_) return;
    if (state_->archive->mode != ZipArchiveMode::Update)
        throw std::runtime_error("ZipArchiveEntry::Delete: archive must be in Update mode");
    // Mark as deleted — simplified: just clear the entry state
    state_ = nullptr;
}

// ---------------------------------------------------------------------------
// ZipArchive helpers
// ---------------------------------------------------------------------------

static std::string basename(const std::string& fullName) {
    auto pos = fullName.rfind('/');
    if (pos == std::string::npos) pos = fullName.rfind('\\');
    return (pos == std::string::npos) ? fullName : fullName.substr(pos + 1);
}

static void openReader(ZipArchiveState& st) {
    mz_bool ok;
    if (!st.filePath.empty()) {
        ok = mz_zip_reader_init_file(&st.zip, st.filePath.c_str(), 0);
    } else {
        ok = mz_zip_reader_init_mem(&st.zip, st.memBuf.data(), st.memBuf.size(), 0);
    }
    if (!ok)
        throw std::runtime_error("ZipArchive: failed to open zip for reading: " +
                                 (st.filePath.empty() ? "(memory)" : st.filePath));
    st.readerOpen = true;
}

static void flushWriter(ZipArchiveState& st) {
    if (st.pending.empty()) return;

    if (!st.filePath.empty()) {
        mz_zip_archive writer{};
        if (!mz_zip_writer_init_file(&writer, st.filePath.c_str(), 0))
            throw std::runtime_error("ZipArchive: failed to init writer for " + st.filePath);
        for (auto& e : st.pending) {
            mz_zip_writer_add_mem(&writer, e.name.c_str(),
                                  e.data->data(), e.data->size(),
                                  MZ_DEFAULT_COMPRESSION);
        }
        mz_zip_writer_finalize_archive(&writer);
        mz_zip_writer_end(&writer);
    } else {
        // Memory-based write — store result back in memBuf
        mz_zip_archive writer{};
        if (!mz_zip_writer_init_heap(&writer, 0, 65536))
            throw std::runtime_error("ZipArchive: failed to init heap writer");
        for (auto& e : st.pending) {
            mz_zip_writer_add_mem(&writer, e.name.c_str(),
                                  e.data->data(), e.data->size(),
                                  MZ_DEFAULT_COMPRESSION);
        }
        void* buf = nullptr; size_t sz = 0;
        mz_zip_writer_finalize_heap_archive(&writer, &buf, &sz);
        mz_zip_writer_end(&writer);
        st.memBuf.assign(reinterpret_cast<SharpRuntime::bytecs*>(buf),
                         reinterpret_cast<SharpRuntime::bytecs*>(buf) + sz);
        mz_free(buf);
    }
    st.pending.clear();
}

// ---------------------------------------------------------------------------
// ZipArchive constructors / destructor
// ---------------------------------------------------------------------------

ZipArchive::ZipArchive(System::IO::Stream* stream, ZipArchiveMode mode)
    : state_(std::make_shared<ZipArchiveState>())
{
    state_->mode = mode;
    if (mode == ZipArchiveMode::Read || mode == ZipArchiveMode::Update) {
        // Read full stream into memory buffer
        SharpRuntime::bytecs tmp[65536];
        SharpRuntime::intcs n;
        while ((n = stream->Read(tmp, 0, 65536)) > 0)
            state_->memBuf.insert(state_->memBuf.end(), tmp, tmp + n);
        if (mode == ZipArchiveMode::Read)
            openReader(*state_);
    }
    // Create mode with stream: we'll write to memBuf on Dispose
}

ZipArchive::ZipArchive(const std::string& archivePath, ZipArchiveMode mode)
    : state_(std::make_shared<ZipArchiveState>())
{
    state_->mode     = mode;
    state_->filePath = archivePath;
    if (mode == ZipArchiveMode::Read || mode == ZipArchiveMode::Update)
        openReader(*state_);
}

ZipArchive::~ZipArchive() { Dispose(); }

// ---------------------------------------------------------------------------
// ZipArchive operations
// ---------------------------------------------------------------------------

std::vector<ZipArchiveEntry> ZipArchive::getEntriesProperty() const {
    if (!state_ || !state_->readerOpen) return {};
    mz_uint count = mz_zip_reader_get_num_files(&state_->zip);
    std::vector<ZipArchiveEntry> result;
    result.reserve(count);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&state_->zip, i, &stat)) continue;
        if (stat.m_is_directory) continue;
        auto es       = std::make_shared<ZipArchiveEntryState>();
        es->archive   = state_;
        es->index     = i;
        es->fullName  = stat.m_filename;
        es->name      = basename(stat.m_filename);
        es->length    = static_cast<long long>(stat.m_uncomp_size);
        result.emplace_back(std::move(es));
    }
    return result;
}

ZipArchiveEntry ZipArchive::GetEntry(const std::string& entryName) const {
    if (!state_ || !state_->readerOpen) return ZipArchiveEntry{};
    mz_uint count = mz_zip_reader_get_num_files(&state_->zip);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&state_->zip, i, &stat)) continue;
        if (entryName == stat.m_filename) {
            auto es      = std::make_shared<ZipArchiveEntryState>();
            es->archive  = state_;
            es->index    = i;
            es->fullName = stat.m_filename;
            es->name     = basename(stat.m_filename);
            es->length   = static_cast<long long>(stat.m_uncomp_size);
            return ZipArchiveEntry(std::move(es));
        }
    }
    return ZipArchiveEntry{};
}

ZipArchiveEntry ZipArchive::CreateEntry(const std::string& entryName) {
    if (!state_)
        throw std::runtime_error("ZipArchive::CreateEntry: archive not open");
    if (state_->mode == ZipArchiveMode::Read)
        throw std::runtime_error("ZipArchive::CreateEntry: archive is read-only");

    auto buf = std::make_shared<std::vector<SharpRuntime::bytecs>>();
    state_->pending.push_back({entryName, buf});

    auto es       = std::make_shared<ZipArchiveEntryState>();
    es->archive   = state_;
    es->fullName  = entryName;
    es->name      = basename(entryName);
    es->isWrite   = true;
    es->writeBuf  = buf;
    return ZipArchiveEntry(std::move(es));
}

void ZipArchive::Dispose() {
    if (!state_ || state_->disposed) return;
    state_->disposed = true;

    if (state_->mode == ZipArchiveMode::Create || state_->mode == ZipArchiveMode::Update)
        flushWriter(*state_);

    if (state_->readerOpen) {
        mz_zip_reader_end(&state_->zip);
        state_->readerOpen = false;
    }
}

} // namespace System::IO::Compression
