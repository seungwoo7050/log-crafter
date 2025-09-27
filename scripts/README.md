# Scripts

Sequence: SEQ0079  \
Track: Shared  \
MVP: Step C  \
Change: Document Step C automation helpers.  \
Tests: smoke_bind_conflict, smoke_dual_listener, smoke_shutdown_signal, smoke_max_clients,
smoke_irc_boot, smoke_persistence_toggle

Sequence: SEQ0105  \
Track: Shared  \
MVP: Step C  \
Change: Document release tooling added during Step C5 validation.  \
Tests: manual_usage_new_version

- `run_smoke.sh` – Configure, build, and execute the smoke test label via `ctest -L smoke`.
- `snapshot_mvp.sh` – Preserve the current `work/<track>` tree into `snapshots/<track>/<mvp>` (added earlier in Step B).
- `new_version.sh` – Prepend a release entry to `CHANGELOG.md` once Step C validation completes and snapshots are refreshed.
- `run_integration.sh` – Convenience wrapper around `ctest -L integration` for the long-running scenarios.
