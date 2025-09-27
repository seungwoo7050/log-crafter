# Tests

Sequence: SEQ0079  \
Track: Shared  \
MVP: Step C  \
Change: Seed the Step C testing layout with smoke automation assets.  \
Tests: smoke_bind_conflict, smoke_dual_listener, smoke_shutdown_signal, smoke_max_clients,
smoke_irc_boot, smoke_persistence_toggle

Sequence: SEQ0092  \
Track: Shared  \
MVP: Step C  \
Change: Document the Step C spec harness and execution flow.  \
Tests: spec_protocol_happy_path, spec_invalid_inputs, spec_partial_io, spec_timeouts,
spec_sigint_shutdown

Sequence: SEQ0099  \
Track: Shared  \
MVP: Step C  \
Change: Document the Step C integration suite layout and execution commands.  \
Tests: integration_multi_client_broadcast, integration_cpp_irc_feature, integration_connection_determinism

```
 tests/
 ├─ README.md
 ├─ CMakeLists.txt      # Registers smoke (label: smoke), spec (label: spec), and integration cases
 ├─ common/
 │   ├─ __init__.py     # Package marker for shared utilities
 │   └─ runtime.py      # Helpers to locate binaries and manage server processes
 ├─ smoke/
 │   └─ test_smoke.py   # Python harness implementing Step C smoke scenarios
 ├─ spec/
 │   └─ test_spec.py    # Python harness implementing Step C spec scenarios
 └─ integration/
     └─ test_integration.py  # Python harness running Step C integration flows
```

Use `scripts/run_smoke.sh` (or `ctest -L smoke`) after configuring a build directory to
execute the smoke suite. The spec scenarios can be invoked with `ctest -L spec` or by
calling `python tests/spec/test_spec.py <case>` from a configured build tree. The
integration workflows run with `scripts/run_integration.sh` or `ctest -L integration`
and exercise long-lived networking/IRC paths. All tests expect the latest C
(`logcrafter_c_mvp5`) and C++ (`logcrafter_cpp_mvp6`) binaries to be available in the
active CMake build directory.
