# EXT-17 — Headless Campaign Runner

Runs many unattended N8RO simulation runs, varies one input across them, judges each against
conditions declared outside the code, and reports across the campaign.

**Status: milestone 2 of 7.** What exists today is the execution half — one run, automated, with
an explicit end and a bounded timeout, repeatable unattended. There is no capture reader (M3),
no determinism self-test (M4), no parameterisation sweep (M5), and no assertions or campaign
report (M6). `n8ro-campaign` therefore reports three outcomes rather than four; see
[The four outcomes](#the-four-outcomes).

The binding contract is `docs/prd.md`, which is itself written against the client brief. The
capture format EXT-17 consumes is vendored, read-only, in `contract/`.

---

## The stop predicate — what "the run is finished" means

> **A run is finished when the engine's frame number, as published on `sim/engine/state`,
> reaches N.** N is the campaign's `--frames`. No wall-clock quantity participates in the
> decision.

This is the answer to OQ-1, decided at M2 against the four criteria the PRD fixed in rev 1 and
measured over twenty runs. The evidence is in [`docs/m2-oq1.md`](docs/m2-oq1.md); the short form:

| OQ-1 criterion | Verdict |
|---|---|
| (a) observable from what the run publishes | Yes — the frame number is a field of `sim/engine/state`, and every run records the value it saw |
| (b) free of wall-clock quantities | Yes — the predicate's only input is an engine-state snapshot. There is no clock in its scope to read |
| (c) identical across two runs of one configuration | Yes, and measured: **twenty runs, one distinct observed frame (1200)**, one distinct `simTime` (60.0), one distinct `dt` (0.05000). **The same claim is false of anything counted in the capture** — those twenty runs produced **seventeen** distinct sample counts, spanning 50 361 to 50 552 |
| (d) reached by every run without the timeout firing | Yes — **20 completed, 0 timeout, 0 infrastructure error** |

Two consequences worth carrying:

- **The predicate lives on the engine-state side, deliberately.** Frame number comes from the
  engine and is unaffected by publication loss; sample counts are not. A predicate keyed on
  anything counted in the capture cannot satisfy criterion (c), and the twenty-run table shows
  it failing rather than argues that it would.
- **`--capture-max-samples` is not a stop predicate**, though it looks like one. It is the
  recorder's record-count safety bound. Two runs stopped at the same sample count have stopped
  at *different* simulation frames, which is the failure CR-EX-3 exists to prevent. It is an
  input to OQ-6, not to this.

`dt` was `0.05000` on every frame of every run measured so far, so on this configuration a frame
budget and a simulation-time budget are the same predicate. **The predicate is stated in frames**
because the frame counter is the quantity the engine publishes directly; the 20 Hz coincidence is
a property of this configuration and not something to depend on.

---

## Building

Requires Visual Studio's x64 toolchain and the N8RO SDK at `C:\N8RO`.

```
tools\n8ro-campaign\build.cmd      ->  build\n8ro-campaign\n8ro-campaign.exe
tests\build.cmd                    ->  builds and runs the tests
```

`tests\build.cmd` links nothing and needs no N8RO install — it covers the parts whose
correctness is about our own output rather than about the platform.

The build ends by running the binary's own `--help` and comparing it against
`tools\n8ro-campaign\help.golden.txt`. A drift fails the build. This is deliberate: the PRD does
not enumerate the option list in prose, because a list nobody executes is exactly what drifted in
the sibling project. **The golden file is the CLI's specification.**

## Two environment preconditions, both measured

Neither is optional and neither covers the other.

| | Why |
|---|---|
| `N8RO_RELEASE=C:\N8RO` | Without it the host resolves its plugin directory from its working directory, skips the plugin scan, never registers `componentPhysics`, and **refuses every 42-entity scenario load while sitting idle rather than failing** |
| `C:\N8RO\bin` on `PATH` | Where `n8ro-sim.dll` and `n8ro-core.dll` resolve from. Without it an SDK-linked binary exits 53 having produced no output — it looks like a crash and is a missing DLL |

`n8ro-campaign` sets both for the processes it starts (`--n8ro-release`, `--path-prepend`). It
needs `PATH` for itself, because it links the SDK too.

## Running

```
set PATH=C:\N8RO\bin;%PATH%

n8ro-campaign run-once ^
    --out-dir campaigns\demo ^
    --recorder <path-to-n8ro-bridge.exe> ^
    --scenario "Atacama Air Defense" ^
    --frames 1200

n8ro-campaign repeat --count 20 --out-dir campaigns\twenty --recorder <...> --frames 1200
```

`n8ro-campaign --help` is the authority for the options.

## What a run produces

```
<out-dir>/
  campaign.log                     the driver's own transcript
  campaign.json                    repeat only: outcome counts and a row per run
  runs/
    000/
      run.json                     the per-run record
      capture-<scenario>-000.n8rocap.jsonl
      host.out  host.err           the host's streams
      host-logger.log              this run's slice of the host's shared log
      recorder.out  recorder.err
      host/                        the host's working directory
```

Run ids are **zero-padded ordinals, never timestamps** — two identical runs must be addressable
as a pair, and a wall-clock name makes them unaddressable.

`run.json` carries the stop predicate, its one-sentence statement, the frame at which it first
held, the timeout and whether it expired, every wait with its bound, and every process with its
pid and exit code. Everything in it that varies between identical runs is fenced inside one
`diagnostics` object which says, in the file, that it is excluded from every comparison.

**A non-zero host exit code is normal.** The engine's command vocabulary is closed at
`start` / `stop` / `pause` / `step` — there is no shutdown command, so the host is ended with a
console control event, and Windows reports `3221225786` (`0xC000013A`) for that. The host still
shuts down in an orderly way. `terminated_by: "handle"` in the record is the case to look at.

## The four outcomes

The PRD requires four, never collapsed: **pass, fail, timeout, infrastructure error.** At M2
there are three, because nothing judges a run yet:

| Outcome | Means |
|---|---|
| `completed` | The stop predicate was satisfied and teardown was clean. **Not a pass** — no condition has been evaluated. `pass` and `fail` replace it at M6 |
| `timeout` | The run timeout expired before the predicate was satisfied. Its own outcome, never a failure |
| `infrastructure_error` | The harness, the host, or the scenario load failed. Never a failing scenario |

Exit code: `0` if every run completed, `1` if any did not, `2` for a usage error before any run
was attempted.

## Limits — what a result here does and does not prove

Partial at M2; CR-DOC-1 requires the full version at M7.

- **The capture is a high-fidelity sample of the published stream, not a transcript, and no
  counter reports the difference.** Measured in this project's own runs: samples go missing with
  all fourteen platform counters reading zero. Nothing built here may read the absence of a
  record as evidence that something did not happen.
- **A run's captured sample count varies between identical runs; its stopping frame does not.**
  Measured over twenty runs: seventeen distinct sample counts, up to five whole frames missing, a
  0.38% spread — with every drop and bus counter reading zero in all twenty files. Meanwhile all
  twenty agree exactly on the roster lifecycle (`entities_added: 89`, `entities_removed: 47`,
  adds `{occupancy 1: 47, occupancy 2: 42}`). **The simulation is reproducible; its publication
  schedule is not.** That asymmetry is the basis of the stop predicate above and of the M4 gate.
- **A run's teardown segment is usually empty.** In 15 of 20 runs segment 1 is opened and closed
  with no sample in it. The format's frozen-clock test cannot classify an empty segment, and a
  segment list built only from segments carrying samples loses it — see `docs/m2-oq1.md`.
- **The install has no geoid grid and no elevation service**, so every run floods with
  `TerrainElevationServiceClient` / `GeoidGridModel` / `requestGoTo 'agl'` errors. This is
  deliberately not fixed: every measurement inherited from EXT-08 was taken in this
  configuration, and provisioning terrain would invalidate the comparability of all of it.
- **The headless invocation is not yet confirmed.** It is measured working; whether it is the
  *intended* production shape is OQ-3, open, in `docs/escalations.md`.

## Boundaries

- **`contract/` is read-only.** It holds the `n8ro-capture/1` specification and fixtures vendored
  from EXT-08. A defect in it goes back to EXT-08 rather than being worked around here.
- **No EXT-08 source, ever** — not a header, not a snippet, not a class name relied upon. The
  host and the recorder are driven as processes.
- **`C:\N8RO` is read-only.** Nothing here writes into the install tree. The host writes its own
  log there; the campaign copies out of it and never into it.
- **The SDK is linked in exactly one place**, `src/control/`. The capture reader (M3) will link
  nothing at all.

## Layout

| Path | What |
|---|---|
| `src/proc/` | Child process supervision. Started, and ended, by handle |
| `src/control/` | The control path. The one place the N8RO SDK is linked |
| `src/run/` | The stop predicate, the run record, and the run itself |
| `src/common/` | Logging and a JSON writer with no run-to-run variation in it |
| `tests/` | Tests that link nothing and need no install. `tests\build.cmd` builds and runs them |
| `tools/n8ro-campaign/` | The CLI, and its golden `--help` |
| `tools/spike-axis/` | M2's R9/OQ-4 feasibility spike. Evidence, not product |
| `tools/m2-checks/` | Throwaway analysis scripts. **Not** the capture reader; M3 builds that |
| `tools/m1-run/` | M1's by-hand driver, kept as the evidence behind `docs/m1-lifecycle.md` |
| `contract/` | Vendored from EXT-08. Read-only |
| `docs/` | The PRD, the milestone records, and the escalations |
