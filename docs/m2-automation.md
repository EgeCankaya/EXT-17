# M2 — one run, automated, and what it takes to end one

**Date:** 2026-08-31
**Milestone:** M2 ([B] step 2 — *"Automate one run: start the host, wait for it to be ready,
load, run, detect the end, tear down"*)
**Platform:** N8RO runtime 2.1.328, SDK component `com.n8ro.dev` 2.1.328
**Deliverable:** `n8ro-campaign`, built from `src/`; two spikes; OQ-1 decided
**Evidence:** `campaigns/m2-oq1/` — twenty runs, their captures, records and logs. Untracked by
`.gitignore`; every number below is re-derivable from it with `tools/m2-checks/`.

> Every number in this document was measured here, during this milestone. Where it reproduces
> something from M1 or from `contract/PROVENANCE.md` it is stated as a *reproduction*. Where it
> does not, that is said too. Nothing is inherited silently.

---

## 1. What was built, and what is a spike

M1 left a ~200-line exploration tool. M2 turns its control path into components and adds the
process supervision, the run record and the CLI around them.

| Component | Owns | Requirement |
|---|---|---|
| `src/proc/Process` | Child launch with an explicit handle, environment provisioning, per-run stdio, bounded wait, **termination by handle only**, and image-name *detection* for the pre-flight refusal | CR-EX-1 |
| `src/control/EngineControl` | The whole control path, and the one place the N8RO SDK is linked. Every wait is an observed condition on `sim/engine/state` with a bounded, logged timeout | CR-EX-2 |
| `src/run/StopPredicate` | "Finished", as an object: its one-sentence statement and the value it evaluated to | CR-EX-3 |
| `src/run/RunRecord` | One JSON record per run, written on **every** path out including the ones where nothing started | CR-EX-3, CR-EX-4 |
| `src/run/RunOnce` | The sequence, the outcome classification, and teardown | CR-EX-2/4/5 |
| `tools/n8ro-campaign` | The CLI: `run-once`, `repeat`, and a golden `--help` compared on every build | CR-DOC-1 |

Deliberately **spikes** — evidence, then delete:

- `tools/spike-axis/` — the R9/OQ-4 axis feasibility probes (§5).
- `tools/m2-checks/*.py` — throwaway analysis. **Not the capture reader.** M3 builds that in
  C++ from `contract/capture-format-v1.md` alone, and these scripts are narrow on purpose so
  they do not become a second one by accident.

`tools/m1-run/` stays as the evidence behind `docs/m1-lifecycle.md`. It is not built into
anything.

---

## 2. Every wait is an event, not a poll — and that is what the criterion asked for

CR-EX-2's sharpest clause is that *"a code search for sleep primitives in that path returns
nothing"*. M1's driver could not have met it: it polled `getFrameNumber()` every 20 ms and slept
750 ms between teardown observations.

`EngineControl` subscribes to `sim/engine/state` and every wait is a `std::condition_variable::
wait_until` on a bounded deadline, woken by that subscription. There is no poll interval to tune
and no fixed delay anywhere.

```
$ grep -rniE 'sleep|this_thread|usleep|delay' src/ tools/n8ro-campaign/
src/control/EngineControl.cpp:  ... // heartbeat. No sleep, no poll interval; ...
src/control/EngineControl.h:    ... // CR-EX-2's sharpest criterion is that "a code search ...
tools/n8ro-campaign/main.cpp:   ... "bring-up timeouts (each bounds one observed condition; ...
```

Three comments and one line of help text. No sleep primitive. For contrast, the same search over
`tools/m1-run/main.cpp` returns three real ones.

Three properties follow, and each is load-bearing rather than tidy:

- **The heartbeat is what re-evaluates a condition.** `sim/engine/state` publishes continuously
  while the host lives, so a condition that becomes true is observed on the next publication.
  A wait is never later than one frame.
- **A dead host is a timeout on our own wait, not a conclusion from silence.** If the host dies
  the notifications stop, the deadline is reached, and the run becomes an
  `infrastructure_error`. This is the distinction PROVENANCE finding 5 makes load-bearing: the
  campaign never reads a missing record as evidence that something did not happen. It reads its
  own clock running out as evidence that *it stopped being told anything*, which is a different
  claim and a sound one.
- **The condition is evaluated with our own lock released.** It reads the SDK client's getters,
  which take the client's lock, while the client's pump calls our handler, which takes ours.
  Evaluating under both would order the two locks oppositely on the two threads. That was the
  shape of the first implementation, it never hung across a dozen runs, and it was replaced
  before the twenty-run evidence was taken rather than after — a deadlocked wait in an
  unattended campaign is exactly the failure this project exists to avoid, and "it did not
  happen yet" is not a measurement.

---

## 3. What M2 measured about the environment that M1 did not

Three findings, all from automating what M1 did by hand. All three are now in `CLAUDE.md`, and
two are added to OQ-3's escalation as questions (e) and (f).

### (a) `C:\N8RO\bin` on `PATH` is a second precondition, separate from `N8RO_RELEASE`

The first automated attempt exited **53 with an empty log**. `n8ro-sim.dll` and `n8ro-core.dll`
resolve from `C:\N8RO\bin` and from nowhere else, and a client started from a scratch working
directory cannot load. M1 never saw it because M1's shell happened to have the directory on
`PATH`.

This reads like a crash and is a missing DLL. It is worth writing down for the same reason the
`N8RO_RELEASE` finding was: the failure mode gives no useful signal about its own cause.

### (b) The host appends to one shared log inside the read-only install tree

`C:\N8RO\logs\n8ro-logger-n8ro-sim-app.log` is one fixed filename, and every host process
**appends** to it. Two identical 200-frame runs produced a file of **exactly twice** one run's
size — 1 061 458 bytes against 530 729 — carrying two startup banners.

Two consequences.

**Redirected stderr is not a complete record of a run.** The same run wrote 5 777
`requestGoTo 'agl'` errors and 5 777 `getDistanceToLatLonAlt` errors to the logger file, and —
under PowerShell's `Start-Process` redirection — three lines to `host.err`. Under the campaign's
own inherited-handle redirection it wrote all of them to `host.err`. The logger file is the one
that is always complete.

**A campaign that wants one log per run has to slice it.** `n8ro-campaign` records the file's
size before starting the host and copies only the bytes after it; `host_logger_offset` and
`host_logger_bytes` in each run record say where the slice came from.

*A note against CR-EX-1, stated rather than glossed.* CR-EX-1 requires that no file written by
run *N−1* is read by run *N*. A file written by run *N−1*'s host is read by run *N* — the
campaign reads that log. Three things make it not a breach, and it is worth being explicit about
all three: the file is the platform's, not the campaign's; the campaign reads only the byte range
its own host wrote; and nothing read from it participates in any outcome, verdict or comparison.
It is evidence for a person. If that ever stops being true, this becomes a real problem.

### (c) There is no shutdown command, and the normal host exit code is non-zero

`sim/engine/command`'s vocabulary is closed at `start` / `stop` / `pause` / `step`
(`C:\N8RO\docs\modules\n8ro-sim\dev\engine-lifecycle-and-control.md`). Nothing asks the host to
exit, so ending it is a process signal.

`n8ro-campaign` sends `CTRL_BREAK_EVENT` to the process group it created for the host — which
addresses that host and nothing else — and falls back to termination by handle if it does not
exit. Measured:

| Ending | `.running` marker | Next start's crash rename | Exit code |
|---|---|---|---|
| `CTRL_BREAK_EVENT` | removed | none | `0xC000013A` (`STATUS_CONTROL_C_EXIT`) |
| `TerminateProcess` | left behind | previous log renamed `.crash-<ts>.log` | `1` |

So the control event shuts the host down in an orderly way and still reports a non-zero exit
code. **A non-zero host exit code is the normal case here and is not evidence of a crash.** This
matters for CR-EX-5: the signal that distinguishes "we ended it" from "it died" is
`terminated_by` and `exited_on_its_own` in the run record, never the exit code.

*This does not move R6.* The `0xC000013A` above is our own control event, not an access
violation. No `0xC0000005` was seen in any run of this milestone.

---

## 4. The twenty runs

`n8ro-campaign repeat --count 20 --out-dir campaigns/m2-oq1 --frames 1200`, recorder attached to
every run. **20 completed, 0 timeout, 0 infrastructure error**, unattended, one command.

The full per-run table and OQ-1's decision live in [`m2-oq1.md`](m2-oq1.md). What matters here is
the validation each milestone criterion asked for.

| Criterion | Measured |
|---|---|
| CR-EX-2 — readiness, loaded, started each an observed condition with a bounded, logged timeout | Five waits per run, each recorded in `run.json` with its bound and the frame it was observed at |
| CR-EX-2 — no fixed delay in the run-start path; a code search for sleep primitives returns nothing | §2. Three comments and one line of help text |
| CR-EX-2 — recorder attached before the roster burst; `attached_mid_run` false, orphans zero | **`attached_mid_run: false` in 20 of 20. `samples_orphaned: 0` in 20 of 20** |
| CR-EX-3 — the predicate is stated, and the value it evaluated to is recorded per run | Every `run.json` carries the statement and `observed_frame` |
| CR-EX-3 — twenty runs end at the same point by the predicate's own measure | **One distinct observed frame across 20 runs: 1200.** `simTime` 60.0 and `dt` 0.05000 likewise |
| CR-EX-4 — every run has a timeout; no configuration runs unbounded | `--run-timeout-ms` is always applied and the CLI rejects a value below 1 |
| CR-EX-4 — outcomes sum to runs attempted, none in two categories and none in none | `campaign.json` asserts it: `20 = 20 + 0 + 0` |
| CR-EX-1 — a pre-existing host is a named error before any run starts | Tested with a stray host: `infrastructure_error`, stage `preflight`, run record still written |
| CR-EX-1 — processes ended by handle, never by image name | `Process::terminate` takes only a handle; image names are used for detection and nowhere else |

**Disk, from this project's own runs.** 24 280 305 – 24 382 694 bytes per capture, **486.4 MB for
the twenty**. M1 projected ~506 MB for the same shape from a single run, so the projection was
good to 4%. OQ-6 now has a measured per-run and per-campaign figure at M3 rather than an estimate.

**Three findings the twenty runs produced that one run could not**, all detailed in
[`m2-oq1.md`](m2-oq1.md):

1. **The roster lifecycle is identical in all twenty runs while the sample counts are not** —
   `entities_added: 89`, `entities_removed: 47`, adds `{occupancy 1: 47, occupancy 2: 42}`, every
   time, against seventeen distinct sample counts. The simulation is reproducible; its
   publication schedule is not. PROVENANCE finding 1, reached independently and without running a
   comparison.
2. **Publication loss is larger than M1 measured** — up to five whole frames missing, 0.38%
   spread, with all nine drop and bus counters reading zero in all twenty files.
3. **Segment 1 is empty in 15 of 20 runs**, because the campaign tears down promptly where M1's
   driver lingered six seconds. The frozen-clock test cannot classify an empty segment, and a
   segment list built from sample records alone loses it entirely — which is a trap M3's reader
   is positioned to fall into, and did fall into once here in the M2 checker.

---

## 5. The R9 / OQ-4 parameterisation-axis spike

**This spike does not choose the axis.** OQ-4 is decided at M5. What it establishes is
feasibility per axis, which is what R9 objected to and what the PRD's own quality-gate note asked
for: *"the M2 spike should establish feasibility for at least two of [B]'s three axes so the
choice at M5 is not forced."*

Seven probe runs, `campaigns/m2-axis/`, 200 frames each, recorder attached. Read back with
`python tools/m2-checks/axis_spike.py campaigns/m2-axis`. Every conclusion is read **off the
capture**, never off the fact that a publish returned true: a publish returning true means the
message reached the bus and says nothing about whether the engine kept it, which is the entire
open question M1 left.

### The question M1 left, answered

> *"whether entity state set between `load_scenario` and `start` survives into the run, or is
> overwritten by materialisation."*

**It survives.** `RedUAV_N_01`'s authored start is
`pos [-23.418705, -68.2802, 400]`, `vel [-55, ~0, 0]`. The probes injected
`pos [-23.30000, -68.10000, 900]`, `vel [-80, 25, -5]`.

| Probe | `RedUAV_N_01` at `sim_time_s` 0.05 |
|---|---|
| `p0-baseline` | `pos [-23.418705, -68.2802, 400]` — authored |
| `p1-update-pre` (update before `start`) | **`pos [-23.300036, -68.099988, 900.25]` — injected**, and integrating from it |
| `p2b-update-mid` (update at frame 100) | `pos [-23.418705, -68.2802, 400]` — authored |

`p1`'s entity carries the injected state at its **first published sample** and flies from there
for the whole run. Materialisation does not overwrite it.

`p2b` is the control that makes that reading safe, and it is the reason there are seven probes
rather than six. The first attempt (`p2-update-post`) fired the moment the engine reported
`running` — which is frame 0, before the first frame is integrated — so its effect was
indistinguishable from a pre-start update, and it could not have told a working call from a
surviving one. `p2b` waits for frame 100 first, on the same event-driven bounded wait the
campaign uses everywhere, and the result is unambiguous:

```
t=4.95   pos=[-23.421126, -68.2802, 400]        vel=[-55, 0.0, 0]     authored trajectory
t=5.00   pos=[-23.421150, -68.2802, 400]        vel=[-55, 0.0, 0]
t=5.05   pos=[-23.300036, -68.099988, 900.25]   vel=[-80, 25.0, -5]   <- update applied
t=5.10   pos=[-23.300072, -68.099976, 900.5]    vel=[-80, 25.0, -5]   integrating from it
```

So an entity update takes effect on the **next frame**, at any point in the run, and persists.

### Per axis

| [B]'s axis | Reachable over the bus, no authoring into `C:\N8RO`? | Evidence |
|---|---|---|
| **Initial positions and velocities** | **Yes** | `p1`: injected state present at the first published sample and integrated for the whole run |
| **Which entities are present** | **Yes, both directions** | `p3`: `sendEntityDelete` before `start` produces an `entity_remove` with `reason: "commanded"` and **zero samples in segment 0** — the entity is materialised and removed before a frame runs. `p4`: `sendEntityCreate("Air_UAV_LoiteringMunition_Generic", "SpikeUAV_01")` yields an `entity_add` at occupancy 1 and **199 samples**, a roster of 44 against the baseline's 43 |
| **Which scenario from the catalogue** | **Yes** | `p5`: `Baltic Sentinel`, named from the catalogue query, loads and runs — 18 entities against Atacama's 43 |

**All three of [B]'s axes are reachable, and R9's objection does not bite on any of them.** That
is a stronger result than the PRD assumed: R9 is rated *"Unknown — not yet investigated"*, and
OQ-4 names "which scenario from the catalogue" as the fallback *"because it needs no authoring
at all"*. On this measurement none of the three needs authoring, so **OQ-4 at M5 is a free choice
on merit rather than a choice forced by the read-only install tree.** R9 can be re-rated.

Two limits of this spike, stated so M5 does not over-read it:

- **It measured feasibility, not fidelity.** That an injected position is honoured says nothing
  about whether a swept range of positions produces a scenario that still makes sense — an entity
  placed somewhere absurd is reachable and useless.
- **`p3`'s deleted entity still appears in `entity_add` and in `trailer.counts`.** The roster is
  materialised first and the delete lands before frame 1. A campaign varying "which entities are
  present" this way must count presence by samples, not by adds — and CR-AS-4 forbids concluding
  from absence, so an assertion over that axis needs care that M6 will have to design.

---

## 6. What M2 did not do

- **It did not choose the parameterisation axis.** That is OQ-4 at M5. §5 reports feasibility
  per axis and stops there.
- **It did not run the determinism comparison.** That is M4, gated on OQ-2. What M2 owes M4 is
  that two runs of one configuration are bounded identically, and §4 is that evidence.
- **It did not build the capture reader.** That is M3, from `contract/capture-format-v1.md`
  alone. Captures were read at M2 only for `attached_mid_run`, orphan counts and segment
  structure, by a throwaway script that is deliberately not a reader.
- **It did not confirm the invocation.** OQ-3 is still drafted and unsent
  (`docs/escalations.md`), now with two more specifics from what this milestone measured. M2
  built on the bus-publish route deliberately, and put the whole control path in one component
  so that a different answer costs one file.
- **It did not decide the disk ceiling.** That is OQ-6 at M3. §4 gives it this project's own
  per-run and per-campaign numbers to work from.
