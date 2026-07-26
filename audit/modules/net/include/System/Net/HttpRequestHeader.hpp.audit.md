# Audit: `modules/net/include/System/Net/HttpRequestHeader.hpp`

Audit status: AUDITED.

The request-header enumerators and guarded name lookup cover the audited
surface.  Invalid enum casts produce a deterministic index exception.  No
separate finding was confirmed.
