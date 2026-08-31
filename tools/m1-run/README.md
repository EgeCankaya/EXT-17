# `m1-run` — the M1 by-hand run driver

Not the campaign runner. This is the M1 exploration tool ([B] step 1), kept because it is the
evidence behind `docs/m1-lifecycle.md` and because M2's control path starts from what it learned.

## Why it exists

Nothing the platform ships publishes a bus command from a script. `n8ro-shark` is a passive
subscriber, `n8ro-sim-starter` is a process launcher, `n8ro-sim-bot` is an MCP/ZMQ server, and
`n8ro-workbook` — the documented interactive driver — is GUI-only. The host itself takes no
scenario argument. So driving one run by hand needed ~200 lines of our own.

## What it links

The N8RO SDK only: `SimulationEngineClient` (`n8ro-sim.lib`, `n8ro-core.lib`). **No EXT-08
source, no EXT-08 identifier** — the boundary the repo split exists to enforce.

## Build and run

```
tools\m1-run\build.cmd            ->  build\m1-run\m1-run.exe

set N8RO_RELEASE=C:\N8RO
m1-run.exe --scenario "Atacama Air Defense" --frames 1200
```

Requires a host already running and, if you want a capture, a recorder already attached — the
`entity_created` burst that fills the roster is published once, at scenario load.

Options: `--sim-config --model-path --schema-file --model-name --scenario --frames --settle-ms`.

## Two habits it keeps, because M2 inherits them

- **Never throws** (constraint C3). Every failure is a return value plus a log line; the exit
  code says which stage failed.
- **No wall clock decides anything.** `steady_clock` appears only in the bounded timeouts
  CR-EX-2 requires each wait to have, and never in anything a run reports.
