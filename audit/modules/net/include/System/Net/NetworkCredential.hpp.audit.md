# Audit: `modules/net/include/System/Net/NetworkCredential.hpp`

Audit status: AUDITED.

`GetCredential` depends on `shared_from_this`; the header documents the
resulting ownership requirement.  No independent compatibility finding was
confirmed beyond that explicit C++ adaptation.

Missing coverage: stack-owned misuse should have a diagnostic regression.
