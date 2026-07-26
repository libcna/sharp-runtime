# Audit: `modules/net-network-information/src/System/Net/NetworkInformation/NetworkInterface.cpp`

## Metadata

- AUDITED: Linux `getifaddrs` PAL, sysfs speed lookup, loopback index, and
  non-Linux exceptions.
- Validation: loopback-index test passed; six enumerator tests fail at
  `getifaddrs` with sandbox `EPERM`/native code 1.

## Assessment

The collector frees `ifaddrs`, aggregates per-name addresses, obtains MACs from
AF_PACKET, and deliberately excludes both Loopback and Tunnel from availability
just as the current Linux managed PAL does. The observed failures are local
permission limits, not a demonstrated implementation regression.

## Other missing assertions and diagnostics

- Surface errno in a diagnostic before wrapping enumeration failures and run
  interface-matrix tests where `getifaddrs` is allowed.

## Final assessment

No source discrepancy was demonstrated. No source or test changed.
