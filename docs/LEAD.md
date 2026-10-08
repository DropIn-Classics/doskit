# Leading workers

How one agent session (the lead) runs a project with other agent
sessions (the workers), each on its own machine and checkout. It adds to
the project's AGENTS.md and PROVENANCE.md and never overrides them. It
was written from running a project with a Windows lead and Linux and
macOS workers driven through herdr; the rules hold for any way of
reaching the workers.

## Roles

- The lead reviews, merges, keeps `master` and hands out tasks. It
  implements itself only when no worker can, or for small fixes found
  while merging.
- A worker does one task at a time on its own branch, named after its
  machine (`debian/<topic>`, `mac/<topic>`), based on `origin/master`.
- The person who owns the project is often away. Keep workers busy and
  tell the person when something needs them or a long task has ended.

## Branches and master

1. Only the lead commits to and merges into `master`. Workers push their
   own branch, never `master`.
2. The lead pushes `master` only when the person has allowed it.
3. History is never rewritten (no force push, no rebase of pushed work).
4. Every commit, the lead's merges included, goes through the
   project's checks (`tools/check.py` says `all ok`, the port builds) by
   the `doskit:git-committer` agent. Hooks are not skipped.

## Merging a worker's branch

1. Only when the worker says its branch is committed and pushed, and it
   has no shell or run still going.
2. `git fetch`, then `git merge --no-ff --no-commit origin/<branch>`.
3. Resolve conflicts in docs (HANDOFF, port README) by keeping both
   sides, each entry once, in order. Do not hand this to a subagent:
   merges done that way have dropped text and mixed entries.
4. Check: no conflict markers left; no address or name given twice in
   the hints; files generated from the hints regenerated (e.g.
   `tools/symmap.py`), and local defines that now duplicate a generated
   name removed; line endings as before.
5. Build and run the checks, commit the merge with a message saying what
   the branch brought, push if allowed.

## Handing out a task

1. Merge the worker's finished branch first, so the next one starts from
   it.
2. Clear the worker's context (`/clear`) before a new task. Never while
   it still has background shells or unpushed work.
3. Write the task as one self-contained prompt; the worker remembers
   nothing. It names:
   - what to read first: AGENTS.md, PROVENANCE.md, README.md,
     docs/HANDOFF.md;
   - the rules: own branch from `origin/master`, push only that branch,
     check.py before each commit, say what was not verified, notes into
     HANDOFF and the port README with the change;
   - where things stand and what the last merges changed;
   - the task, what counts as done, and what to compare against the
     original;
   - "reply with a short summary when done".
4. Give the two workers tasks that touch different files, so merges stay
   small. When one task changes something another worker relies on, tell
   that worker in a short note (what changed, merge `origin/master`
   before committing, what to rerun).

## Watching

- Poll the workers every one to two minutes in the background. A worker
  is free only if it is not working and has no shells running, seen on
  two polls in a row. "Done" with a shell still running is busy.
- If a run takes far longer than it should, ask the worker. A wait loop
  built on `pgrep -f` can match its own command line and wait forever;
  tell workers to avoid that.
- Remote services fail now and then (push errors): retry after a pause
  before treating it as a problem.

## The lead's own notes

- Keep what a later lead session needs (who the workers are, how to
  reach them, what each is doing, what is merged) in its memory or a
  note outside the repository, dated, and update it at each handoff.
- What was learned about the game goes into the project's docs through
  the workers' commits, as AGENTS.md says, not only into the lead's
  notes.
