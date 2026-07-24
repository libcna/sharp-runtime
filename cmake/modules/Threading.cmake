# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_component(
    NAME Threading
    TARGET sharp_runtime_threading
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_THREADING_SOURCES}
    DEPENDENCIES Core
)

sharp_runtime_register_component(
    NAME Threading.Tasks
    TARGET sharp_runtime_threading_tasks
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_THREADING_TASKS_SOURCES}
    DEPENDENCIES Core Threading
)

sharp_runtime_register_component(
    NAME Timers
    TARGET sharp_runtime_timers
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_TIMERS_SOURCES}
    DEPENDENCIES Core Threading
)
