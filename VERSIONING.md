# Versioning Strategy

- **Format**: `MAJOR.MINOR.PATCH` where `MAJOR` tracks the highest completed MVP stage across
  both implementations, `MINOR` increments for substantial cross-track features (e.g., new
  automation suites), and `PATCH` covers maintenance fixes without new SEQ ranges.
- **Current release**: `0.6.0`, representing parity through C MVP5 and C++ MVP6 plus Step C
  automation.
- **Snapshot linkage**: Each increment to `MINOR` or `MAJOR` must be accompanied by fresh
  snapshots under `snapshots/<track>/mvpX` produced via `scripts/snapshot_mvp.sh`.
- **Change tracking**: Detailed sequence-by-sequence notes remain in `CHANGES.md`; release
  summaries belong in `CHANGELOG.md`.
- **Bumping flow**:
  1. Ensure `ctest` passes for all suites (`smoke`, `spec`, `integration`).
  2. Capture updated snapshots for the affected track(s).
  3. Run `scripts/new_version.sh <version> "Title" "Bullet..."` to prepend a release entry.
  4. Update any relevant documentation with new SEQ identifiers in chronological order.
