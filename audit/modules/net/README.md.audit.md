# Audit: `modules/net/README.md`

Audit status: AUDITED.

The module overview accurately identifies its partial network surface.  Its
DNS note is stale (the implementation now has IPv6 support), and its documented
CookieContainer simplifications are recorded in SR-AUD-308 rather than hidden
as full compatibility.
