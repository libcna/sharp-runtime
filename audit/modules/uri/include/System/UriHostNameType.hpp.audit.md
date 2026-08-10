# Audit: `modules/uri/include/System/UriHostNameType.hpp`

## Metadata

- AUDITED: 21-line enum declaration, fully read.
- Validation: `UriHostNameTypeTest.*` passed 6/6 within the selected 38-test
  URI value-type filter on 2026-07-27.

## SR-AUD-151 — medium — UriHostNameType has no Uri.CheckHostName classifier or public producer

The header documents the enum for `System::Uri::CheckHostName`, yet audited
`Uri.hpp`/`Uri.cpp` export no static classifier. Current .NET exposes
`Uri.CheckHostName(string?)` returning these five values. C++ can name a
constant but cannot classify a DNS, IPv4, IPv6, basic, or invalid host through
the advertised API.

## Other missing assertions and diagnostics

- Tests check only values, not a classifier's normal, malformed, IDN, or null
  host inputs.

## Final assessment

The enum values are correct but its documented producer API is absent. No
source or test was modified during this audit.
