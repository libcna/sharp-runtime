# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Net.WebSockets
    TARGET sharp_runtime_net_websockets
    TYPE STATIC
    # Net became PUBLIC with ticket #2089: ClientWebSocketOptions.hpp, a public header, now
    # validates its handshake header doors with the shared
    # System::Net::detail::ContainsProtocolFieldTerminator predicate. The edge itself already
    # existed (privately, for ClientWebSocket.cpp) and Net already reaches consumers
    # transitively through Net.Sockets, so only the declared visibility changed -- no new
    # component edge and no new closure for a Net.WebSockets consumer.
    PUBLIC_DEPENDENCIES
        ComponentModel
        Core.Base
        Net
        Net.Sockets
        Threading
        Threading.Tasks
        Uri
    # Security.Cryptography.Random became a PRIVATE dependency with ticket #2401: the per-frame
    # masking key must come from a CSPRNG (RFC 6455 section 5.3 -- "MUST be derived from a strong
    # source of entropy"), and .NET spells that RandomNumberGenerator.Fill
    # (ManagedWebSocket.cs:762-763). It is PRIVATE because the only caller is a file-local helper
    # in ClientWebSocket.cpp, so no public header names the type and no consumer inherits the
    # edge. The Sec-WebSocket-Key nonce needed no edge at all: .NET builds it from Guid.NewGuid()
    # (WebSocketHandle.Managed.cs:490-494), and Core.Base is already public here.
    PRIVATE_DEPENDENCIES
        Security.Cryptography.Random
)
