# Using agent-runner as a CI triage step (plan, not implemented)

This is a plan for another agent to execute, not a finished integration. Nothing in
`.github/workflows/` has been touched yet. Read this file, then `AGENTS.md`'s
"Human ownership" section and [firmware-handoff](../../.agents/skills/firmware-handoff/SKILL.md)
before writing any workflow or skill code - the fit described below depends on both.

## What agent-runner is

A standalone, already-running headless `claude -p` job runner on the fork owner's own
host, same LAN as the host this repo's supervisor crate runs on. HTTP JSON API,
bearer-token auth, queued/polling execution, no built-in tools - only the MCP
servers/tools a request explicitly names are ever reachable from a run. Full API
reference: ask the human for `agent-runner-handoff.md` (written for a different
integration, jobseek, but the API surface is identical) - address and token are
handed over out of band, never committed here.

## Why this, not a fifth firmware-handoff reviewer

`firmware-handoff`'s four reviewers (`review-correctness`, `review-architecture`,
`review-embedded`, `review-i18n-docs`) already cover source/diff/history inspection
and already run as subagents inside whatever crate is doing the implementation work -
they need no network hop. agent-runner only earns its place for the one thing those
reviewers *can't* do: react to a change from **outside** that crate's own session -
a real GitHub Actions run, or the human's own Mac session where no subagent dispatch
exists. Its job is automated triage evidence (a fifth data point to read before
deciding), never a sixth vote that decides anything by itself.

## Hard blocker: network reachability

`ci.yml`'s jobs run on `runs-on: ubuntu-latest` - GitHub-hosted runners with no path
to this LAN's `192.168.68.0/24`. agent-runner cannot be reached from there as-is.
Three ways to close that gap, in the order they're worth trying:

1. **Don't put it in GitHub Actions at all.** Neither `push: branches: [master]` nor
   a `pull_request` trigger has ever actually fired for this fork (`gh run list` is
   empty) - work happens on `dkej/x4pro`, no PRs are opened from this fork against
   upstream. If that stays true, the realistic integration point is a step the
   **agent itself runs locally** before or alongside `firmware-handoff` (a plain
   `curl`/script call to agent-runner from this session), not a workflow file. Check
   current branch/PR habits with the human before assuming this is still accurate.
2. **Self-hosted runner**, same pattern as the sibling `hrpill` project
   (`gh-runner-hrpill`, see that repo's `.github/workflows/device-tests.yml` and the
   supervisor's `shared-env`/`shared-env-pool` notes) - a container on this host
   registered as this repo's runner, which can reach `192.168.68.x` directly. Real
   infra work (a new `gh-runner-*` container), ask the human first.
3. **Tailscale.** This host already runs tailscaled for other crates (see the
   `remote-adb-tailscale` pattern). A GitHub-hosted runner could join the tailnet
   for the one job and reach agent-runner over it - lighter than a self-hosted
   runner, but still new infra (tailscale auth key as a repo secret, tailnet admin
   approval) and an extra moving part for something that's advisory, not required.

Don't build 2 or 3 speculatively. Confirm with the human whether this repo's CI even
needs to react automatically, or whether "call it from the current agent session" (1)
already satisfies the actual want.

## What it must do, and must not

This repo's governance (`AGENTS.md` → Human ownership) is explicit: *"Fully
autonomous end-to-end agents are forbidden,"* reviewers may not write files, run
builds, commit, publish, or flash, and the human owns PR prose and push decisions.
An agent-runner triage step inherits all of that - it is not exempt because it runs
outside the crate:

- **Read-only tools only.** `allowed_tools` in the `POST /v1/runs` body should name
  nothing but a read-scoped MCP (e.g. a GitHub MCP server backed by a read-only PAT,
  for diff/PR/commit data) - never a write-capable GitHub token, never `Bash`/`Write`
  (agent-runner already can't expose those; don't ask for an MCP that re-creates
  them).
- **Advisory output only.** Its result is evidence placed where the human or the
  next firmware-handoff round reads it - a job summary, a non-approving PR comment,
  a line a reviewer dispatch packet can cite - never a merge gate, never something
  that blocks a push or triggers a flash/release step on its own.
- **No duplicate authority.** Don't ask it to re-decide what `firmware-handoff`'s
  reviewers already own (correctness/architecture/embedded/i18n axes); ask it
  narrower questions a human would otherwise have to check by hand before even
  starting that process - e.g. "does this diff touch files outside `SCOPE.md`'s
  stated boundaries," "does any new allocation lack the heap-discipline
  justification `hardware-resources.md` requires," "does `CI/CD Pipeline
  Awareness`'s board-matrix note apply to this diff." Keep the prompt scoped to one
  or two concrete checks per call, not a vague "review this."

## Request shape (once reachability is solved)

```json
POST /v1/runs
{
  "prompt": "<one or two concrete triage questions, with the diff/PR number and SCOPE.md/hardware-resources.md excerpts inlined or fetchable via the MCP tool>",
  "engine": "claude-code",
  "mcp_servers": {
    "github-readonly": {
      "type": "http",
      "url": "<a read-only GitHub MCP endpoint>",
      "headers": {"Authorization": "Bearer <scoped PAT, from a repo secret, never Bash/Write-capable>"}
    }
  },
  "allowed_tools": ["mcp__github-readonly__*"],
  "max_turns": 20,
  "timeout_s": 300,
  "metadata": {"task_id": "crosspoint-pr-<n>-triage"},
  "callback_url": null
}
```

Poll `GET /v1/runs/{run_id}` (or `/events`) rather than relying on a callback until
there's a concrete place for one to land. `error: "mcp_unreachable:<server>"` means
the read-only MCP endpoint wasn't reachable/authed - fail the triage step loudly,
don't silently skip it.

## Open questions for the human before building anything

- Does this repo actually want automatic reaction to every push/PR, or is
  "ask agent-runner when I ask for it" (option 1 above) enough?
- Who hosts the read-only GitHub MCP server this would use, and what's its PAT
  scope?
- Where should the triage output land - job summary, PR comment, a file
  `firmware-handoff`'s reviewers read? Pick one before writing any code.
