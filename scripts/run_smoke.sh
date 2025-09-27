#!/usr/bin/env bash
#
# Sequence: SEQ0079
# Track: Shared
# MVP: Step C
# Change: Provide convenience wrapper to build the project and execute the smoke suite.
# Tests: smoke_bind_conflict, smoke_dual_listener, smoke_shutdown_signal, smoke_max_clients,
#        smoke_irc_boot, smoke_persistence_toggle

set -euo pipefail

BUILD_DIR="${1:-build}"
shift || true

cmake -S "$(dirname "$0")/.." -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}"
ctest --test-dir "${BUILD_DIR}" -L smoke "$@"
