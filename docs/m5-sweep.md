# M5 — One parameterisation axis, and a sweep

**Date:** 2026-08-31
**Milestone:** M5 ([B] step 5 — *"Add one parameterisation axis and run a sweep across it."*)
**Platform:** N8RO runtime 2.1.328; captures from producer `n8ro-bridge` 0.9.0
**Deliverable:** one axis declared in campaign configuration and swept across a campaign, each
run's value in its own record, and the trend legible in the report's own format
**Decision:** OQ-4, resolved on measurement — `docs/m5-oq4.md`, 18 probe runs
**Evidence:** `campaigns/m5-sweep/` (the committed sweep), `campaigns/m5-sweep-first/` (its first
execution, kept — see §5), `campaigns/m5-oq4*/` (the decision)

> Every number in this document was measured here, by this project's own binaries, on this
> machine.

---

## 1. The headline

**One axis, declared in a file, swept across seven values, and the result changes with it for a
reason that can be named.**

```
SWEEP  red_raid_speed_ms  (m/s)  velocityNed magnitude, set before start  -  7 run(s), ordered by value

  value  run   outcome                   adds     keys   samples
  11     000   completed                   48       48     50588  ##
  27.5   001   completed                   48       48     50626  ##
  55     002   completed                   47       47     50471  .
  82.5   003   completed                   54       54     50032  ###############
  110    004   completed                   61       61     49210  ###############################
  165    005   completed                   65       65     48635  ########################################
  220    006   completed                   62       62     48884  #################################

  the bar is `adds` scaled between 47 and 65 - NOT from zero, so a small real change is visible
  and is not a small real change drawn large by accident
  ...
  These are counts read off each capture, NOT verdicts. No condition is declared until M6, so no
  run here is a pass or a fail.
  The determinism gate ran at red_raid_speed_ms = 55 and established determinism FOR THAT VALUE.
  It is one claim at a named point, not one per run.
```

Seven of seven completed. The gate passed at 55 m/s before any campaign run was attempted.

### What the trend actually is

`adds` counts entities created during the run, and in this scenario the entities created during
a run are **weapons**. So the column is the engagement, read straight off the roster:

| speed m/s | CIWS gun rounds | SAM rounds | raiders leaving `nominal` | closest approach, m |
|---:|---:|---:|---:|---:|
| 11 | **0** | 6 | 3 | 7 948 |
| 27.5 | **0** | 6 | 3 | 6 965 |
| 55 *(authored)* | **0** | 5 | 3 | 5 326 |
| 82.5 | **7** | 5 | 8 | 3 687 |
| 110 | **13** | 6 | 12 | 2 048 |
| 165 | **16** | 7 | 13 | 16 |
| 220 | **15** | 5 | 10 | 17 |

**A condition changes outcome, and it is a clean binary flip with a mechanism.** *"No CIWS gun
engaged"* is **true** at 11, 27.5 and 55 m/s and **false** at 82.5 and above. Closest approach
falls monotonically from 7 948 m to 16 m as the raid crosses the 8.6 km to the defended cluster
inside a fixed 60 s window; somewhere between 55 and 82.5 m/s it first reaches the guns' envelope.
That is the trend, and `adds` tracks it because the gun rounds *are* the extra adds.

**And it is not monotone at the top, which the report does not hide.** 165 m/s produces more
engagement than 220: a raid fast enough to overfly is engaged less, not more. A sweep that
reported only "higher is more" would be reporting a line that is not there.

---

## 2. What was built

| Component | Owns | Requirement |
|---|---|---|
| `src/param/Axis` | The axis: its model, its campaign-file parser, its sweep order. **Links nothing** — not even the run path | CR-PAR-1 |
| `n8ro-campaign` (extended) | `--campaign`, the hook built from a declared value, the gate at a declared value, the `axis` and `sweep` objects in `campaign.json`, and the printed sweep table | CR-PAR-1, CR-PAR-2 |
| `src/run/RunOnce` + `RunRecord` | The parameter in every run record, and the check that every named entity actually appeared | CR-PAR-1 |
| `tests/parameter_test` | 126 checks | CR-PAR-1, CR-PAR-2 |
| `examples/atacama-raid-speed.json` | The committed example sweep | CR-PAR-2 |
| `tools/spike-oq4`, `tools/m5-checks` | The OQ-4 decision's evidence. Spikes, not product | OQ-4 |

*And one repair.* `tools/spike-axis/build.cmd` had not linked since M3 gave `RunOnce` a capture
read-back; nothing depended on it, so nothing noticed for two milestones. Fixed, and recorded as
**F-25** — the finding worth keeping is not the missing source lines but that **a build script
outside the main path rots invisibly**, and this one was found only because M5 wrote a sibling
and ran both.

**`src/param/` links nothing, and that is the point.** An axis is a declaration; turning one
into bus traffic is `n8ro-campaign`'s job. So the **whole** of CR-PAR-1's configuration surface —
what parses, what is refused and by what name, that a value's declared text survives, that the
sweep is ordered by value — is in `tests/parameter_test.cpp`, which needs no N8RO install. The
only part needing a simulator is whether the platform honours a swept value, and that is
measured in `docs/m5-oq4.md` rather than asserted anywhere.

**It acts through `RunConfig::afterLoadBeforeStart`, the seam M2 built.** There is no second
mechanism, deliberately: a parameter applied anywhere else is a parameter the run record does not
know about.

---

## 3. Four decisions inside CR-PAR-1, and each could have gone wrong quietly

### The declared text of a value is authoritative; the double is derived

`27.5` reaches `run.json` and the report as `27.5`. The double publishes the value and orders
the sweep, and is never printed. This is M4's rule — *the comparison never converts a number for
a decision* — applied to the parameter, and it is tested the same way: the whole of
`parameter_test` is re-run under `German_Germany.1252` and every answer must be identical.

Consequences that are tested rather than hoped for: `"55.0"` stays `"55.0"` and does not become
`"55"`; `"1e2"` stays `"1e2"`; a value written `"1,5"` is **refused** rather than half-read as 1;
and the gate value is matched on **text**, so `self_test_value: "55"` against a sweep declaring
`"55.0"` is a refusal rather than a silent match.

### Entities are named, never matched

There is no glob, and the refusal says why rather than only that:

```
"RedUAV_N_*" looks like a pattern, and there is no pattern matching here. Resolving one would
mean subscribing the control path to sim/entity/state, which would perturb the publication
schedule the determinism gate measures. Name the entities
```

Instead, **the run's own capture answers whether the entity was there.** `sendEntityUpdate`
returning true means the message reached the bus; the read-back collects which named entities
carried a sample, and `run.json` lists any that did not. A mistyped name is a reported fault
rather than a sweep in which every run is silently the baseline.

### A campaign file inverts `contract/`'s unknown-key rule, on purpose

The capture format says an unrecognised key is **ignored** (§13) and the reader does exactly
that — it is why `format_version` has held across three producer releases. A campaign file gets
the opposite rule: an unknown key, and a key written twice, are both **refused by name**. The
difference is who wrote the file. A producer adds keys and an old reader must survive them; a
person writes a campaign file, and `"value"` for `"values"` is a typo that would otherwise be a
sweep that silently did not happen. A key beginning with `_` is a comment, because JSON has none
and a configuration file needs them.

**This was found by a test, not by reading the code** — see F-23. The first version resolved a
duplicated key first-wins, inherited from the JSON parser's correct-for-captures behaviour.

### The gate runs at one declared value, and says what that establishes

CR-DET-1 says *"the same configuration twice"*; a sweep has seven. `self_test_value` picks
which, defaulting to the first value written, and **must be one of the values the campaign
sweeps** — a gate on a configuration nobody runs is the "checked once, elsewhere" shape CR-DET-1
exists to prevent. Both gate runs are copies of one `RunConfig`, so *"two runs at the same
parameter value"* is guaranteed by construction rather than arranged.

Every report says what that does and does not establish:

```
  The determinism gate ran at red_raid_speed_ms = 55 and established determinism FOR THAT VALUE.
  It is one claim at a named point, not one per run.
```

`self-test.json` carries the same machine-readably, as `parameter.establishes`.

**And two runs at different values are never compared.** Measured, as a negative control: two
runs 0.1 m/s apart, which is physically negligible and configurationally total, correctly **fail**
the content gate with 33 546 differing samples and the verdict *"This is not the publication
schedule; it is the simulation."* Nothing in the campaign can hand the comparison such a pair —
the only comparison is the self-test's, over two copies of one configuration.

---

## 4. CR-PAR-1's third criterion, discharged by demonstration

*"Two runs with the same parameter value are identical configurations, and are therefore valid
inputs to CR-DET-1's self-test."* That was a claim, and nothing had run it. Four runs at exactly
110 m/s, all six pairings, through `n8ro-compare`:

| | |
|---|---:|
| pairs compared | **6 of 6 pass** |
| samples compared | **293 576** |
| differing | **0** |
| worst coverage | **99.6761%** (floor 99%) |
| segment 0 clock | `running` in all four |
| `(entity, occupancy)` keys | **61** in all four |

Detail in `docs/m5-oq4.md` §7, including the one number worth watching: the worst coverage here
is 99.6761% where the worst of M4's 190 unparameterised pairs was 99.8513%. Still comfortably
inside the floor, at a smaller margin.

---

## 5. What the milestone found in its own code, and it took running the thing

**F-24: a run with no result reported it as `0`, and the table plotted it.**

The committed sweep's first execution hit R14 twice in seven runs — segment 0 classified
`frozen`, so there was **no running segment to measure in**. The counts stayed at their
initialised zero, and the table printed:

```
  11     000   completed                    0        0     50607  .
  27.5   001   completed                    0        0     50550  .
  55     002   completed                   47       47     50390  ############################
```

Three things wrong at once, and the third is the worst. The zero is a **missing** measurement
shown as a measurement of zero — tenet 3, turned on our own report. It was drawn as a bar, so it
sits on the trend as a point. And it became **the bar scale's minimum**, which made every other
bar in the table wrong as well: 47 against a floor of 0 draws as 28 cells where against the real
floor it draws as one.

Fixed: a run with no running segment prints `-`, is excluded from the bar *and from its scale*,
and the table says how many points the sweep is short. `run.json` and `campaign.json` write
`null` rather than `0`.

**Unit tests would not have found this.** The axis parser is 126 checks and none of them touches
the report; what found it was running the committed sweep on real runs and reading the output.
That is the same lesson M3's §5 drew from its own three defects, and the reason the sweep is run
rather than only built.

### The two executions, and why both are kept

The sweep was run, F-24 was found in what it printed, the reporting was fixed, and it was run
again. The second execution froze nothing, so the committed table has seven measured points
where the first had five.

**Both are kept** — `campaigns/m5-sweep-first/` and `campaigns/m5-sweep/` — and both are counted
in the frozen-segment rate below. The re-run was because the tool changed, not because the
numbers were unwelcome, and reporting only the clean execution would be choosing evidence.

*One label in that evidence is stale and its content is not.* `campaign.json` and `run.json`
were bumped to `ext17-campaign-summary/3` and `ext17-run-record/2` **after** the second
execution, so the committed evidence carries the previous version strings. The bump was a
version label on a shape that was already what it describes; nothing in either file differs. It
was not re-run a third time for a string, because re-running the campaign re-rolls §6's dice and
this project does not do that to improve an artifact.

---

## 6. The cost the chosen axis carries, measured across every parameterised run

The axis acts before `start`, so it can land between two publications of the start-up roster
burst — making segment 0 `frozen` with repeated values that **differ**. That is a *third* shape
through §5.1's single test, and the only one this project causes.

| batch | frozen segment 0 |
|---|---:|
| the OQ-4 sweep, 7 values | **2 of 7** |
| the 110.x sensitivity batch, 6 runs | 0 of 6 |
| the same-value pair batch, 4 runs | 0 of 4 |
| the committed sweep, first execution, 9 runs | **2 of 9** |
| the committed sweep, re-executed, 9 runs | 0 of 9 |
| **every parameterised run** | **4 of 35 — 11.4%** |
| R12's baseline for ordinary runs (M4, F-13) | 1 of 27 — 3.7% |

Reproduced independently on the committed sweep's own frozen runs: 23 and 30 duplicated
instants, **12 identical in both** — the Blue entities, which nothing updated — and 11 and 18
differing, the raiders which were. The 12 is the untouched roster showing through in every case.

**Elevated, consistent across two batches, and reported as a direction rather than as a number.**
The exclusion is not relaxed, **no retry is added**, and this strengthens E-4 rather than opening
a new escalation: §5.1 presenting one test as detecting one phenomenon is the same finding, now
with three instances.

**A mitigation exists and was deliberately not taken.** Applying the parameter after `start` at
frame ≥ 1 avoids the roster burst entirely — M2's `p2b` measured a mid-run update taking effect
on the next frame and persisting. It was declined because the axis would then be *"state at
frame 1"* and [B]'s axis is *"initial positions and velocities"*. That is a trade recorded for
whoever revisits it, not a door left closed by accident.

---

## 7. What M5 did *not* do

- **It did not judge anything.** The sweep's result columns are counts read off captures. No
  condition is declared until M6, so no run here is a pass or a fail, and every place the table
  appears says so. **CR-PAR-2's third criterion is therefore half-met and recorded as half-met**:
  a *result* changes across the range, demonstrably and for a nameable reason; a *verdict* cannot,
  because none exists yet. M6 attaches at the `sweep` array in `campaign.json`.
- **It did not close OQ-2.** The gate basis is still a flag and a ruling still changes a default
  and no code. Re-checked at M5 against EXT-08's own escalation record: still *"awaiting a
  reply"*.
- **It did not enforce the fidelity ceiling in code.** 400 m/s belongs to this scenario's entity
  profiles, not to the campaign runner; hard-coding it would be this project asserting something
  about scenarios it has never loaded. The committed example stops at 220 and says why in the
  file itself (R13).
- **It did not verify a declared direction against the scenario's authored one.** The campaign
  file states a heading per entity; if it says south where the scenario authored east, the sweep
  is real, deterministic and about a different question than the author meant. What *is* checked
  is that every named entity carried a sample.
- **It did not implement a second axis kind.** `velocity_ned_scaled` is the one; any other
  spelling is a named refusal that says so. [B] settles the count at one.
- **It did not re-judge a stored run** (CR-CAP-1's second half, M6 with CR-AS-3) or widen ADR-1's
  one-machine scope.
