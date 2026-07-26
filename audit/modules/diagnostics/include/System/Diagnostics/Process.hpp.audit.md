# Audit: `modules/diagnostics/include/System/Diagnostics/Process.hpp`

## Metadata

- AUDITED: public Process lifecycle, process-tree, output capture, and wait contract.
- Evidence: header/source review, 159/159 Diagnostics tests, official .NET Process documentation, and direct native probes.

## Assessment

The declared "restarts" contract can terminate the caller when a redirected
Process is started again (SR-AUD-270). Destruction neither reaps an unredirected
child nor gives nonblocking ownership release; it leaves a zombie, while a
redirected child makes destruction join the pipe reader until the child exits
(SR-AUD-269). Public access to lifecycle and captured-text state is concurrent
with internal reader threads without synchronization (SR-AUD-271).

`WaitForExit(int)` treats `-1` as an expired timeout rather than infinite and
accepts every other negative value (SR-AUD-268); an interrupted `waitpid`
returns without recording exit state (SR-AUD-272). `Kill(true)` kills only a
process group, not a detached descendant tree (SR-AUD-273).

## Other missing assertions and diagnostics

- Add restart, destructor/zombie, nonblocking-dispose, concurrent output/state,
  `-1`/`<-1`, EINTR, and detached-descendant process-tree tests.
- Surface failed `kill`/`killpg` and `waitpid` errors with stable System errors.

## Final assessment

SR-AUD-268 through SR-AUD-273 apply. No source or test changed.
