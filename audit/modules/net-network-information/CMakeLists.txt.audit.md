# Audit: `modules/net-network-information/CMakeLists.txt`

## Metadata

- AUDITED: static-module registration and declared public dependencies.
- Validation: `SharpRuntimeTests_Net_NetworkInformation` rebuilt successfully.

## Assessment

The target correctly exposes the ComponentModel, Core.Base, Net, and
Threading.Tasks dependencies used by its public headers and implementation.

## Other missing assertions and diagnostics

- CI needs a network-permitted run of this target: 11 of 39 runtime tests need
  `getifaddrs` or ICMP and are denied by the current sandbox.

## Final assessment

No build-graph discrepancy was demonstrated. No source or test changed.
