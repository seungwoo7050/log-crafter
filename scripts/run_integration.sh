#!/usr/bin/env bash
#
# Sequence: SEQ0100
# Track: Shared
# MVP: Step C
# Change: Provide a helper wrapper to execute the integration suite via ctest.
# Tests: integration_multi_client_broadcast, integration_cpp_irc_feature, integration_connection_determinism
#

set -euo pipefail

if [[ $# -gt 0 ]]; then
  echo "Usage: $0" >&2
  exit 1
fi

ctest --test-dir "${LOGCRAFTER_BUILD_DIR:-build}" -L integration --output-on-failure
