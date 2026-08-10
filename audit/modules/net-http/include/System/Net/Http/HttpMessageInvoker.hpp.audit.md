# Audit: `modules/net-http/include/System/Net/Http/HttpMessageInvoker.hpp`

Audit status: AUDITED.

The invoker validates null handler/request, respects its disposal policy, and
forwards an already-cancelled token before handler execution.  Its async route
does not retain a handler across task execution; see the shared raw-lifetime
analysis in `HttpMessageHandler.hpp.audit.md` and SR-AUD-310.

Missing coverage: disposal immediately after `SendAsync`, concurrent Dispose/
SendAsync, and handler destruction with a retained returned task.
