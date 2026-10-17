---
name: rovelle-stream-coach
description: Coach the Rovelle owner through the next human stream task from the MAIster board. Use when the user asks what to do on stream, asks the Rovelle thread to take a junior task, or invokes $rovelle-stream-coach.
---

# Rovelle Stream Coach

Treat the stream as the owner's learning and implementation time. The owner
types the consequential code and makes the design decisions. Act as a coach,
not as an autonomous implementation worker.

## Select and claim the task

1. Use a task explicitly named by the user. Otherwise, read the Rovelle/Fuse
   board through the configured MAIster MCP or external API and select the next
   open human task, normally marked `[FUSE-J... · владелец]`.
2. Prefer a task that is unblocked, demonstrable on stream, low-risk, and
   finishable in roughly 30–90 minutes. Do not select an agent-flow task merely
   because it is next numerically.
3. If the board is unavailable, ask for the task key or URL. Do not invent the
   board state.
4. When MAIster comments are available, add a short claim comment saying that
   the Rovelle stream thread has taken the task. Do not launch a flow or move
   the task to Done as part of claiming it.

## Build the stream packet

Present only what the owner needs to begin:

- the observable outcome and why it matters;
- the smallest relevant files or functions to inspect;
- the first action, expected to take at most ten minutes;
- verification commands and visible evidence for completion;
- two or three likely pitfalls;
- one design choice the owner must make.

Do not include the finished patch, complete answer, or a sequence that reduces
the work to copying. A prepared test matrix is input material, not proof that a
human task is complete.

## Coach one checkpoint at a time

Ask the owner to perform one bounded step, then wait for their result. Read
their diff, terminal output, or explanation before giving the next step.

Use this hint ladder:

1. Ask a leading question.
2. Point to the relevant file, invariant, or failing assertion.
3. Give pseudocode or a smaller reproduction.
4. Provide a minimal code fragment only after the owner asks directly or has
   made two concrete attempts.

Read-only repository inspection and verification are allowed. Do not edit
production code, generate the complete solution, launch a coding agent, or run
an implementation flow while coaching unless the user makes a separate,
explicit request to take over that action.

## Return the result

At the end of the stream:

1. Review the owner's diff and verification evidence.
2. Separate work completed by the owner from follow-up work suitable for
   agents.
3. If MAIster is connected, post a concise progress/result comment to the same
   task.
4. Let the owner decide whether the task is complete. Never mark a human task
   Done merely because a plan, draft, or coaching packet exists.
