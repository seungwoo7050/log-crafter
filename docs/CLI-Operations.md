# CLI Operations Guide

Sequence: SEQ0108  \
Track: Shared  \
MVP: Step C  \
Change: Document end-to-end command-line workflows for builds, tests, runtime, and release.  \
Tests: documentation_only

## 1. Audience and Goals
This guide targets learners following the MVP/SEQ curriculum who need a single reference for every terminal command that keeps the project moving—from configuring builds to cutting releases. Each workflow includes copy-paste ready examples and troubleshooting notes so contributors can practice confidently without guessing the syntax.

## 2. Environment Preparation
1. Ensure the toolchain is installed:
   - GCC/Clang with C11 and C++17 support
   - CMake 3.20+
   - Python 3.10+
   - `ninja` or `make` (optional but recommended)
2. Clone the repository and start in the project root:
   ~~~bash
   git clone <repo-url>
   cd log-crafter
   ~~~
3. Clean up old build artifacts when switching branches:
   ~~~bash
   rm -rf build
   ~~~

## 3. Configure and Build
All build commands run from the repository root.

| Step | Command | Notes |
|------|---------|-------|
| Configure | `cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo` | Creates (or refreshes) the `build/` tree with tests enabled.【F:CMakeLists.txt†L1-L40】 |
| Build | `cmake --build build --parallel` | Builds every MVP target plus tests. Use `--target logcrafter_c_mvp5` to compile only the latest C binary if desired.【F:work/c/CMakeLists.txt†L1-L40】 |
| Clean | `cmake --build build --target clean` | Removes compiled objects while keeping configuration files. |

When switching toolchains or generators, rerun the configure step to update CMake cache files.

## 4. Running the Servers Locally
After building, binaries live in `build/work/<track>/`.

### 4.1 C Track (MVP5)
- Binary: `build/work/c/logcrafter_c_mvp5`
- Typical launch:
  ~~~bash
  build/work/c/logcrafter_c_mvp5 -p 11000 -P -d ./logs/c -s 25
  ~~~
- Flags:
  | Flag | Purpose | Default |
  |------|---------|---------|
  | `-p PORT` | Listener port (query port binds to `PORT-1`). | `9999` |
  | `-P` | Enable persistence layer. | Disabled |
  | `-d DIR` | Directory for persisted logs. | `./logs` |
  | `-s SIZE_MB` | Rotation threshold in megabytes. | `10` |
  | `-h` | Print usage banner and exit. | — |

Stop the server with `Ctrl+C` (SIGINT) to ensure a graceful shutdown and persistence flush.

### 4.2 C++ Track (MVP6)
- Binary: `build/work/cpp/logcrafter_cpp_mvp6`
- Typical launch (with IRC):
  ~~~bash
  build/work/cpp/logcrafter_cpp_mvp6 -p 12000 -P -d ./logs/cpp -I 6669 -i
  ~~~
- Additional flags beyond the C track:
  | Flag | Purpose | Default |
  |------|---------|---------|
  | `-i` | Enable IRC bridge with default port 6667. | Disabled |
  | `-I PORT` | Override IRC port when `-i` is supplied. | `6667` |

### 4.3 Quick Smoke Interaction
1. Start the desired server in one terminal.
2. From another terminal, send a log entry:
   ~~~bash
   printf 'level=INFO msg="hello"\n' | nc localhost 11000
   ~~~
3. Query it back:
   ~~~bash
   printf 'QUERY level=INFO\n' | nc localhost 10999
   ~~~

## 5. Test Execution Matrix
Every suite is available through `ctest`. Commands assume the project has already been built.

| Purpose | Command | Coverage |
|---------|---------|----------|
| Smoke regression | `scripts/run_smoke.sh` | Fast readiness checks (bindings, shutdown, capacity, IRC boot).【F:scripts/run_smoke.sh†L1-L16】 |
| Spec verification | `ctest --test-dir build -L spec --output-on-failure` | Protocol matrix (happy path, invalid input, partial I/O, timeouts, signal handling).【F:tests/spec/test_spec.py†L1-L160】 |
| Integration flows | `scripts/run_integration.sh` | Multi-client broadcast, persistence replay, IRC bridge workflows.【F:scripts/run_integration.sh†L1-L16】【F:tests/integration/test_integration.py†L1-L200】 |
| Full suite | `ctest --test-dir build --output-on-failure` | Runs smoke + spec + integration (same command CI uses). |
| Targeted rerun | `ctest --test-dir build -R smoke_max_clients --output-on-failure` | Replace regex with the desired test name. |

If a command fails immediately with “unknown option,” double-check spelling (`--output-on-failure`, not `--output-on-failur`).

## 6. Snapshot and Release Tasks
### 6.1 Creating MVP Snapshots
Use the provided script after verifying tests for the track/MVP you want to freeze.
~~~bash
scripts/snapshot_mvp.sh c mvp5
scripts/snapshot_mvp.sh cpp mvp6
~~~
The script copies `work/<track>` into `snapshots/<track>/<mvp>` and refuses to overwrite existing snapshots.【F:scripts/snapshot_mvp.sh†L1-L80】

### 6.2 Publishing a New Version
1. Update `VERSIONING.md` if policies change.
2. Run the helper with the new version string and release summary:
   ~~~bash
   scripts/new_version.sh 1.3.0 "Add Step C CLI operations guide"
   ~~~
3. Review the updated `CHANGELOG.md` entry before committing.【F:scripts/new_version.sh†L1-L68】【F:CHANGELOG.md†L1-L8】

## 7. Continuous Integration Reference
CI reuses the same commands:
1. `cmake -S . -B build`
2. `cmake --build build --parallel`
3. `ctest --test-dir build --output-on-failure`
The workflow definition (`ci/github-actions.yml`) mirrors this order if you need to replicate the pipeline locally.【F:ci/github-actions.yml†L1-L23】

## 8. Troubleshooting Checklist
- **Build directory issues**: Delete `build/` and reconfigure if you see stale include paths or missing targets.
- **Port conflicts**: Kill old server processes or change `-p`/`-I` values when smoke tests report bind errors.
- **Persistence permissions**: Ensure the directory passed with `-d` is writable before enabling `-P`.
- **Long-running tests**: Integration scenarios can take ~2–3 minutes; use the regex filter to iterate faster on failures.

## 9. Quick Reference (Cheat Sheet)
| Task | Command |
|------|---------|
| Configure project | `cmake -S . -B build`
| Build everything | `cmake --build build --parallel`
| Run smoke suite | `scripts/run_smoke.sh`
| Run spec suite | `ctest --test-dir build -L spec --output-on-failure`
| Run integration suite | `scripts/run_integration.sh`
| Run all tests | `ctest --test-dir build --output-on-failure`
| Launch C server | `build/work/c/logcrafter_c_mvp5 -p 11000 -P -d ./logs/c`
| Launch C++ server with IRC | `build/work/cpp/logcrafter_cpp_mvp6 -p 12000 -i -I 6669`
| Snapshot MVP | `scripts/snapshot_mvp.sh <track> <mvp>`
| Prep release log | `scripts/new_version.sh <version> "summary"`

Keep this guide handy while progressing through each SEQ—every critical CLI you need is now centralised in one document.
