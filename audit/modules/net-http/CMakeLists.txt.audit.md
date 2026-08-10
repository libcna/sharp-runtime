# Audit: `modules/net-http/CMakeLists.txt`

Audit status: AUDITED.

The static module declares the implementation sources through the shared module
registration mechanism and exposes the required Core.Base, IO, Net, Threading,
and Threading.Tasks dependencies.  The Uri parser is correctly private.  The
focused target builds successfully with `gmake -C build -j4
SharpRuntimeTests_Net_Http`.

No independent build-graph finding was confirmed.
