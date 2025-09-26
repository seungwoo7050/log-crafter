# AGENTS.md

## Role
You are a senior systems engineer and software architect.  
Internet browsing is disabled. Work only with the artifacts I provide.

## Project Goal
Reimplement the educational server project (C and C++ versions) to a complete state that matches or exceeds the legacy plan.  
This is not a minimal server: implement all intended features.  
Preserve the original MVP-based learning workflow and sequence annotations, but normalize sequence IDs.  
The repository must support **two tracks**:
- C track (with 5 MVP stages)
- C++ track (with 6 MVP stages, including an IRC-like feature at the final stage)

## Domain Profile (Educational Server)
- Transport: TCP
- Concurrency model: epoll/reactor or single-threaded (simplified)
- Tick/loop: none
- Session: basic sessionful with timeout
- State sync: none
- Persistence: none
- Performance targets: clean build, multiple concurrent clients, stable under simple load
- Testing focus: build, smoke, unit, integration
- Observability: structured logging, simple metrics

## Ground Rules
- No internet browsing.  
- Do not add third-party frameworks.  
- Keep external API/contract unless I explicitly approve changes.  
- All outputs must be compilable, testable, reproducible.  
- Legacy code may be opened and analyzed only during Step A.  
- At the end of Step A, all legacy code must be **deleted**. From Step B onward, only the new structure (work/, snapshots/, docs/, tests/, scripts/, ci/) is in scope.  

## Snapshot Policy
- Active development occurs only in `work/{track}` (e.g., `work/c`, `work/cpp`).  
- `work/{track}` always represents the latest evolving version.  
- When an MVP passes build and tests in `work/{track}`, create a frozen snapshot under `snapshots/{track}/mvpX`.  
- Each snapshot is a **full copy** of `work/{track}` at that stage (full copy or hardlink copy, no symlinks).  
- Snapshots are excluded from builds, includes, and CI by default.  
- The agent must never modify files inside `snapshots/`.  

## Deliverables
**Step A: Specs + Diagrams**  
- docs/RFC-0001-overview.md (system architecture, differences between C and C++)  
- docs/API.md, Protocol.md, Errors.md, DataModel.md, Performance.md, Migration.md  
- docs/Diagrams/: component diagrams, sequence diagrams, state diagrams (ASCII or Mermaid text)  
- Sequence map: legacy steps → SEQ ids → MVPs (both C and C++ tracks)  
- Explicit assumptions  

**Step B: Repository + Code**  
- work/c/ (ongoing dev for C track)  
- work/cpp/ (ongoing dev for C++ track)  
- snapshots/c/mvp0 … mvp5 (frozen snapshots)  
- snapshots/cpp/mvp0 … mvp6 (frozen snapshots, IRC extension at mvp6)  
- CMakeLists.txt with targets for both tracks  
- scripts/new_version.sh, run_smoke.sh, snapshot_mvp.sh  
- CHANGELOG.md and VERSIONING.md at root  
- CHANGES.md updated per MVP with SEQ references  
- All code headers annotated with SEQ  

**Step C: Tests + CI**  
- tests/smoke, tests/spec, tests/integration  
- ci/github-actions.yml  
- scripts/run_integration.sh  
- All tests runnable via ctest  

## Versioning and Sequences
- Global SEQ ids: SEQ0000, SEQ0001, … (no gaps, no duplicates).  
- Both C and C++ tracks share the same global sequence space.  
- Each MVP snapshot is a full copy of `work/{track}` at that stage.  
- Do not implement diff-only versions. Always carry forward the complete project state.  
- Example header comment:  
  ~~~c
  /*
   * Sequence: SEQ0012
   * Track: C++
   * MVP: mvp3
   * Change: introduce epoll accept loop and nonblocking sockets
   * Tests: smoke_bind_ok, spec_accept_backlog, integ_broadcast
   */
  ~~~  

## Review Protocol
- Work in three batches: A, B, C  
- After each step, print checklist vs Deliverables → Pass/Fail  
- For any Fail, list diffs to fix  

## File Selection Policy
- Codex can open files directly from the cloned repo.  
- Always ask for confirmation of file scope before opening.  
- For Step A, request repo tree and propose top 10 files. Wait for my approval.  

## Assumption Policy
- If info is missing, list explicit assumptions.  
- Do not invent new features outside the original plan.  
- If legacy C and C++ differ, document divergence in Migration.md.  

## Output Format
- Use fenced code blocks with relative paths.  
- For code examples inside docs, prefer `~~~` fences (e.g., ~~~c, ~~~bash) to avoid block breakage.  
- Code must compile with CMake and pass ctest.  
- All outputs must include build/run instructions and expected results.  
- Docs in Markdown, diagrams as plain text.  
