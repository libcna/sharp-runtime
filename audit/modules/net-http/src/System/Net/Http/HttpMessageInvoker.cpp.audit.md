# Audit: `modules/net-http/src/System/Net/Http/HttpMessageInvoker.cpp`

Audit status: AUDITED.

The implementation validates nulls, enforces disposal before dispatch, forwards
cooperative cancellation, and invokes owned-handler disposal exactly once.  Its
returned asynchronous task inherits the base handler raw-lifetime concern; no
separate invoker-only behavior defect was confirmed.

Missing coverage: disposal while task execution is pending and concurrent
Send/Dispose transitions.
