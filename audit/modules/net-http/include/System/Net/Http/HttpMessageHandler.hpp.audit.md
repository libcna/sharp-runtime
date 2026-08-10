# Audit: `modules/net-http/include/System/Net/Http/HttpMessageHandler.hpp`

Audit status: AUDITED.

The synchronous base validates a cancellation token before forwarding.  Its
default `SendAsync` task captures raw `this`; callers with a handler whose
lifetime ends while the task runs can reproduce the same lifetime class as
SR-AUD-310.  The direct ASan proof is on the public `HttpClient` wrapper, which
uses the identical task pattern.

Missing coverage: handler destruction/disposal during `SendAsync`, cancellation
after start, and async exception propagation.
