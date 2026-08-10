# Audit: `modules/net-http/include/System/Net/Http/HttpIOException.hpp`

Audit status: AUDITED.

The error category is retained and appended to a usable message.  The fallback
text is an intentional local adaptation of .NET's generic exception message;
no independent defect was confirmed.

Missing coverage: every `HttpRequestError` spelling, inner-exception retention,
and polymorphic catching through `IOException` and `Exception`.
