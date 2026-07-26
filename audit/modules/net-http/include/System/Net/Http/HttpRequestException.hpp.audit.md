# Audit: `modules/net-http/include/System/Net/Http/HttpRequestException.hpp`

Audit status: AUDITED.

The wrapper retains the optional status code and explicit request-error value.
It follows the project-wide `std::exception_ptr` adaptation and documents the
missing HResult propagation.  No independent construction defect was
confirmed.

Missing coverage: all overload combinations, inner exception rethrow behavior,
and status codes at the managed constructor bounds.
