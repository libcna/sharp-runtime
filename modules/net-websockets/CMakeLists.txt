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
)
