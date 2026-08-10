# Audit: `modules/net-network-information/include/System/Net/NetworkInformation/PhysicalAddress.hpp`

## Metadata

- AUDITED: MAC-address value type, parsing, formatting, and hash API.
- Validation: ten focused fixture cases and a high-bit UBSan hash probe passed.

## Assessment

The C++ representation is a faithful vector adaptation of current .NET's
byte-array value: copied bytes, uppercase hex output, and supported bare,
hyphen, colon, and Cisco-dot grammar agree with the reference algorithm.

## Other missing assertions and diagnostics

- Add empty, long hyphen-delimited, mixed-delimiter, partial-tail, hash-zero,
  and copy-isolation cases.

## Final assessment

No PhysicalAddress discrepancy was demonstrated. No source or test changed.
