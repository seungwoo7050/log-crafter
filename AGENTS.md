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

## Deliverables
**Step A: Specs + Diagrams**  
- docs/RFC-0001-overview.md (system architecture, differences between C and C++)  
- docs/API.md, Protocol.md, Errors.md, DataModel.md, Performance.md, Migration.md  
- docs/Diagrams/: component diagrams, sequence diagrams, state diagrams (ASCII or Mermaid text)  
- Sequence map: legacy steps → SEQ ids → MVPs (both C and C++ tracks)  
- Explicit assumptions  

**Step B: Repository + Code**  
- src/c/mvp0 … mvp5/  
- src/cpp/mvp0 … mvp6/ (IRC extension in mvp6)  
- CMakeLists.txt with targets for both tracks  
- scripts/new_version.sh, run_smoke.sh  
- CHANGES.md per MVP with SEQ references  
- All code headers annotated with SEQ  

**Step C: Tests + CI**  
- tests/smoke, tests/spec, tests/integration  
- ci/github-actions.yml  
- scripts/run_integration.sh  
- All tests runnable via ctest  

## Versioning and Sequences
- Global SEQ ids: SEQ0000, SEQ0001, … (no gaps, no duplicates)  
- Both C and C++ tracks share the same global sequence space.  
- MVP checkpoints remain. Each change ties to a SEQ.  
- Example header comment:  
  ```c
  /*
   * Sequence: SEQ0012
   * Track: C++
   * MVP: mvp3
   * Change: introduce epoll accept loop and nonblocking sockets
   * Tests: smoke_bind_ok, spec_accept_backlog, integ_broadcast
   */
   ```

## Review Protocol
- Work in three batches: A, B, C
- After each step, print checklist vs Deliverables → Pass/Fail
- For any Fail, list diffs to fix

## File Selection Policy
- Codex can directly open and read files from the cloned repository.
- You must not blindly scan everything: always ask me to confirm scope first.
- For Step A, request the repo tree and propose the top priority files (max 10) to analyze first.
- I will approve or adjust the list, then you open those files directly from the repo.

## Assumption Policy
- If info is missing, list explicit assumptions.
- Do not invent new features outside the original plan.
- If legacy C and C++ differ, document divergence explicitly in Migration.md.

## Output Format
- Use fenced code blocks with relative paths.
- Code must compile with CMake and pass ctest.
- Docs in Markdown, diagrams as plain text.
- When you open files, print the relative path and short summary first, then the extracted content.
- Always explain why each file is relevant to spec, architecture, or MVP sequencing.
