# ADR-0003: Agent Team Workflow

## Status

Accepted.

## Decision

Develop Bedrock with a Claude Code agent team:

- One lead session owns `main`: merging, linear history (no merge commits),
  commit authoring, pushes, tracking-doc updates, and the shared task list.
- Teammate roles:
  - `scout` — upstream research briefs. Odin is the API contract; Go, Rust,
    musl/glibc, BSD libcs, or any relevant runtime may inform implementation.
  - `porter-*` — implementation in per-teammate git worktrees, with plan
    approval required before writing code.
  - `judge` — consumer-perspective API review of every diff before merge;
    owns the standing simplification agenda and the cut list.
  - `breaker` — adversarial testing: fuzz harnesses, sanitizer matrix runs,
    regression tests for review findings.
- The shared task list is seeded from `tracking/odin-port-matrix.md` and is
  treated as disposable: durable state is the repo itself (tracking docs,
  specs, ADRs, and small, frequently pushed commits).
- Scope contract: the port matrix as written. `exclude` rows stay out,
  `redesign` rows wait until the port waves finish, and removal of
  already-landed code happens only from the judge's cut list with the
  explicit recorded approval.
- CI (GitHub Actions) is the merge gate: Linux and macOS across debug,
  release, sanitize, and thread-sanitize; Windows informational until proven
  green; clang-format check.

## Rationale

Teammates have independent context windows and can research, implement, and
review in parallel, but they do not survive session resumption. Keeping all
durable state in-repo, plus per-teammate worktrees on disk, makes any teammate
— or the whole team — cheap to respawn mid-task with nothing lost beyond one
un-merged worktree.

A single writer for `main` keeps history linear and commit style uniform, and
avoids concurrent build and commit races in one working tree.

## Consequences

- Worktrees are transient, harness-managed checkouts; only the lead's merges
  reach `main`.
- Tracking docs are updated in the same slice as the code they describe, so a
  fresh session can reconstruct project state from the repo alone.
- If a session dies, a new lead re-seeds the task list from `tracking/` and
  respawns teammates.
- A teammate that goes unresponsive mid-task gets its uncommitted work
  CHECKPOINT-COMMITTED on its worktree branch by the lead, and the task is
  reassigned; the successor continues from the checkpoint in its OWN worktree
  (cherry-pick or reset onto the checkpoint's branch — never rebase a stale
  checkpoint across an advanced main, which drags stale file states along;
  twice-confirmed hazard).
- A teammate RESUMING after any gap must re-read its task from the board
  (ownership and status) before acting — a takeover may have completed the
  work in the interim, and a stale branch or a duplicate delivery must be
  stood down, not merged.
- Updating the Odin pin invalidates checklist statuses until a delta review
  runs; pin bumps therefore come with a scout task to audit the diff.
