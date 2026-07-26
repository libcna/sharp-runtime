# Audit: `modules/net-http/include/System/Net/Http/HttpCompletionOption.hpp`

Audit status: AUDITED.

The two ordinal values match the managed enum.  No method in this module
accepts or observes this type, so `ResponseHeadersRead` is inert rather than a
selectable completion behavior.

Missing coverage: add a consumer-level compile/behavior check if an overload
is later exposed; do not imply streamed-response support from the enum alone.
