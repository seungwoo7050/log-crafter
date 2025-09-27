# Step C5 Deliverables Checklist

## Step A – Specs & Diagrams
- **Pass – Planning documentation**: Architecture overview, API, protocol, error, data model, performance, migration, and diagram references are published under `docs/`.【F:docs/RFC-0001-overview.md†L1-L40】【F:docs/API.md†L1-L40】【F:docs/Protocol.md†L1-L40】【F:docs/Errors.md†L1-L40】【F:docs/DataModel.md†L1-L40】【F:docs/Performance.md†L1-L40】【F:docs/Migration.md†L1-L40】【F:docs/Diagrams/component-shared.txt†L1-L40】
- **Pass – Sequence mapping and assumptions**: Legacy-to-SEQ crosswalk and explicit assumptions captured for both tracks.【F:docs/SequenceMap.md†L1-L40】【F:docs/Assumptions.md†L1-L20】

## Step B – Repository & Code Foundation
- **Pass – Active work trees**: C and C++ implementations live under `work/` with SEQ-annotated sources and build targets wired through CMake.【F:work/c/src/main.c†L1-L40】【F:work/cpp/src/main.cpp†L1-L60】【F:CMakeLists.txt†L1-L40】
- **Pass – Snapshots**: Frozen copies exist for every MVP stage across both tracks under `snapshots/`.【F:snapshots/c/mvp5/src/main.c†L1-L40】【F:snapshots/cpp/mvp6/src/main.cpp†L1-L60】
- **Pass – Release collateral**: CHANGELOG, VERSIONING guidance, snapshot tooling, and the new-version helper script reside at the repository root and under `scripts/`.【F:CHANGELOG.md†L1-L8】【F:VERSIONING.md†L1-L16】【F:scripts/snapshot_mvp.sh†L1-L24】【F:scripts/new_version.sh†L1-L67】【F:scripts/README.md†L1-L28】

## Step C – Tests & CI
- **Pass – Automated suites**: Smoke, spec, and integration harnesses registered with CTest alongside helper scripts and load generator utilities.【F:tests/CMakeLists.txt†L1-L63】【F:tests/smoke/test_smoke.py†L1-L200】【F:tests/spec/test_spec.py†L1-L120】【F:tests/integration/test_integration.py†L1-L200】【F:scripts/run_smoke.sh†L1-L16】【F:scripts/run_integration.sh†L1-L16】【F:tools/load_generator.py†L1-L40】
- **Pass – Continuous integration**: GitHub Actions workflow builds the project and executes the full `ctest` suite, with recent runs passing locally.【F:ci/github-actions.yml†L1-L23】【cd6edc†L1-L11】
- **Pass – Developer CLI guidance**: A dedicated CLI operations guide documents every build, test, runtime, snapshot, and release command to support learners progressing through SEQ milestones.【F:docs/CLI-Operations.md†L1-L200】
