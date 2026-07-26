# Audit: `modules/net/include/System/Net/Sockets/SocketException.hpp`

Audit status: AUDITED.

SocketException delegates POSIX errno translation through the shared helper;
constructor and property paths are coherent.  No separate finding was
confirmed.
