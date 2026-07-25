<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Sharp Runtime modules

Every directory here owns one public CMake component and its `include/`,
`src/`, and `tests/` trees. Header-only modules retain an empty, versioned
`src/` directory so the layout stays uniform. Tests that intentionally span
multiple components live in the repository-level `tests/integration/`.

Module `CMakeLists.txt` files register sources, headers, tests, direct
dependencies, and platform setup with the root component resolver. They are
not standalone projects: configure the repository root and select components
with `SHARP_RUNTIME_COMPONENTS`.

Project-wide planning and contributor rules remain centralized in the root
`plan.md`, `NEXT.md`, and `CLAUDE.md`. A module keeps only its local `README.md`
unless it later needs genuinely module-specific guidance.

See [CMake components](../docs/CMakeComponents.md) for the complete dependency
map and consumer examples.
