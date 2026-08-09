// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

// =====================================================================================
// The ONE authoritative definition of the security-cryptography key-material test seam.
// Tickets #2159 (SR-AUD-332) and #2160 (SR-AUD-331).
//
// `System/Security/Cryptography/detail/SecureMemory.hpp` DECLARES
//
//     namespace SharpRuntime::Testing { template <typename TOwner> struct KeyMaterialAccess; }
//
// and never defines it, so no production translation unit and no consumer can name a
// complete type -- proved, not asserted, by
// `test/consumer/security_cryptography_key_material_negative.cpp`.
// `KeyedHashAlgorithm` and `HMAC` each befriend it (`Rfc2898DeriveBytes` joins with #2160).
//
// WHY A SEAM AT ALL, when the rest of this batch's evidence comes from a freed-storage
// recorder: the recorder cannot tell `Dispose()` erased the pads from `~HMAC()` erased them,
// because both leave the same zeroed block at the same moment. The distinction is exactly
// what SR-AUD-332 is about -- .NET's contract is that DISPOSAL erases -- so the regression
// has to read the live object. And erasure is not observable through any public API by
// construction: a type that let a caller read its own key-derived pads would be the defect.
//
// THIS FILE IS THE ONLY PLACE IN THE REPOSITORY THAT MAY DEFINE ANY SPECIALISATION OF THAT
// TEMPLATE, for the reasons ticket #1800 measured and `docs/CollectionVersionTestSeamDesign.md`
// records: two token-different definitions of one class in one program is a one-definition-rule
// violation that is ill-formed with NO DIAGNOSTIC REQUIRED -- no linker error, no `-flto -Wodr`,
// no ASan `detect_odr_violation=2`, no UBSan.
//
// RULES FOR ANY FUTURE SUITE THAT NEEDS KEY MATERIAL:
//
//   1. Include this header. Do NOT write `template<> struct KeyMaterialAccess<...>` in a test
//      translation unit -- `scripts/check_version_seam_odr.py` fails the build if you do.
//   2. If a type is missing below, add it HERE, once, through the
//      SHARP_RUNTIME_KEY_MATERIAL_SEAM macro.
//   3. The seam grants access and defines no behaviour. It reads private buffers; it must not
//      mutate them, and it must not do anything the type's own public API could not.
// =====================================================================================

#include <cstddef>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Security/Cryptography/HMAC.hpp"
#include "System/Security/Cryptography/HMACMD5.hpp"
#include "System/Security/Cryptography/HMACSHA1.hpp"
#include "System/Security/Cryptography/HMACSHA256.hpp"
#include "System/Security/Cryptography/HMACSHA384.hpp"
#include "System/Security/Cryptography/HMACSHA512.hpp"
#include "System/Security/Cryptography/HMACSHA3_256.hpp"
#include "System/Security/Cryptography/HMACSHA3_384.hpp"
#include "System/Security/Cryptography/HMACSHA3_512.hpp"
#include "System/Security/Cryptography/KeyedHashAlgorithm.hpp"
#include "System/Security/Cryptography/detail/SecureMemory.hpp"

namespace SharpRuntime::Testing {

/**
 * Every specialisation exposes the same shape: named const references to the owner's private
 * key-material buffers. One macro so that a future type cannot acquire a different idea of what
 * "read the key material" means -- the divergence ticket #1800 found in the collections seam.
 */
#define SHARP_RUNTIME_KEY_MATERIAL_SEAM_BUFFER(name, member)                                       \
    static const std::vector<SharpRuntime::bytecs>& name(const Owner& owner) { return owner.member; }

#define SHARP_RUNTIME_KEY_MATERIAL_SEAM_KEYED(OwnerType)                                           \
    template <>                                                                                    \
    struct KeyMaterialAccess<OwnerType> {                                                          \
        using Owner = OwnerType;                                                                   \
        SHARP_RUNTIME_KEY_MATERIAL_SEAM_BUFFER(key, keyValue_)                                      \
        SHARP_RUNTIME_KEY_MATERIAL_SEAM_BUFFER(innerPad, innerPad_)                                 \
        SHARP_RUNTIME_KEY_MATERIAL_SEAM_BUFFER(outerPad, outerPad_)                                 \
        SHARP_RUNTIME_KEY_MATERIAL_SEAM_BUFFER(pendingMessage, pendingMessage_)                     \
    };

SHARP_RUNTIME_KEY_MATERIAL_SEAM_KEYED(System::Security::Cryptography::HMACMD5)
SHARP_RUNTIME_KEY_MATERIAL_SEAM_KEYED(System::Security::Cryptography::HMACSHA1)
SHARP_RUNTIME_KEY_MATERIAL_SEAM_KEYED(System::Security::Cryptography::HMACSHA256)
SHARP_RUNTIME_KEY_MATERIAL_SEAM_KEYED(System::Security::Cryptography::HMACSHA384)
SHARP_RUNTIME_KEY_MATERIAL_SEAM_KEYED(System::Security::Cryptography::HMACSHA512)
SHARP_RUNTIME_KEY_MATERIAL_SEAM_KEYED(System::Security::Cryptography::HMACSHA3_256)
SHARP_RUNTIME_KEY_MATERIAL_SEAM_KEYED(System::Security::Cryptography::HMACSHA3_384)
SHARP_RUNTIME_KEY_MATERIAL_SEAM_KEYED(System::Security::Cryptography::HMACSHA3_512)

} // namespace SharpRuntime::Testing
