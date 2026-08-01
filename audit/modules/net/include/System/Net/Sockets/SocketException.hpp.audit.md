# Audit: `modules/net/include/System/Net/Sockets/SocketException.hpp`

Audit status: AUDITED.

SocketException delegates POSIX errno translation through the shared helper;
constructor and property paths are coherent.  No separate finding was
confirmed.

## Post-audit remediation — ticket #1875 (2026-08-01)

The shared Win32 root now supplies current .NET's inherited `E_FAIL`
(`0x80004005`) instead of `COR_E_EXCEPTION`, while native and Socket error
properties remain unchanged. The permanent population matrix pins the derived
result; no declaration or platform translation changed.
