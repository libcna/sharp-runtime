# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

# Ticket #2124 (SR-AUD-319, cause NH-H) declared the PRIVATE edge on Net. The family's single
# protocol-field-terminator predicate, System::Net::detail::ContainsProtocolFieldTerminator, lives
# in modules/net/include/System/Net/detail/ProtocolFieldValidation.hpp and already serves ten
# System::Net::Http doors (#2063) and three System::Net::WebSockets doors (#2089). Before #2124
# this module carried FOUR independently written copies of that same rule, and they disagreed --
# which is cause NH-H itself. The edge is PRIVATE because the predicate is called from .cpp bodies
# only: no public header here includes anything from Net, so no consumer's include surface grows.
sharp_runtime_register_module(
    NAME Net.Http.Headers
    TARGET sharp_runtime_net_http_headers
    TYPE STATIC
    PUBLIC_DEPENDENCIES Collections.Core Core.Base Uri
    PRIVATE_DEPENDENCIES Net
)
