# Scripts

Sequence: SEQ0079  \
Track: Shared  \
MVP: Step C  \
Change: Document Step C automation helpers.  \
Tests: smoke_bind_conflict, smoke_dual_listener, smoke_shutdown_signal, smoke_max_clients,
smoke_irc_boot, smoke_persistence_toggle

- `run_smoke.sh` – Configure, build, and execute the smoke test label via `ctest -L smoke`.
- `snapshot_mvp.sh` – Preserve the current `work/<track>` tree into `snapshots/<track>/<mvp>` (added earlier in Step B).

Additional CI and integration scripts will land alongside future Step C sequences.
