# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors
#
# =====================================================================================
# Sharp Runtime release identity -> the generated public header SharpRuntime/Version.hpp.
#
# The single source of truth is project(SHARP_RUNTIME VERSION ...) plus
# SHARP_RUNTIME_VERSION_PRERELEASE in the root CMakeLists.txt; this file only renders that
# decision into a header. Nothing else in the tree may hard-code the number -- docs/releasing.md
# lists the few hand-maintained copies (CHANGELOG.md, Doxyfile's PROJECT_NUMBER) that a release
# bump must update by hand.
#
# SHARP_RUNTIME_BINARY_DIR/SHARP_RUNTIME_SOURCE_DIR, not CMAKE_BINARY_DIR/CMAKE_SOURCE_DIR:
# this project is consumed with add_subdirectory (CNA does exactly that from ../sharp-runtime),
# and there CMAKE_BINARY_DIR is the *consumer's* build root, where the generated header does not
# belong.
# =====================================================================================

include_guard(GLOBAL)

set(SHARP_RUNTIME_GENERATED_INCLUDE_DIR "${SHARP_RUNTIME_BINARY_DIR}/generated/include")

configure_file(
    "${SHARP_RUNTIME_SOURCE_DIR}/cmake/templates/Version.hpp.in"
    "${SHARP_RUNTIME_GENERATED_INCLUDE_DIR}/SharpRuntime/Version.hpp"
    @ONLY)
