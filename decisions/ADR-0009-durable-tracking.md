# ADR-0009: Local Durable Tracking

## Status

Accepted October 4, 2026, following the user's request to adopt
`durable-tracking` and a task graph.

Supersedes the tracking storage, scope-location, and session-resumption
portions of ADR-0001 and ADR-0003. Their reference-source and team ownership
decisions remain applicable.

## Decision

- Keep one canonical work tracker at `.codex/tracking/TRACKER.md` in the main
  checkout. The existing `.codex/` ignore rule keeps work memory out of Git.
  Every linked worktree uses that same tracker.
- Keep a short current snapshot, active decision links, and an append-only
  checkpoint log. Read the snapshot, current decisions, and newest checkpoint
  before resuming.
- Maintain task state and prerequisite edges in a focused Markdown task
  graph. The task table is authoritative; its Mermaid diagram shows the same
  dependencies. Independent tasks remain independent. A candidate or deferred
  task does not become authorized merely by appearing in the graph.
- Keep detailed decisions, scope, per-module coverage, issue records, and
  evidence in linked companion documents. Read only the relevant records.
  Preserve large output in `evidence/`; never store credentials or secrets.
- Verify recorded state against the live workspace, branch, commit, working
  tree, worktree assignments, and active command sessions. Query remote state
  when it matters. Unknown remote or historical results stay explicitly
  unknown rather than being inferred from a local branch.
- Update the snapshot and append a checkpoint at meaningful changes,
  verification results, decisions, remote actions, blocker transitions,
  long-running command starts/finishes, and before handoff or a final response.
  Record exact session/review/job identifiers when relevant. Correct old
  checkpoints with dated additions rather than rewriting them.
- The lead owns the canonical tracker; teammates supply their evidence and
  status. The tracker never grants permission. The latest user instruction
  and live state override stale records.
- Preserve the old `tracking/` documents unchanged as historical references.
  Stop using them for active task state or scope updates. Their inventories
  are migrated into local module records; old recommendations remain
  recommendations until a current decision adopts them.
- Keep stable design contracts in committed specs and ADRs. Record semantic
  deviations with the code in those specs, and update the ignored local
  records at the same checkpoint.

## Rationale

Package-level status and large research documents previously hid missing
procedures, stale claims, and differences between recommendations and accepted
decisions. A short verified snapshot supports resumption, a dependency graph
supports parallel work, and focused coverage and evidence preserve the detail
without making every session reread the full project history.

## Consequences

- The initial migration retains the previous inventories and independently
  verified Odin evidence, with dated corrections for known stale claims.
- The library build, test suite, and distribution have no tracking dependency.
- Ignored work memory is local. A fresh clone needs the tracking directory
  transferred separately or a new snapshot reconstructed from live state,
  committed contracts, and the current request. Missing records must not be
  invented.
- Completion requires final live-state verification and a checkpoint with
  results, remaining work, blockers, active commands, and concrete next actions.
