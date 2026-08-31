# OQ-1 — what predicate defines "the run is finished"

**Status: DECIDED at M2.** The decision is written into `README.md`, which is the PRD's gate for
opening M3.

**Decision**

> A run is finished when the engine's frame number, as published on `sim/engine/state`, reaches
> N, where N is the campaign's `--frames`. No wall-clock quantity participates in the decision.

`ADR-4` in the PRD proposed this shape and named a frame budget as the leading candidate without
choosing it. This document is the measurement that chooses it, and — more usefully — the
measurement that shows the one *other* place a predicate could have been keyed, and why that
place cannot work.

---

## Why the question was hard enough to be an open question

[B] flags it itself: *"Getting 'is it finished' right is harder than it sounds — decide what
defines the end and make it explicit rather than a sleep."* EXT-08 refused the problem in its own
PRD and passed it here. The PRD promoted it to a rev-1 open question with four criteria fixed in
advance, rather than leaving it to be met as a rabbit hole at this milestone.

The criteria, unchanged since rev 1:

> (a) observable from what the run publishes
> (b) free of wall-clock quantities
> (c) identical across two runs of one configuration — because CR-DET-1 cannot compare two runs
>     that stopped at different points
> (d) reached by every run of the reference scenario without the CR-EX-4 timeout firing

**Criterion (c) is the whole difficulty**, and it is the one a single run cannot answer. M1
observed one run and could say only that a frame budget was *observable* and *reached*. Whether
two runs of one configuration stop at the same point is a claim about run-to-run behaviour, and
M1 explicitly did not make it.

## The candidates, and why three of them lose

| Candidate | Verdict |
|---|---|
| **Frame budget** — `frame >= N` off `sim/engine/state` | **Chosen.** Satisfies all four; measured below |
| **Simulation-time budget** — `simTime >= T` | Equivalent *on this configuration* and strictly weaker in general. `dt` was `0.05000` on every frame of every run measured, so 20 Hz makes the two the same predicate — but that is a property of this configuration, and `simTime` is a double where the frame counter is an integer. Nothing is gained and a floating-point comparison is risked |
| **Sample count** — the recorder's `--capture-max-samples` | **Rejected, and it is the trap M1 warned about.** Sample counts are precisely the quantity that varies run to run. Two runs stopped at the same sample count have stopped at *different simulation frames*, which is the failure CR-EX-3 exists to prevent. It is a safety bound, and an input to OQ-6 |
| **The engine ending by itself** | **Not available, and the platform says so.** The documented state machine is `Uninitialized → Initialized → Idle → Running → Stopped`, and the only edge out of `Running` is a `stop` somebody sends (`engine-lifecycle-and-control.md`). There is no terminal state a run reaches on its own and nothing to observe if there were. A *scenario-specific* terminal condition — all Red destroyed, say — is observable, but it fails criterion (d) the moment a parameter sweep changes the outcome, which is the entire point of a sweep, and it would make the end of a run depend on the thing the campaign is trying to measure |

## The measurement

`n8ro-campaign repeat --count 20 --frames 1200`, recorder attached to every run.
486.4 MB of captures. Reproduce with `python tools/m2-checks/oq1_table.py campaigns/m2-oq1`.

| run | outcome | frame | simTime | dt | seg-0 samples | distinct `sim_time_s` |
|---|---|---:|---:|---:|---:|---:|
| 000 | completed | 1200 | 60.0 | 0.05 | 50 389 | 1196 |
| 001 | completed | 1200 | 60.0 | 0.05 | 50 389 | 1196 |
| 002 | completed | 1200 | 60.0 | 0.05 | 50 439 | 1197 |
| 003 | completed | 1200 | 60.0 | 0.05 | 50 486 | 1198 |
| 004 | completed | 1200 | 60.0 | 0.05 | 50 375 | 1196 |
| 005 | completed | 1200 | 60.0 | 0.05 | 50 436 | 1197 |
| 006 | completed | 1200 | 60.0 | 0.05 | 50 552 | **1200** |
| 007 | completed | 1200 | 60.0 | 0.05 | 50 457 | 1198 |
| 008 | completed | 1200 | 60.0 | 0.05 | 50 477 | 1198 |
| 009 | completed | 1200 | 60.0 | 0.05 | 50 436 | 1198 |
| 010 | completed | 1200 | 60.0 | 0.05 | 50 499 | 1199 |
| 011 | completed | 1200 | 60.0 | 0.05 | 50 404 | 1196 |
| 012 | completed | 1200 | 60.0 | 0.05 | 50 361 | **1195** |
| 013 | completed | 1200 | 60.0 | 0.05 | 50 427 | 1197 |
| 014 | completed | 1200 | 60.0 | 0.05 | 50 429 | 1197 |
| 015 | completed | 1200 | 60.0 | 0.05 | 50 365 | 1196 |
| 016 | completed | 1200 | 60.0 | 0.05 | 50 449 | 1198 |
| 017 | completed | 1200 | 60.0 | 0.05 | 50 361 | **1195** |
| 018 | completed | 1200 | 60.0 | 0.05 | 50 530 | 1199 |
| 019 | completed | 1200 | 60.0 | 0.05 | 50 447 | 1197 |

**The two halves of that table behave completely differently, and that is the finding.**

| | distinct values across 20 runs |
|---|---|
| observed frame | **1** — `1200` |
| observed `simTime` | **1** — `60.0` |
| observed `dt` | **1** — `0.05000` |
| seg-0 sample count | **17** — 50 361 … 50 552, a spread of 191 samples (0.38%) |
| seg-0 distinct `sim_time_s` | **6** — 1195 … 1200 |

### The four criteria

**(a) Observable from what the run publishes — yes.** The frame number is a field of
`sim/engine/state`, published every frame. Every run recorded the value the predicate first saw.

**(b) Free of wall-clock quantities — yes, structurally.** `StopPredicate::satisfiedBy` takes an
`EngineSnapshot` and nothing else. There is no clock in its scope to read, and each run record
carries `"wall_clock_participates": false` as a claim the type system already enforces.

**(c) Identical across two runs of one configuration — yes on the engine-state side, and
*measurably not* on the capture side.** One distinct observed frame across twenty runs; seventeen
distinct sample counts across the same twenty. This is the criterion the whole question turned on
and the reason it needed twenty runs rather than an argument: **a predicate keyed on anything
counted in the capture would have stopped these twenty runs at up to six different points.**

**(d) Reached by every run without the timeout firing — yes.** 20 completed, 0 timeout,
0 infrastructure error. The campaign summary asserts
`completed + timeout + infrastructure_error == attempted` and it holds.

### Three things that fell out of the same twenty runs

**Publication loss is confirmed at this project's own scale, and it is larger than M1 saw.**
One run (006) published all 1200 frames; the worst (012, 017) published 1195. Up to **five whole
frames** go unpublished, and the sample-count spread is **191 of ~50 450, or 0.38%** — consistent
with PROVENANCE finding 1's ~0.2% between two runs, and larger than M1's single-run 0.066%.
**All nine `trailer.drops` and `bus_metrics` counters read zero in all twenty files.** Finding 5
is now this project's own measurement twenty times over, not an inherited caution.

**The roster lifecycle is identical in all twenty runs while the sample counts are not.** Every
capture reports `entities_added: 89`, `entities_removed: 47`, and adds of exactly
`{occupancy 1: 47, occupancy 2: 42}`. The five munitions created mid-run, the four expended, the
three destroyed and the forty unloaded happen the same way every time.

> This is the sharpest thing M2 measured, and it is worth stating plainly: **the simulation is
> reproducible and its publication schedule is not.** Twenty runs agree exactly on what happened
> and disagree on what was written down about it. That is PROVENANCE finding 1's claim, reached
> independently, from a different direction, without running a comparison — and it is the
> assumption M4's gate rests on, now supported by this project's own evidence before M4 needs it.
> It is **not** the determinism gate. CR-DET-1 compares sample *values*; this compares structure.

**Segment 1 is usually empty.** In **15 of 20** runs the teardown segment is opened and closed
with no sample in it at all; the other five hold 14, 16, 30, 37 and 42. M1's segment 1 held 2 226,
because M1's by-hand driver watched the engine for six seconds after `stop` while the campaign
tears down as soon as the engine leaves `running`. Two consequences for M3 and M4, both real:

- **The format's frozen-clock test cannot classify an empty or near-empty segment.** The test is
  "more than one sample for one `(entity, occupancy)` at one `sim_time_s`"; with no samples it
  cannot fire. It is still exact when it fires — segment 0 scored `max = 1` in all twenty — but
  a reader that identifies the frozen segment *only* by that test will not identify this one.
  Excluding by zero `sim_time_s` span works and is what `tools/m2-checks` does.
- **A capture's segment list must be built from `segment_open` too, not only from segments that
  carry samples.** The first version of the M2 checker built it from sample records alone,
  reported "1 segment" for those 15 runs, and disagreed with the trailer's own `segments: 2`.
  That was a bug in the checker, not in the files — and it is exactly the mistake M3's reader is
  positioned to repeat.

## What this decision commits M3 and M4 to

- **The comparison window is defined by the predicate, not by the capture's tail.** Two runs are
  comparable because both were bounded at frame N, not because their captures ended alike. M4
  aligns on `sim_time_s` within a segment and must not assume the two files have the same
  number of samples in them — §"The measurement" shows they do not.
- **`--capture-max-samples` stays out of the stop path.** It may be configured as a safety bound
  at OQ-6; if it ever fires, the run's capture is short of the predicate's frame and the run
  record's `observed_frame` is what says so.
- **The predicate is one object with one statement.** `src/run/StopPredicate` has a closed set of
  one kind. Adding a second is a documented change with a reason, not somewhere convenient to
  put a new idea.
