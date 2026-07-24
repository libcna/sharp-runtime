# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

function(_sharp_runtime_setup_cryptography_random target)
    if(WIN32)
        target_link_libraries("${target}" PRIVATE bcrypt)
    endif()
endfunction()

sharp_runtime_register_component(
    NAME Security.Cryptography
    TARGET sharp_runtime_security_cryptography
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_CRYPTOGRAPHY_SOURCES}
    DEPENDENCIES Core
)

sharp_runtime_register_component(
    NAME Security.Cryptography.Random
    TARGET sharp_runtime_security_cryptography_random
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_CRYPTOGRAPHY_RANDOM_SOURCES}
    DEPENDENCIES Core
    SETUP _sharp_runtime_setup_cryptography_random
)
