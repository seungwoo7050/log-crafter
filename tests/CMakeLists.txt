cmake_minimum_required(VERSION 3.20)

#
# Sequence: SEQ0079
# Track: Shared
# MVP: Step C
# Change: Register smoke tests with ctest using Python harnesses for both tracks.
# Tests: smoke_bind_conflict, smoke_dual_listener, smoke_shutdown_signal, smoke_max_clients,
#        smoke_irc_boot, smoke_persistence_toggle
#
#
# Sequence: SEQ0091
# Track: Shared
# MVP: Step C
# Change: Add Step C spec harness wiring for protocol, error, partial I/O, timeout, and SIGINT cases.
# Tests: spec_protocol_happy_path, spec_invalid_inputs, spec_partial_io, spec_timeouts, spec_sigint_shutdown
#
#
# Sequence: SEQ0098
# Track: Shared
# MVP: Step C
# Change: Register the Step C integration suite for broadcast, IRC, and lifecycle scenarios.
# Tests: integration_multi_client_broadcast, integration_cpp_irc_feature, integration_connection_determinism
#

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(LOGCRAFTER_SOURCE_ROOT "${CMAKE_SOURCE_DIR}")
set(LOGCRAFTER_SMOKE_RUNNER "${CMAKE_SOURCE_DIR}/tests/smoke/test_smoke.py")
set(LOGCRAFTER_SPEC_RUNNER "${CMAKE_SOURCE_DIR}/tests/spec/test_spec.py")
set(LOGCRAFTER_INTEGRATION_RUNNER "${CMAKE_SOURCE_DIR}/tests/integration/test_integration.py")

function(logcrafter_add_smoke name)
    add_test(
        NAME ${name}
        COMMAND ${Python3_EXECUTABLE} "${LOGCRAFTER_SMOKE_RUNNER}" ${name}
    )
    set_tests_properties(${name} PROPERTIES
        LABELS "smoke"
        RUN_SERIAL TRUE
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        ENVIRONMENT "LOGCRAFTER_BUILD_DIR=${CMAKE_BINARY_DIR};PYTHONPATH=${LOGCRAFTER_SOURCE_ROOT}"
    )
endfunction()

logcrafter_add_smoke(smoke_bind_conflict)
logcrafter_add_smoke(smoke_dual_listener)
logcrafter_add_smoke(smoke_shutdown_signal)
logcrafter_add_smoke(smoke_max_clients)
logcrafter_add_smoke(smoke_irc_boot)
logcrafter_add_smoke(smoke_persistence_toggle)

function(logcrafter_add_spec name)
    add_test(
        NAME ${name}
        COMMAND ${Python3_EXECUTABLE} "${LOGCRAFTER_SPEC_RUNNER}" ${name}
    )
    set_tests_properties(${name} PROPERTIES
        LABELS "spec"
        RUN_SERIAL TRUE
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        ENVIRONMENT "LOGCRAFTER_BUILD_DIR=${CMAKE_BINARY_DIR};PYTHONPATH=${LOGCRAFTER_SOURCE_ROOT}"
    )
endfunction()

logcrafter_add_spec(spec_protocol_happy_path)
logcrafter_add_spec(spec_invalid_inputs)
logcrafter_add_spec(spec_partial_io)
logcrafter_add_spec(spec_timeouts)
logcrafter_add_spec(spec_sigint_shutdown)

function(logcrafter_add_integration name)
    add_test(
        NAME ${name}
        COMMAND ${Python3_EXECUTABLE} "${LOGCRAFTER_INTEGRATION_RUNNER}" ${name}
    )
    set_tests_properties(${name} PROPERTIES
        LABELS "integration"
        RUN_SERIAL TRUE
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        ENVIRONMENT "LOGCRAFTER_BUILD_DIR=${CMAKE_BINARY_DIR};PYTHONPATH=${LOGCRAFTER_SOURCE_ROOT}"
    )
endfunction()

logcrafter_add_integration(integration_multi_client_broadcast)
logcrafter_add_integration(integration_cpp_irc_feature)
logcrafter_add_integration(integration_connection_determinism)
