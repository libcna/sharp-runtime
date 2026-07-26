# Audit: `modules/net/include/System/Net/Dns.hpp`

Audit status: AUDITED.

The public synchronous DNS surface is intentionally partial.  The header's
old IPv4-only note conflicts with the current IPv6 implementation and should
be corrected with the remediation for SR-AUD-304.  No separate header finding
was recorded.
