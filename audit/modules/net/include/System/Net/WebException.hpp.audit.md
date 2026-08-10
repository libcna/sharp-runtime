# Audit: `modules/net/include/System/Net/WebException.hpp`

Audit status: AUDITED.

The implemented constructors and status value match the retained legacy
surface.  The intentionally omitted WebResponse member is documented.  No
separate finding was confirmed.

Correction and post-audit closure (ticket #1932, 2026-08-01): the represented
constructor surface had a separable HResult propagation gap despite retaining
the right legacy overloads and status values. Under approved Option 2R, both
WebException constructors accepting `std::exception_ptr` now copy the exact
HResult of a contained System::Exception, including zero. Null and non-System
pointers retain `0x80131509`, and WebExceptionStatus never overrides the copied
value. The full direct/copy/move/assignment/exception-pointer matrix is now
permanent. No WebException producer exists in the represented port, and none
was invented. This was inactive post-audit work; no new finding was issued and
the audit total remains 68 remediated / 296 open / 364.
