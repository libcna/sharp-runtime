# Audit: `modules/net-http/include/System/Net/Http/HttpRequestException.hpp`

Audit status: AUDITED.

The wrapper retains the optional status code and explicit request-error value.
It follows the project-wide `std::exception_ptr` adaptation and documents the
missing HResult propagation.  No independent construction defect was
confirmed.

Missing coverage: all overload combinations, inner exception rethrow behavior,
and status codes at the managed constructor bounds.

Correction and post-audit closure (ticket #1932, 2026-08-01): the original
“project-wide adaptation” characterization was too broad. Current .NET defines
a constructor-specific conditional rule shared by HttpRequestException and
WebException. Under approved Option 2R, all three HttpRequestException
constructors accepting `std::exception_ptr` now copy the exact HResult of a
contained System::Exception, including zero. Null and non-System pointers
retain `0x80131500`; HttpRequestError and HttpStatusCode never override it.
Permanent tests now cover every overload, inner identity and rethrow, exact
metadata, copy/move/assignment, and sync/async forwarding. This was inactive
post-audit ticket work, not a new audit finding; numbering and totals remain
68 remediated / 296 open / 364.
