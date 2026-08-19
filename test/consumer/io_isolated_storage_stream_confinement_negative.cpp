// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Negative compile fixture for ticket #2208: pins the STRUCTURE that makes
// IsolatedStorageFileStream's confinement inescapable.
//
// The constructor used to take a std::filesystem::path and check nothing: it opened whatever it
// was handed, anywhere on the filesystem, and created that path's missing parents on the way --
// a wider hole than #2207's declared TOCTOU, since it needed neither a race nor a privilege,
// only the call. Both public constructors now resolve through the owning store's private
// fullPath(), which is the same resolver every other door on IsolatedStorageFile uses.
//
// What is NOT pinned here is a removed overload, because #2208's premise was wrong about that:
// .NET publishes `(string path, FileMode mode)` and its store parameter is optional and
// TRAILING (IsolatedStorageFileStream.cs:21-56), so both spellings survive. What a consumer
// must not be able to do is step AROUND the resolver -- resolve a path itself, reach the
// resolving constructor directly, or find a store-first overload that never existed in .NET.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by the compiler;
// with no site selected this file must compile with zero diagnostics.
// scripts/check_negative_consumer_fixtures.py compiles the baseline and each site separately.
//
// NEGATIVE-FIXTURE: component=IO.IsolatedStorage

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

#include <filesystem>
#include <string>

#include "System/IO/FileMode.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageFile.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageFileStream.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageScope.hpp"

using System::IO::FileMode;
using System::IO::IsolatedStorage::IsolatedStorageFile;
using System::IO::IsolatedStorage::IsolatedStorageFileStream;

namespace {

    IsolatedStorageFile& store()
    {
        static IsolatedStorageFile s = IsolatedStorageFile::GetUserStoreForApplication();
        return s;
    }

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // The escape route a consumer reaches for once the constructor is confined: resolve the path
    // yourself and hand over the result. fullPath() is private and IsolatedStorageFileStream is
    // its only friend, so the friendship #2208 relies on grants a consumer NOTHING -- the half of
    // the repair no behavioural test can express.
    // NEGATIVE(store-fullpath-is-not-consumer-reachable): is private within this context
    //     | no member named 'fullPath'
    void site1()
    {
        const auto resolved = store().fullPath("x.dat", "relativePath");
        (void)resolved;
    }
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // The four-argument resolving constructor is private. It exists so each door can report its
    // OWN path parameter's name, and a consumer reaching it could name a parameter that is not
    // theirs -- and, more to the point, could reach the one constructor that takes its paramName
    // from the caller rather than from the door.
    // NEGATIVE(resolving-ctor-is-private): is private within this context
    //     | no matching function for call to
    void site2()
    {
        IsolatedStorageFileStream stream("x.dat", FileMode::Create, store(), "path");
        stream.Close();
    }
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // Resolve() is the confinement itself. A consumer that could call it could pre-resolve a path
    // and then... still have nowhere to put it -- but a public Resolve would advertise that the
    // check is separable from the construction, which is exactly what #2208 made untrue.
    // NEGATIVE(resolve-is-private): is private within this context
    //     | no member named 'Resolve'
    void site3()
    {
        const auto resolved =
            IsolatedStorageFileStream::Resolve("x.dat", FileMode::Create, store(), "path");
        (void)resolved;
    }
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // The store-FIRST parameter order. #2208 was implemented that way before the reference was
    // read, and .NET has no such overload -- its store is always trailing. This site keeps the
    // corrected order from drifting back, because the wrong order compiles perfectly well as an
    // addition and would leave the type with two spellings where .NET has one.
    // NEGATIVE(store-first-overload-does-not-exist): no matching function for call to
    //     | candidate expects
    void site4()
    {
        IsolatedStorageFileStream stream(store(), "x.dat", FileMode::Create);
        stream.Close();
    }
#endif

} // namespace

int main()
{
    // Both public spellings, which are .NET's: the store is optional and trailing.
    IsolatedStorageFileStream withStore("migrated.dat", FileMode::Create, store());
    withStore.Close();

    IsolatedStorageFileStream defaulted("defaulted.dat", FileMode::Create);
    defaulted.Close();
    return 0;
}
