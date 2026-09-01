# EXT-17 — Headless Campaign Runner

Runs many unattended N8RO simulation runs, varies one input across them, judges each against
conditions declared outside the code, and reports across the campaign.

**Status: milestone 7 of 7 — built, and one deliverable short.** One run, automated, with an
explicit end and a bounded timeout; a conformant reader for the capture format that **links
nothing at all**; a determinism self-test that runs at the start of every campaign and stops it
if it does not pass; one parameterisation axis declared in a file and swept across a campaign;
conditions declared outside the code and judged into three-valued verdicts; per-run records and a
campaign summary, both machine-readable and both legible; a re-judge mode over stored captures; a
run-to-run diff for both of the questions the brief asks of one; and the four ugly realities
injected deliberately and survived.

> **One deliverable is outstanding: the 5-minute recording.** It needs a person, it is scripted
> beat by beat in `docs/recording-script.md`, and it is **not delivered**. Everything else the
> brief asks for is here. See
> [the deliverables](#the-deliverables-the-brief-asks-for-and-their-status).
>
> **Two open questions were DECIDED on 2026-09-01, and neither was ANSWERED.** The DRI authorised
> deciding OQ-2 (which comparison the gate reads) and OQ-3 (the host invocation) from the brief's
> own words, because schedule became binding and neither recipient had replied in five
> milestones. **`decided` is not `answered`** — this project keeps the two apart in every artifact
> that carries them, down to `self-test.json`'s `oq2_answered_by_brief_author: false` — and
> [`docs/m7-oq2-oq3.md`](docs/m7-oq2-oq3.md) is the reading. **Neither changed any code.**

> **The determinism gate passes on content, and that is this project's decision rather than the
> client's.** The brief asks for two identical runs to *"produce identical captures"*; measured
> here across 190 pairs, they agree on every sample present in both at the same simulation
> instant and are **never** byte-identical. Both comparisons are run and reported on every
> self-test, and which one *decides* was **decided by the DRI on 2026-09-01 from the brief's own
> words — and was never answered by the brief's author, who has not replied** (OQ-2). Those are
> different words and this project keeps them apart everywhere. See
> [Proving determinism](#proving-determinism--the-self-test-and-the-gate) and
> [`docs/m7-oq2-oq3.md`](docs/m7-oq2-oq3.md).

The binding contract is `docs/prd.md`, which is itself written against the client brief. The
capture format EXT-17 consumes is vendored, read-only, in `contract/`.

### The four things the brief asks this README to cover

| | |
|---|---|
| *"how to configure a campaign"* | **[Configuring a campaign](#configuring-a-campaign--and-running-one)**, and [Sweeping one parameter](#sweeping-one-parameter--the-axis-and-where-the-trend-is) for the axis |
| *"how to write an assertion"* | **[Writing an assertion](#writing-an-assertion--the-condition-file-and-what-a-verdict-is-entitled-to-say)** |
| *"the output format"* | **[The output format](#the-output-format--what-a-run-and-a-campaign-produce)** |
| *"the limits"* | **[Limits](#limits--what-a-result-here-does-and-does-not-prove)** — the section to read first if you are reviewing a result |

Three more that are not on that list and are worth the detour: [the stop
predicate](#the-stop-predicate--what-the-run-is-finished-means), because *"is it finished"* is
harder than it sounds and the answer here is a decision rather than a default; [proving
determinism](#proving-determinism--the-self-test-and-the-gate), because the campaign refuses to
run a single run until it passes; and [the four outcomes](#the-four-outcomes), because two of
them are not test results.

**And two documents beside this one.** `docs/determinism-notes.md` is the brief's fifth
deliverable — what had to be done to make comparison meaningful, and **the things that could not
be explained**. `docs/decisions-m6-m7.md` records every decision taken without asking, with what
each would cost to reverse.

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

## Proving determinism — the self-test and the gate

> **A run is compared to another run of the same configuration by per-`(entity, occupancy)` value
> sequences, aligned on `sim_time_s`, over running segments only. A sample present in one run and
> absent from the other is not a difference.**

This is CR-DET-1 and it is [B]'s step 4, which the brief makes a hard stop: *"Do not build
further until it passes."* So `n8ro-campaign repeat` **runs it before its first campaign run**,
and a campaign whose gate does not pass executes no campaign run at all. It is not a command
anyone has to remember.

### The two comparisons, and why both always run

| | |
|---|---|
| **content** | per `(entity, occupancy)` value sequences aligned on `sim_time_s`, **running segments only**. Values are compared as the verbatim text the capture carried, never as reformatted numbers |
| **bytes** | byte for byte, with `platform.model_path` excluded — the one field the format names as legitimately host-dependent (§14), and the only exclusion there will ever be. **Expected to fail here, and never engineered to pass** |

**Which of the two decides the gate is OQ-2, and it is DECIDED — content — and was never
ANSWERED.**
Decided by the DRI on 2026-09-01 from the brief's own words, because schedule became the binding
constraint and the brief's author had not replied in five milestones. The reading is
[`docs/m7-oq2-oq3.md`](docs/m7-oq2-oq3.md) §1, and **the sentence it turns on is not the one
usually quoted** — it is the brief's statement of what the self-test is *for*:

> *"If it ever fails, you have found either a defect in your harness or something far more
> interesting, and you must be able to tell which."*

A byte gate fails **190 times in 190** here, identically whether the harness is clean or broken,
so it distinguishes neither case — it defeats the purpose the brief states for the very test its
acceptance criterion 2 is about. The content gate passed 190 of 190 on a clean harness **and has
failed on a real pair for a real reason** (`campaigns/m6-gate-refused/`), which is the brief's
*"defect in your harness"* case caught and named.

**The mentor concurred with this reading on 2026-09-01**, independently and without having seen
it. **That is a second opinion and not a ruling.** Acceptance criterion 2 belongs to the brief's
author, who has still never replied, so the concurrence raises confidence in the decision and
closes nothing — and every claim of criterion 2 still carries "under the content reading".

**No code behaviour changed when it was decided, or when it was concurred with.** `--gate-basis
content|bytes` still selects it; `content` was already the default and is ADR-1's decision. Under
`bytes` the gate correctly fails on this platform and the campaign correctly stops, and that has
been run (`campaigns/m4-bytes/`). **A ruling from the brief's author would still change a default
and no code**, and would still be acted on.

Every report says all three things, on every run:

```
  gate basis            content   (ADR-1: the content basis is THIS PROJECT'S decision, not the client's)
  OQ-2                  DECIDED (DRI, 2026-09-01) - content. Decided from [B]'s own words and
                        CONCURRED with by the mentor on the same day, independently. Still NOT
                        answered by [B]'s author, who has not replied - and criterion 2 is
                        theirs to discharge, so a second opinion strengthens the decision
                        without closing the question. ...
  GATE                  PASS   on the content basis
                        This does NOT discharge [B]'s acceptance criterion 2 as written.
```

`self-test.json` carries the same three facts as three separate keys — `oq2_decided_by`,
`oq2_concurred_by`, and `oq2_answered_by_brief_author`, which is still `false` — so a consumer
cannot compute the last from the others. Four checks in `tests/determinism_test.cpp` hold the
distinction together; two of them exist specifically to stop the concurrence ever appearing
without the caveat.

### Measured here, not inherited

Over **all 190 pairs** of M2's twenty runs of one configuration, through `n8ro-compare`:

| | |
|---|---:|
| samples compared | **9 573 667** |
| agreeing | **9 573 667** |
| differing | **0** |
| pairs passing the content gate | **190 of 190** |
| pairs byte-identical | **0 of 190** |
| worst coverage | 99.8513% |

The inherited figure was 50 358 of 50 358 on one pair. This reproduces it and exceeds it by a
factor of 190. **The simulation is reproducible; its publication schedule is not.**

### A sample in one run and not the other is the expected case

Two runs of one configuration never have the same number of samples — M2 measured seventeen
distinct counts across twenty runs. Such a sample is counted as `only_in_A` / `only_in_B` and
reported **inside the verdict**, never as a difference and never as a failure. The verdict says
what was actually established:

> every sample present in **both** runs at the same simulation instant agrees. This is not a claim
> that the two captures are identical.

**But the intersection is bounded**, because "they all agreed" over three samples is a wrong
number. The comparison carries a **coverage floor** — the intersection must reach 99% of the
smaller run's comparable samples, or the verdict is `indeterminate` rather than `pass`. The floor
is measured: the worst of those 190 pairs was 99.8513%.

### Four verdicts, and what each does

| | means | the campaign |
|---|---|---|
| `pass` | every compared sample agreed, and enough were compared | runs |
| `fail` | a compared sample disagreed, **or** an `(entity, occupancy)` is in one run and not the other | **stops, exit 3** |
| `indeterminate` | the comparison ran and cannot support a verdict | **stops, exit 3** |
| `refused` | a precondition was not met, and the refusal names which of twelve | **stops, exit 3** |

Exit `3` is its own code deliberately: collapsing [B]'s hard gate into exit 1 alongside an
ordinary failing run is exactly the collapse the four-outcome rule forbids. `failed` and
`infrastructure_error` are kept apart inside it — a self-test run that did not complete has
established *nothing* about determinism and is not a determinism failure. `self-test.json` says
which.

**There is no `--skip-self-test`.** A skip flag is how a self-test becomes something everybody
skips. The `self-test` command exists for running the gate alone; `repeat` does not consult it.

The self-test's two runs land in `<out-dir>/selftest/runs/000` and `001`. **They are not campaign
runs** and are never counted among the campaign's outcomes — a `--count 2` campaign reports
`attempted: 2`, not 4 — but they *are* counted in the disk pre-flight, because a projection that
ignored them would be projecting a campaign nobody runs. They cost two runs, about two minutes
and 60 MB, on every campaign.

### Comparing two stored captures

```
n8ro-compare <capture-a> <capture-b> [--gate-basis content|bytes] [--coverage-floor <pct>]
```

Exit `0` if the gate passed, `1` if it failed or was indeterminate, `2` if the comparison was
refused. This is how the 190-pair table above was produced, and it needs no simulator.

### Nothing of ours varies between runs, checked twice over

[B] names three hazards — *"a timestamp in the compared output, an unordered container iterated,
a value read from a clock"* — and this project adds a fourth, the comma-decimal locale this
machine actually has. Each is closed **by design**, **by a build-time search that fails the build
on a hit**, and **by a behavioural test**:

- the same comparison run twice produces **byte-identical report text**;
- two captures whose entities appear in the **opposite order** produce byte-identical report text;
- the whole report is regenerated under **`German_Germany.1252`** and must be byte-identical.

Neither the search nor the test subsumes the other: the search catches a reintroduction on a path
no test exercises, the test catches one the search does not know the spelling of.

The comparison **never converts a number for comparison**. Runs are aligned on the *verbatim
text* of `sim_time_s`, and values are digested from their original text. The format writes doubles
in shortest round-trip form (§8.3), so equal text and equal double are the same relation — which
puts the locale hazard off the path that decides the gate rather than testing it back off.

Detail, and every number, in [`docs/m4-determinism.md`](docs/m4-determinism.md).

---

## Building

Requires Visual Studio's x64 toolchain and the N8RO SDK at `C:\N8RO`. A run that **records**
additionally needs EXT-08's recorder binary, which is **not built here** — see *"The fourth
binary is not in this repository"* below, after the build scripts.

```
tools\n8ro-campaign\build.cmd      ->  build\n8ro-campaign\n8ro-campaign.exe
tools\n8ro-capture\build.cmd       ->  build\n8ro-capture\n8ro-capture.exe
tools\n8ro-compare\build.cmd       ->  build\n8ro-compare\n8ro-compare.exe
tools\n8ro-judge\build.cmd         ->  build\n8ro-judge\n8ro-judge.exe
tests\build.cmd                    ->  builds and runs the tests
```

`tests\build.cmd` links nothing and needs no N8RO install — it covers the parts whose
correctness is about our own output rather than about the platform; since M3 the capture reader,
whose correctness is about somebody else's bytes; since M4 the determinism comparison, whose
correctness is what every other result rests on; since M5 the axis; and since M6 the whole
assertion surface. **72 + 105 + 126 + 166 = 469 checks** across five suites, of which the last
two are each run twice, the second time under a comma-decimal locale.

**That number is not typed here on trust.** `tests\build.cmd` sums what the suites actually
printed and fails if the total is not the one in `tests\checks.golden.txt` — the same mechanism
as the four golden `--help` files, and for the same reason. It is here because this figure
*did* rot: it read `466` for a milestone after the determinism suite grew from 96 checks to 105,
which is the third counting-drift finding in this project's own log (F-41). Growing the suite
means updating the golden file and this sentence in the commit that grew it.

**469 is the MANDATORY total, and that distinction was itself a finding (F-44).** The capture
reader's tier 4 reads the real producer-0.9.0 captures under `campaigns\m2-oq1\runs`, which are
untracked and 569 MB — so it runs on the machine that has them and is skipped everywhere else,
and it contributes **6 further checks** there. The count was therefore a property of the machine:
475 on the development machine, 469 on a clean runner, zero failures both times. **A golden that
only holds in one place is the same defect the golden was added to prevent**, and it was found
the first time this repository was built anywhere else — by the CI job below, on its first run.
The suite now prints its optional count separately and `tests\build.cmd` subtracts it, so the
pinned number is one every machine reproduces. The optional checks still run wherever the
captures are, and a failure in one still fails the suite; only the count is held apart.

### CI — the same tier, on a machine nobody here owns

`.github/workflows/zero-install-tier.yml` runs all of the above on a stock `windows-latest`
runner: the 469 checks, the three SDK-free tool builds with their boundary searches and golden
`--help` comparisons, and then a read of EXT-08's capture and a load of EXT-08's condition file
with neither EXT-08 nor the SDK present. **Its first step asserts the runner has no `C:\N8RO` and
no `N8RO_RELEASE`, and fails the job otherwise**, because none of the rest means anything on a
machine that has the install.

**This is the only claim here that is not self-certified.** Every other proof of the boundary is
a search this project wrote, run over sources this project wrote, on the one machine that has the
SDK — which can show that no forbidden name appears, and cannot show the install is unnecessary,
because the install was always there. The four SDK-linked targets — `n8ro-campaign`, `m1-run` and
the two spikes — cannot build there, and a recording run additionally needs EXT-08's recorder.
That is half the build scripts, and the job runs the half it can rather than weakening the half
it cannot.

**Three of the four tools need no N8RO install to build or to run, and that is the point.** Look
at the compile lines of `n8ro-capture`, `n8ro-compare` and `n8ro-judge`: no `/I`, no `/LIBPATH`,
no `.lib`. Neither the reader, nor the comparison, nor the condition evaluator links EXT-08 or
the SDK, and a build script anyone can read in ten seconds is a better proof of that than an
argument about translation units. Each build then searches its own sources for the SDK's and the
producer's names, and for any global sort of a capture, and fails on a hit.

**`n8ro-compare`'s carries two searches more** — for a clock read or a formatted time, and for an
unordered container or a locale-dependent number conversion — because those are CR-DET-2's
hazards and they are properties of the comparison's own sources rather than of whatever links
them.

**`n8ro-judge`'s carries those and one beyond them:** it fails the build if the assertion path
ever names a process, a bus or the control path. The compile line already makes a host
unreachable, since none of those symbols could resolve; the search is the second lock, and it
fires a milestone before the link would notice. That is CR-CAP-1's *"no host started and no bus
subscription made"* made structural rather than promised.

Each build ends by running its binary's own `--help` and comparing it against the golden file
beside it — `help.golden.txt`, one in each of the four tool directories. A drift fails the build. This is deliberate: the PRD does not enumerate the option list in prose, because
a list nobody executes is exactly what drifted in the sibling project. **The golden file is the
CLI's specification.**

## The fourth binary is not in this repository — `--recorder`

**Building all four tools does not give you everything a recording run needs.** `n8ro-campaign`
starts a *recorder* as a child process and passes it `--recorder <path>`; that binary is
**EXT-08's `n8ro-bridge.exe`**, and it is required unless you pass `--no-recorder`. Nothing here
builds it, and nothing here can: EXT-08 and EXT-17 are separate repositories with no shared
source, which is the rule this project is organised around.

| | |
|---|---|
| What it is | EXT-08's capture recorder, built from that repository — typically `…\EXT-08\build\x64\Release\n8ro-bridge.exe`. The committed campaigns record the exact path they used in each run's `run.json` under `environment.recorder_exe` |
| What it does here | It holds the bus subscription and writes the `n8ro-capture/1` file. **EXT-17 never subscribes to entity state itself** — that is why `src/control/` publishes on the control path only, and why resolving an entity glob is refused: a second subscriber would perturb the publication schedule the determinism gate measures |
| Which commands need it | `run-once`, `repeat` and `self-test`. **`report` and every `n8ro-judge` command do not** — they read stored files, start nothing, and need no N8RO install either |
| If you do not have it | `--no-recorder` still executes and still writes `run.json`; there is simply no capture, so nothing can be judged, compared or swept |

**What crosses the boundary is a process and a documented file format, never a symbol.** The
format is vendored in `contract/capture-format-v1.md` and this project implements it from that
document alone — `tools\n8ro-capture\build.cmd` is the proof, and it is why a capture recorded by
somebody else's conformant producer reads here just as well.

**[B]'s surface table cites `include\n8ro-sim\infrastructure\EntityStateSample.h` as the answer
to "what a run publishes". That header does not exist in release 2.1.328** — that directory holds
exactly two files, `SimulationEngineClient.h` and `SimulationEngineHost.h`, and a tree-wide search
finds nothing under any other name. It is recorded as **F-20**, a defect in the brief rather than
a gap here, and the capture format is what stands in its place.

## Two environment preconditions, both measured

Neither is optional and neither covers the other.

| | Why |
|---|---|
| `N8RO_RELEASE=C:\N8RO` | Without it the host resolves its plugin directory from its working directory, skips the plugin scan, never registers `componentPhysics`, and **refuses every 42-entity scenario load while sitting idle rather than failing** |
| `C:\N8RO\bin` on `PATH` | Where `n8ro-sim.dll` and `n8ro-core.dll` resolve from. Without it an SDK-linked binary exits 53 having produced no output — it looks like a crash and is a missing DLL |

`n8ro-campaign` sets both for the processes it starts (`--n8ro-release`, `--path-prepend`). It
needs `PATH` for itself, because it links the SDK too.

## Configuring a campaign — and running one

```
set PATH=C:\N8RO\bin;%PATH%

n8ro-campaign run-once ^
    --out-dir campaigns\demo ^
    --recorder <path-to-n8ro-bridge.exe> ^
    --scenario "Atacama Air Defense" ^
    --frames 1200

n8ro-campaign repeat --count 20 --out-dir campaigns\twenty --recorder <...> --frames 1200

n8ro-campaign repeat ^
    --out-dir campaigns\sweep ^
    --campaign examples\atacama-raid-speed.json ^
    --recorder <...> ^
    --frames 1200

rem The committed twenty-run campaign, judged. One command, no manual step.
n8ro-campaign repeat ^
    --out-dir campaigns\m6-campaign ^
    --campaign examples\atacama-raid-speed-20.json ^
    --conditions examples\atacama-raid.conditions.json ^
    --recorder <...>

rem Re-read a stored campaign's report. Starts nothing; needs no N8RO install.
n8ro-campaign report --out-dir campaigns\m6-campaign ^
    --campaign examples\atacama-raid-speed-20.json

rem Re-judge stored captures against new conditions, and check the identity.
n8ro-judge campaign campaigns\m6-campaign ^
    --conditions <new-conditions> --write verdicts-new.jsonl

rem Change one input, and show exactly where the two runs diverged.
n8ro-compare --changed-input ^
    campaigns\m6-campaign\runs\000\capture-atacama-air-defense-000.n8rocap.jsonl ^
    campaigns\m6-campaign\runs\019\capture-atacama-air-defense-019.n8rocap.jsonl
```

`n8ro-campaign --help` is the authority for the options, and the same is true of the other three
binaries — each build compares its binary's own `--help` against the golden file beside it.

**A note on `--run-timeout-ms`, because the default is generous.** A 1200-frame run takes about
70 s here, and the default backstop is 600 000 ms. That costs nothing on a healthy run and ten
minutes on a host that dies mid-run, which is not detected until the timeout expires (F-27). Size
it against the frame budget for an unattended campaign.

## Sweeping one parameter — the axis, and where the trend is

**One axis, declared in a file, and no rebuild to change it.** [B]: *"One axis done properly
beats four done loosely."* Which axis was OQ-4, and it is decided — **initial positions and
velocities**, as one declared scalar applied to named entities before `start`. The measurement
that decided it, against all three of [B]'s candidates, is `docs/m5-oq4.md`.

`examples/atacama-raid-speed.json` is the committed example: the closing speed of the Red raid
in `Atacama Air Defense`, swept from 11 to 220 m/s.

```json
{
  "axis": {
    "name": "red_raid_speed_ms",
    "kind": "velocity_ned_scaled",
    "units": "m/s",
    "entity_groups": [
      { "direction_ned": [-1, 0, 0], "names": ["RedUAV_N_01", "RedUAV_N_02"] },
      { "direction_ned": [0, -1, 0], "names": ["RedUAV_E_01"] }
    ],
    "values": ["11", "27.5", "55", "82.5", "110", "165", "220"],
    "self_test_value": "55"
  }
}
```

The number of runs **is** the number of values; `--count` is refused alongside `--campaign`,
because two statements of how many runs there are is one statement too many. Runs execute in
sweep order, so a run's ordinal ascends with its value.

### Five things about it that are decisions, not details

**The value is carried as the text you wrote.** `27.5` reaches `run.json` and the report as
`27.5`. The double derived from it exists to publish the value and to order the sweep, and is
never printed. M4 closed CR-DET-2's locale hazard by never converting a number for a decision,
and a re-formatted double in a report would put it straight back on a path the build searches
for. A value written `1,5` is refused rather than half-read.

**Entities are named, never matched.** There is no glob. Resolving one would mean subscribing
the control path to `sim/entity/state` — which would perturb the publication schedule the
determinism gate measures — and a pattern that silently matches nothing is the failure this
project keeps finding. An entity the axis names that carries **no sample in the run's own
capture** is listed in that run's record: publishing an update says the message reached the bus
and nothing about whether anything was there to receive it.

**The gate runs at one declared value, and establishes determinism for that value.** CR-DET-1
says *"the same configuration twice"*; a sweep has many. `self_test_value` picks which, and
defaults to the first value written. Both gate runs are copies of one configuration, which is
what makes them a valid pair rather than an arrangement — and **two runs at different values are
never compared**: they are two configurations, and a gate over them would report a difference
meaning only that the sweep worked. Measured while deciding OQ-4: four runs at one value, all
six pairings, **293 576 samples compared, zero differing**.

**A campaign file is not a capture, and the unknown-key rule is inverted.** The capture format
says an unrecognised key is *ignored* (§13), and the reader does exactly that — it is why the
format version has held across three producer releases. Here a key the format does not know is
**refused**, because a person wrote this file and `"value"` for `"values"` is a typo that would
otherwise be a sweep that silently did not happen. A key beginning with `_` is a comment.

**The axis has a measured range, and the example stops inside it.** The platform honours an
injected speed exactly up to **400 m/s** and clamps above it, walking the entity down at
20 m/s². At 900 m/s a run spends 25 s — 42% of it — off parameter. So the sweep stops at 220,
and `docs/m5-oq4.md` §3 carries the measurement.

### Where the trend is

In the campaign log and in `campaign.json`, ordered by parameter value, with no other tool:

```
[campaign] sweep        SWEEP  red_raid_speed_ms  (m/s)  velocityNed magnitude, set before start  -  7 run(s), ordered by value
[campaign] sweep
[campaign] sweep          value  run   outcome                   adds     keys   samples
[campaign] sweep          11     000   completed                   90       48     50614  .
[campaign] sweep          27.5   001   completed                   90       48     50511  .
...
```

The bar is scaled between the **minimum and maximum** of the column rather than from zero, and
the report says so — a column running 89 to 104 drawn from zero is five identical bars.

**A run with no result shows `-`, never `0`.** A run that did not complete, and a run whose
capture has no `running` segment for the determinism gate's own reason (R12, R14), had nothing
measured in it. Both are excluded from the bar *and from the bar's scale*, and the table says
how many points the sweep is short. This is not a hypothetical nicety: the committed sweep hits
it, and the first version of this table printed those runs as `0`, drew them, and used the zero
as the scale's floor — which made every other bar in the table wrong as well.

**The result columns are counts read off each capture, not verdicts.** No condition is declared
until M6, so no run here is a pass or a fail. `adds` is the column to read a trend from: M2
measured the roster lifecycle agreeing *exactly* across twenty identical runs, so unlike
`samples` it carries none of the platform's 0.38% publication-schedule spread. What the
committed sweep shows it doing, and why, is `docs/m5-sweep.md`.

## Reading a capture back

```
n8ro-capture read      <capture-file>       one capture
n8ro-capture read-set  <first-part-file>    a rotated capture, as the set it is
n8ro-capture campaign  <campaign-dir>       every run's capture under <dir>/runs/NNN
```

Exit `0` if every capture read is conformant, `1` if something was found wrong with one, `2` if
one was rejected — unreadable, not a capture, or a `format_version` this reader does not
implement. `n8ro-capture --help` is the authority for the options.

It reads a 24 MB, 50 573-line capture in 0.24 s and a twenty-run campaign in 4.7 s.

`n8ro-campaign` also reads back each capture as it produces it, and writes what it found into
`run.json` — see [What a run produces](#what-a-run-produces).

## The output format — what a run and a campaign produce

```
<out-dir>/
  campaign.log                     the driver's own transcript
  campaign.json                    repeat only: the self-test's result, outcome counts, a row per run
  selftest/                        the determinism gate. NOT campaign runs
    self-test.json                 the ext17-self-test/1 record: both comparisons, and OQ-2's status
    runs/000  runs/001             the two runs it compared
  runs/
    000/
      run.json                     the per-run record
      verdicts.jsonl               one verdict per declared condition, one JSON object per
                                   line. Written when --conditions is given
      capture-<scenario>-000.n8rocap.jsonl
                                   one file per run under --on-size-limit stop; under
                                   rotate, also .partNNN siblings, listed in run.json
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

Since M3 it also carries what this project's own reader made of the capture, read back
immediately, on the run that produced it:

```json
"capture": {
  "parts": ["capture-atacama-air-defense-000.n8rocap.jsonl"],
  "end_reason": "host_lost",
  "covers_whole_run": true,
  "conformant": true,
  "samples": 50401, "segment_keys": 2, "run_segments": 2
}
```

**`covers_whole_run` is the one to read.** `false` means the recorder stopped at its byte bound
before the run did: the capture is complete and valid and does **not** cover the whole run.
Anything computed from it is over the part of the run that was recorded. Measured at M3 — a
1200-frame run bounded at 8 MB recorded to `sim_time_s` 19.5 of 60.0, conformantly, and nothing
outside the file said so until this field existed.

**A run that was asked to record and produced no capture is an `infrastructure_error`**, not a
completed run. It reached its stop predicate and it recorded nothing, so there is nothing to
judge or compare later, and reporting it as completed would be reporting a number that is wrong.

**A non-zero host exit code is normal.** The engine's command vocabulary is closed at
`start` / `stop` / `pause` / `step` — there is no shutdown command, so the host is ended with a
console control event, and Windows reports `3221225786` (`0xC000013A`) for that. The host still
shuts down in an orderly way. `terminated_by: "handle"` in the record is the case to look at.

### The judgement, and the two vocabularies it keeps apart

With `--conditions` given, `run.json` gains a `judgement` object and the run directory gains a
`verdicts.jsonl`:

```json
"judgement": {
  "conditions_file": "examples\\atacama-raid.conditions.json",
  "conditions_declared": 7,
  "judged_this_run": true,
  "not_judged_reason": null,
  "judgeable": true,
  "verdicts":   { "met": 1, "not_met": 5, "indeterminate": 1,
                  "indeterminate_is_a_verdict_state_not_a_run_outcome": true },
  "assertions": { "satisfied": 3, "violated": 3, "undetermined": 1,
                  "only_a_violation_makes_a_run_fail": true },
  "verdicts_file": "verdicts.jsonl",
  "one_verdict_per_declared_condition": true
}
```

**`verdicts` is what the run did; `assertions` is whether that was what was asserted.** They are
counted separately and never merged, because a condition asserting non-occurrence is *satisfied*
by a `not_met` answer. Only a **violation** fails a run.

**`judged_this_run` is the field to read on anything that is not `pass` or `fail`.** A run whose
outcome is `infrastructure_error` or `timeout` is **not judged** — inventing verdicts for a run
the harness broke would turn an infrastructure failure into a test result — so it carries 0
verdicts against 7 declared conditions, with `not_judged_reason` saying why. That count mismatch
is deliberate, and it is the cut-short signal: a reader seeing fewer verdicts than conditions
treats the run as cut short, never as passing.

Each line of `verdicts.jsonl` is one `ext17-verdict/1` object:

```json
{"schema":"ext17-verdict/1","condition_id":"raid-leader-reaches-airfield","kind":"proximity",
 "state":"not_met","expect":"met","outcome":"violated","because":"cleared_by_continuity_bound",
 "segment":{"part":0,"segment":0},
 "entities":[{"entity":"RedUAV_N_01","occupancy":1,"sim_time_s":"59.99999999999873","line":50634},
             {"entity":"BlueBase_Airfield","occupancy":1,"sim_time_s":"59.99999999999873","line":50605}],
 "deciding_sim_time_s":"59.99999999999873",
 "measured_name":"closest_approach_m","measured":"8693.1695",
 "threshold_name":"within_m","threshold":"3000",
 "absence_dependent":true,"bound_applied":true,
 "margin_m":"5693.17","bound_m":"1.20","largest_gap_s":"0.1000",
 "reason":"closest approach 8693.1695 m at sim_time_s 59.99999999999873, against a threshold of 3000 m. The margin of 5693.17 m exceeds the 1.20 m they could have closed inside the largest unobserved window (0.1000 s at a relative 11.0 m/s), so they did not reach it"}
```

**Everything needed to check the verdict by hand is in it**: the entities with their occupancies,
the exact line in the capture, the deciding `sim_time_s` verbatim, the measured value, the
threshold, and — for a bounded not-met — the margin, the bound, and the gap it was computed over.

**The capture path is deliberately not a member.** A live judgement runs against a path inside the
run directory and a re-judgement is handed an absolute one; including it would make
`n8ro-judge --verify`'s byte-for-byte identity check fail on a difference that means nothing. The
run record carries the path; the verdict carries the finding.

### The campaign summary

`campaign.json` is `ext17-campaign-summary/4`. Beyond M5's `axis` and `sweep` it carries:

```json
"outcomes":   { "attempted": 20, "pass": 6, "fail": 12, "timeout": 0,
                "infrastructure_error": 2, "completed_unjudged": 0,
                "sums_to_attempted": true,
                "no_aggregate_merges_two_of_the_four": true },
"conditions": { "file": "...", "declared": 7, "indeterminate_verdicts": 18,
                "indeterminate_is_a_verdict_state_not_a_run_outcome": true }
```

and every entry of `sweep` gains the per-condition verdict at that parameter value — the seam the
sweep's trend is read from, and the half M5 recorded as unmet because no condition existed yet.

## Disk — the ceiling, and what it is measured over

**The ceiling is `8 GiB` over the whole campaign directory** — captures *and* logs — and reaching
it stops the campaign with a named outcome, leaving every completed run valid and readable.
`--disk-ceiling-bytes 0` disables it, deliberately and visibly.

This is CR-CAP-5, and it is still this project's own concern because **the upstream bound is per
capture file**: a per-file bound multiplied by twenty is not a campaign bound.

**The projection is measured, not estimated.** One 1200-frame run costs **29 788 003 bytes**:

| | bytes | |
|---|---:|---|
| the capture | 24 297 928 | 81.6% |
| `host-logger.log` | 2 849 933 | this run's slice of the host's shared log |
| `host.err` | 2 559 540 | the terrain-error flood the install is expected to produce |
| the rest | 80 602 | |
| **total** | **29 788 003** | **24 823 bytes per frame** |

**The ceiling is over the directory rather than over the captures, and that is the point.** A
projection built from capture size alone under-states a campaign by **22.6%**. A campaign that
passed a capture-only pre-flight check could still exhaust the disk on logs, which is the failure
this requirement exists to prevent. Cross-checked against M2's twenty runs: 595 590 013 bytes of
campaign directory against 486 359 759 of captures — a projection from the captures alone
under-states it by 22.5%. The two agree to a tenth of a percent.

At [B]'s scale a 200-second run is 4 000 frames, **~99 MB**, and a twenty-run campaign is
**~1.99 GB**. The 8 GiB ceiling is four times that: large enough that no legitimate campaign meets
it, small enough to catch a runaway early.

Checked **twice**, and the second is not redundant with the first:

- **Before run 1** — the projection (`--bytes-per-frame`, default 25 400) against the ceiling and
  against free space. A refusal names both numbers and creates no run directory at all.
- **After every run** — actual usage against the ceiling. A projection is an estimate, and a run
  that overruns is a run whose estimate was wrong.

**Per run, the upstream bound is `--on-size-limit stop`**, with `--capture-max-bytes` defaulting
to `61 000 × --frames` — three times the measured per-frame capture cost, so the bound exists on
every run and does not fire on a normal one. A run that reaches it has overrun its projection
rather than merely run, and says so in `covers_whole_run`.

`rotate` was **exercised, not merely declined**: a real four-part capture, stitched and read back
conformantly. It works completely. It was not chosen because a rotated run's two segments become
five `(part, segment)` keys, four of which are fragments of one segment that nothing in any file
identifies as such — a cost every per-segment statistic downstream would pay on every run, whether
or not any run ever rotates. The full comparison is in [`docs/m3-oq6.md`](docs/m3-oq6.md). **The
reader supports rotation regardless**, because a capture rotated by somebody else still has to be
readable here.

## The four outcomes

The brief requires four, never collapsed: **pass, fail, timeout, infrastructure error.**

| Outcome | Means |
|---|---|
| `pass` | Every condition the campaign asserted was satisfied |
| `fail` | The capture read, and a declared condition was **violated** — evaluated, and its answer was not the asserted one |
| `timeout` | The run timeout expired before the stop predicate was satisfied. Its own outcome, **never a failure**: a run that did not finish has told you nothing about the scenario |
| `infrastructure_error` | The harness, the host, or the scenario load failed. Never a failing scenario |

A fifth key, `completed`, appears only when **no condition file was declared** — a run nothing
judged is not a pass, and calling it one would undo the whole distinction. It is 0 in any judged
campaign.

**Two things route to `infrastructure_error` that are worth knowing about**, because neither
looks like broken infrastructure at first glance and both are the honest reading of the brief's
*"never let an infrastructure failure count as a test result"*:

- **A capture with no running segment.** Segment 0 can classify `frozen` — see the limits
  section — and `sim_time_s` then does not order its samples, so nothing in the file can be
  judged and the sampling gap a not-met verdict is bounded against cannot be measured. It is not
  a determinism failure and it is certainly not a failing scenario.
- **A run in which nothing at all was decided.** Every verdict indeterminate. Calling that a
  pass is the *"all passed having checked nothing"* failure, turned on the verdicts instead of
  on the loader.

**`indeterminate` is not a fifth outcome.** It is a **verdict** state (see the limits section),
and a run carrying one is reported with its four-state outcome plus the indeterminate verdict
named beside it. Keeping the two vocabularies apart is deliberate, and it is what makes the
brief's acceptance criterion 5 stay exactly satisfied rather than approximately.

Exit code: `0` if every run completed, `1` if any did not, `2` for a usage error before any run
was attempted, and `3` if the determinism self-test did not pass — in which case **no campaign run
was attempted at all**.

`3` is its own code rather than folded into `1`, for the same reason the four outcomes are never
collapsed: "the gate everything rests on did not pass" and "one run of twenty failed" call for
different actions, and an exit code that cannot tell them apart forces the operator to go and
read a log to find out which happened.

## The capture format, and the one thing that is never negotiable

The reader implements **`n8ro-capture/1`** and is written from `contract/capture-format-v1.md`
alone. Two rules govern how it meets a file it was not expecting, and they pull in opposite
directions on purpose:

> **A `format_version` it does not implement is rejected with a named error and no partial parse.**
> **A key it does not recognise, inside a record type it does recognise, is ignored.**

Reject on the **version**; ignore unknown **keys**. That is the entire compatibility mechanism and
it is deliberately blunt: partial parsing of an unknown format is how silently-wrong analysis
happens, and a campaign runner's whole output is analysis.

It is also why the version has held across three producer releases while `contract/` drifted twice
— 0.8.0 added `header.sample_form`, 0.9.0 added four more keys, and neither moved the version. The
conformance suite tests both halves: a version-bumped file is rejected after **one line**, and a
file carrying four keys from a producer that does not exist yet reads identically to the original.

Absence is reported as absence throughout. A missing `sample_form` is *unknown*, never
`"predicted"`; missing `limits` is *unknown*, never "unbounded"; a missing counter is *unknown*,
never zero. "All zeros" and "we were not told" are different claims.

**Every statistic is scoped to a segment, and the segment is named as the pair `(part, segment)`**
— ordinals restart at 0 in every part of a rotated set. An entity's identity is
`(entity, occupancy)` everywhere, never the name: the engine re-creates entities under names it
has already used, both mid-run and at teardown.

**A segment's clock is one of three values, never two.** `running` — the format's exact test fired
and passed. `frozen` — it fired and failed; the segment cannot be aligned against another run at
all. `indeterminate` — **it could not fire**, because no key repeated a `sim_time_s`. That third
value exists because M2 measured segment 1 empty in 15 of 20 runs and this project met the other
shape too: 42 samples, one per entity, all at `sim_time_s` 0.0. A boolean has to call both of
those "running", which is asserting the result of a test that never ran. `frozen` and
`indeterminate` are both excluded from comparison; only `running` is comparable, and it is
comparable *because* the test fired.

**Conformance is checked by 78 checks over four tiers** (`tests\build.cmd`): the vendored fixture
untouched; five mutations and one positive one generated into the build tree — never into
`contract/`, which is read-only; 17 synthetic micro-captures, one rule each; and all twenty of
M2's real producer-0.9.0 captures, read with the same reader, skipped **with a printed message**
when they are absent. Detail in [`docs/m3-capture-reader.md`](docs/m3-capture-reader.md).

## Writing an assertion — the condition file, and what a verdict is entitled to say

Conditions are declared in **their own file**, never in EXT-17's source, and the file is loaded
and validated **before any host is started**. A duplicate id, an unrecognised kind, an unknown
key, a key written twice and an empty condition list are each a distinct named error and a
non-zero exit, so a typo costs ten seconds rather than twenty runs.

```cmd
n8ro-judge check --conditions examples\atacama-raid.conditions.json
n8ro-campaign repeat --out-dir campaigns\mine --campaign examples\atacama-raid-speed-20.json ^
                     --conditions examples\atacama-raid.conditions.json --recorder <path>
```

**The vocabulary is closed at three kinds** — proximity between two entities, presence in a
region, reaching a terminal state. A fourth spelling is a named parse error and never a silently
skipped condition, because a campaign that judged everything except the one that mattered would
report all passed. Adding a fourth kind is a change to the requirements, which is the point.

**Units are the platform's own and are never converted:** metres, degrees, and the platform's
`[lat, lon, alt]` order.

**The file shape is EXT-08's**, adopted at M6 by writing this project's conditions in it and
evaluating them against real captures rather than by reading it — the decision and its
measurements are in [`docs/m6-oq5.md`](docs/m6-oq5.md). **Three rules around it differ, and one
key is added**, each for a reason this project measured:

| | |
|---|---|
| an unknown key, or a key written twice | **refused by name**, the opposite of the capture format's §13 rule. The difference is who wrote the file: a producer adds keys and an old reader must survive them, whereas `"within_meters"` for `"within_m"` is a threshold that silently did not apply. A key beginning with `_` is a comment |
| `"scenario_unload"` as a `removal_reason` | **refused**. It is what the engine's stop path writes for every surviving entity at teardown — measured, 267 of 385 removals across the committed sweep, all at `sim_time_s` 0 — so a condition on it is met in every run for every entity, at a time that points at the wrong end of the run |
| the verdict | **three-valued**, not two. The vendored `met: false` at end of run is a conclusion drawn from absence, and this project does not draw those |
| **`expect`**, added | `"met"` (the default) or `"not_met"`. The vendored schema is a *referee*: it reports whether a condition was satisfied and says nothing about whether that is welcome. Two of the brief's three questions survive being read as "this should hold"; *"did anything reach a terminal state it **should not** have"* does not, and the vendored shape expresses non-occurrence only for the area kind |

The verdict still records the **fact** in the vendored schema's own terms — `met` or `not_met` —
so EXT-08's verdicts and a re-judgement here stay directly comparable. Whether the fact was the
asserted one is a separate field: `satisfied`, `violated`, `undetermined`. **Only a violation
fails a run.**

### An assertion never reads absence as evidence

A capture is a very high-fidelity **sample** of the published stream, not a guaranteed-complete
transcript, and loss has been measured with every platform counter reading zero. So *"no record
says it happened"* is not the same claim as *"it did not happen"*, and a condition that depends
on the difference reports **`indeterminate`** with its reason.

A `met` verdict is always sound — it is computed from records that are **present**. The whole
question is what a not-met verdict may claim, and the classification is **per form**, because
`terminal_state`'s two forms differ completely:

| form | a `not_met` verdict is sound when | what licenses it |
|---|---|---|
| `proximity` | the closest observed approach clears the threshold by more than the pair could have closed inside the largest unobserved window, **and** both tracks are bounded over the segment | continuity over present samples |
| `area` | the same, measured to the region's boundary | continuity over present samples |
| `terminal_state` + `removal_reason` | every occupancy of the entity is closed by a record stating some *other* reason, or carries a sample at the segment's last sampled instant | **the capture format §8.1**, normatively: *"within one `(entity, occupancy)` pair, no `sample` ever appears after that pair's `entity_remove`"* — so a sample is positive evidence of non-removal, and a gap does not weaken it because a re-created entity carries a **higher** occupancy |
| `terminal_state` + `field`+`equals` | **never** | nothing in the format bounds a string field's rate of change, so the value could be taken and left between two samples |

The bound is `(v_a + v_b) · Δt_max + ½ · 20 m/s² · Δt_max²`, and every verdict that used it
carries the margin, the bound and the largest gap, so the claim is checkable rather than
asserted. Measured across the committed sweep: the largest gap is `0.1000 s` — exactly one missed
frame at the platform's 0.05 s period — and the tightest not-met margin clears its bound by a
factor of 64.

**`indeterminate` is a verdict state and never a fifth run outcome.** A run carrying one is
reported with its four-state outcome plus the indeterminate verdict named.

### Re-judging a stored run

```cmd
n8ro-judge campaign campaigns\mine --conditions <new-conditions> --write verdicts-new.jsonl
```

**No host is started and no bus subscription is made — and `n8ro-judge` could not make one.** It
links nothing that could reach the platform, which its build script proves by naming no include
path and no library and by failing the build if the assertion path ever names a process, a bus
or the control path.

The live campaign judges the capture it has just written, through **the same evaluator over the
same stored file**. So *"a re-judgement produces verdicts identical to the live run's"* is
structural rather than promised — and `--verify` checks it anyway, byte for byte:

```
  000  fail   satisfied 3  violated 3  indeterminate 1   (met 1, not met 5)
    verify: 7 verdict(s) byte-identical to the live run's verdicts.jsonl
```

## Limits — what a result here does and does not prove

- **The determinism gate passes on content, and that is weaker than the strictest reading of the
  brief.** Two runs of one configuration are **never byte-identical here** — 0 of 190 pairs — and
  the gate does not require them to be. What it establishes is that every sample present in both
  captures at the same simulation instant carries the same values: 9 573 667 of 9 573 667 over
  those pairs, zero differing. It establishes **nothing** about the samples present in only one of
  them, and it says so in the verdict rather than in a footnote. Whether that discharges the
  brief's acceptance criterion 2 was **OQ-2**, and it is **decided rather than answered** — see
  below.
- **This project measures one machine.** The brief claims the platform's guarantee holds *"on
  every run and every machine"*. Nothing here tests cross-machine reproducibility and no claim
  about it is made (ADR-1). The self-test run against captures from two machines is the cheapest
  possible check if a second one ever becomes available.
- **A campaign can stop at its own self-test for a reason that is not a determinism failure, and
  on this machine that happens.** Measured over 42 captures: **2** have a segment 0 that the
  format's exact frozen-clock test classifies as `frozen`, and in both every duplicated instant
  carries **byte-identical values** — part of the start-up roster burst published twice, inside a
  segment whose clock plainly did not reset. Such a segment is excluded, as the format requires,
  and a pair with nothing left to compare is **refused** rather than passed. That is about **1 in
  27 ordinary runs and so about 1 in 14 pairs**. There is deliberately **no retry**: a harness
  that re-rolls its gate until it likes the answer has no gate. The refusal names the shape it
  found — a duplicated publication or a reset clock — and the imprecision is with EXT-08 as E-4.
- **A sweep's determinism claim is about ONE of its values.** The gate runs two runs at
  `self_test_value` and compares them. That establishes determinism at that value; it is not one
  claim per run, and the campaign report, `self-test.json` and the sweep table each say so in
  words. A twenty-value sweep is one determinism claim at a named point plus nineteen runs of a
  harness whose determinism was demonstrated at that point.
- **A parameterised run can refuse the gate more often than an unparameterised one, and it is
  the same refusal.** The axis acts before `start`, and it can land between two publications of
  the start-up roster burst — making segment 0 `frozen` with repeated values that **differ**
  rather than agree. Measured over 35 parameterised runs: **4** — 11.4%, against R12's 1 in 27
  (3.7%) for ordinary runs — consistent across two independent batches. That is elevated on a
  sample that supports a direction and not a number, and it is reported as elevated rather than
  as established. The exclusion is not relaxed and there is still no retry.
  It is a third shape through a test `contract/`'s §5.1 presents as detecting one thing, and it
  strengthens E-4 rather than being worked around.
- **The swept parameter has a range the platform enforces, and outside it the value is not what
  flew.** An injected speed is honoured exactly up to **400 m/s**; above it the platform walks the
  entity down at 20 m/s² until it reaches the ceiling. At 440 m/s a 60 s run spends 2.0 s off
  parameter; at 900 m/s, **25.0 s — 42% of the run**. That ceiling belongs to the entity profile
  the committed example uses; another scenario's raiders may clamp elsewhere, and nothing here
  establishes where. **Nothing checks this at run time** — the campaign publishes the value it
  was given, and a sweep past the ceiling would plot a real result against a number that stopped
  being true partway through each run. `docs/m5-oq4.md` §3 is the measurement; staying inside the
  range is the campaign author's, not the tool's.
- **The sweep's result columns are counts, not verdicts.** `adds`, `keys` and `samples` are read
  off each run's capture by this project's reader. They are not judgements — no condition exists
  until M6 — and a trend in them is a trend in what the runs recorded, not in whether the runs
  were correct. `samples` additionally carries the platform's 0.38% publication-schedule spread
  and `adds` does not, which is why the trend is drawn from `adds`.
- **Nothing verifies that a declared entity direction matches the one the scenario authored.**
  The campaign file states a heading per entity and the value scales it; if the file says south
  and the scenario authored east, the sweep is real, deterministic, reported correctly, and about
  a different question than the author meant. What *is* checked is that every named entity
  carried a sample — a name that matched nothing is in the run record.

- **The comparison establishes agreement, never completeness.** It compares digests of values
  present in both files. Two runs that both failed to publish the same frame agree about it
  perfectly, and no comparison of two captures can detect that. This is the first bullet below,
  applied to the gate itself.

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
- **A conformant capture is not a complete one, and a complete one need not cover its run.**
  `n8ro-capture` reporting `CONFORMS` means the file is internally consistent and agrees with its
  own trailer. It says nothing about what the bus delivered — see the first bullet — and nothing
  about how much of the run it covers. A capture bounded by `--capture-max-bytes` under
  `--on-size-limit stop` is complete, valid, conformant and possibly a third of a run; measured at
  M3 at `sim_time_s` 19.5 of 60.0. `run.json`'s `capture.covers_whole_run` is the field that says
  which, and it is a precondition on anything computed from the file, not a footnote.
- **The reader is checked against one producer's output and one specification.** Thirty real
  captures, every one written by `n8ro-bridge` — twenty-nine at producer 0.9.0 and the vendored
  fixture at 0.5.0 — plus the mutants and micro-captures the conformance suite writes itself.
  Nothing here has met a capture from a second producer, and "conformant" means "agrees with
  `contract/capture-format-v1.md` as this project reads it".
- **Four defects in `contract/` have been found, raised, and fixed upstream — none worked
  around.** §6.7 said a rotated run's totals are the sum across its parts, which for `segments` is
  false (measured: 5 summed for a 2-segment run); §5.1's frozen-clock test was read as "the clock
  was reset" when three phenomena satisfy it; the condition digest stopped one heading before the
  arithmetic every geometric verdict rests on; and EXT-08's README documented the headless
  invocation without `N8RO_RELEASE`. All four were raised as E-3 to E-6, **all four were fixed by
  EXT-08 on 2026-09-01**, and `contract/` is re-pinned at `ca5118c` — the fourth pin. **Nothing
  here changed as a result**: every correction confirmed a behaviour already implemented and
  stated beside the text it disagreed with. The reader still reports both segment numbers and
  names which is which; it now agrees with the specification rather than reporting beside it.
- **The headless invocation is confirmed in all six of its parts.** The brief says *"confirm the
  invocation with your mentor"*, and on 2026-09-01 the mentor confirmed: `N8RO_RELEASE` **is** expected in production; the degraded terrain configuration
  **is** expected and should be left as it is; a console control event **is** the intended way to
  end an unattended host **and** its non-zero exit is expected; and `C:\N8RO\bin` on `PATH` **is**
  a known second precondition. **Two further parts came back on a follow-up the same day**: the
  bus-publish route **is** the intended control path, and on the choice of host variant the
  answer was *"pick the one you prefer"* — so **no `SimEngineHost_*` variant is prescribed**, and
  `SimEngineHost_SharedMemory` stays as this project's own explicitly-delegated choice rather
  than an inference, on the reasoning in `docs/m7-oq2-oq3.md` §2.2. **Every one of the six
  confirms what was already built, so nothing changed** — and the standing exposure carried since
  M1 is discharged rather than merely unrealised.

### What a verdict does and does not prove

- **A `pass` means every declared condition was satisfied — not that the run was correct.** It
  is a statement about the questions somebody thought to ask, over the data that reached the
  file. A condition nobody wrote is a condition nobody checked, and the campaign cannot tell you
  which those are.
- **A `not_met` verdict on a proximity or area condition is a *bounded* conclusion, not a
  record.** Nothing observed the entity between two samples; what the verdict claims is that
  nothing it could have done in that window would have changed the answer. The margin, the bound
  and the largest gap are all in the verdict so the claim can be checked. Where the margin does
  **not** clear the bound, the verdict is `indeterminate` — and that is the correct answer, not
  a failure of the tool.
- **The bound rests on one assumption that is inferred rather than measured directly**: that the
  platform cannot exceed its own measured 20 m/s² acceleration clamp inside one frame. That clamp
  was measured on one entity profile in one scenario. Its practical weight is small — across the
  committed sweep the term contributes 0.10 m to bounds of 1.20–22.10 m — but it is the
  assumption to attack first. **F-28.**
- **A `terminal_state` condition using `field` + `equals` can never report a sound `not_met`.**
  It reports `indeterminate` instead, every time, and that is by design rather than by
  limitation: nothing in the format bounds how fast a field may change. `health` was measured
  moving through nominal → degraded → disabled → wrecked → destroyed with zero regressions in
  seven runs, which is a direction and not a guarantee, and this classification does not lean on
  it.
- **"No CIWS gun engaged" cannot be expressed**, and it is the cleanest binary result this
  project has measured. The gun rounds are entities named `BlueGun_East_01_wpn_44749_4` — the
  numeric parts are generated and differ every run — and conditions name entities, with no
  pattern matching. Two other conditions flip across the same sweep and are expressible, so
  nothing required is lost. Note that M5's reason for refusing a glob (it would perturb the
  publication schedule the gate measures) does **not** apply on a read-back path; this is
  declined on merit and stays available to a future revision.
- **A condition asserting non-occurrence is expressible for the `area` kind and, through
  `expect`, for the other two.** What is not expressible is a condition over more than one run.
  The sweep's comparison lives in the report, not in the condition language, deliberately — the
  first step of a general expression language is the rabbit hole ADR-5 exists to close.
- **The arithmetic every geometric verdict rests on is this project's decision, not an inherited
  one.** `contract/condition-file-schema.md` documents `within_m` as a threshold "in metres" and
  stops one heading before the sections that say how a distance is computed and how a boundary
  is decided. Both exist upstream and neither was vendored, so EXT-17 decided them —
  ECEF on WGS-84, straight-line Euclidean, `<=` at the threshold, edge-inclusive polygons — and
  states the constants in `src/assert/Geodesy.h` so a verdict can be recomputed with a
  calculator. Raised as **E-5**; the decision does not wait on the answer.

### What the campaign costs when the platform does something awkward

- **The determinism gate refuses roughly 1 pair in 14, and more under parameterisation.** Part of
  the start-up roster burst is published twice with byte-identical values, in a segment whose
  clock did not reset — which satisfies the format's frozen-clock test and excludes the segment.
  Measured 2 of 42 ordinary captures; **4 of 35 parameterised runs (11.4%)**. It is not a
  determinism failure, the refusal names which shape it found, and **there is deliberately no
  retry**.
- **The same mechanism can stop a whole campaign, not just one run.** Applying the parameter
  before `start` can land between two publications of the roster burst, so one run of the
  self-test pair records the scenario's authored velocity and the other records ours. Measured
  on the first execution of the committed twenty-run campaign: **23 samples differing, every one
  at `sim_time_s` 0, every one in `velocityNed`, every one a raider the axis updates**. The gate
  correctly failed, the campaign correctly stopped at exit 3, and **zero runs were attempted**.
  That execution is kept at `campaigns/m6-gate-refused/`; it is the first time the content gate
  has failed on a real pair for a real reason. **F-29**, and it strengthens E-4.
- **The value that makes that visible is a platform artifact of ~1e-14.** An injected
  `velocityNed` comes back through the capture as `[-1.0103336092965664e-14, -55, 0]` where the
  scenario's authored value is exactly `[0, -55, 0]`. This project's arithmetic is exact —
  `direction × value`, no normalisation — so the artifact is introduced between the update and
  the capture. With a bit-identical vector the race above would produce no difference at all.
  **F-30.**
- **A host that dies mid-run is not noticed until the run timeout expires.** The campaign
  survives it, reports `infrastructure_error`, and continues — which is what the brief asks —
  but it pays `--run-timeout-ms` in wall clock for each occurrence, ten minutes at the default.
  There is deliberately no second timed quantity watching for heartbeat silence, because the run
  timeout is the only clock a run is allowed. **Size `--run-timeout-ms` against the frame
  budget**; at 1200 frames a run takes about 65 s here, so 120 000 is generous and 600 000 is
  ten minutes of waiting for nothing. **F-27.**
- **The axis has a measured fidelity ceiling at 400 m/s and the tool does not enforce it.** Above
  it the platform clamps, walking the entity down at 20 m/s²; at 900 m/s a run spends 42% of
  itself off parameter. Staying inside the range is the campaign author's job on purpose: the
  ceiling belongs to a scenario's entity profiles, not to the campaign runner, and hard-coding it
  would be this project asserting something about scenarios it has never loaded. **R13.**

## The evidence, and what it does not include

Every campaign this project has run is committed — its configuration, its run records, its
verdicts, its summary and its log. **Not its captures.** One 1200-frame run of Atacama Air
Defense writes about 24 MB of JSON Lines, so the twenty-run campaign alone is 527 MB, and
`.gitignore` has excluded captures since this repository's first commit for that reason.

Each campaign carries a `MANIFEST.md` instead, giving every capture's byte size, SHA-256 and the
counts this project's own reader made of it. **What that establishes is narrower than it looks,
and the file says so**: it does not make a re-run byte-identical — nothing does, and that is this
project's central measurement rather than a limitation of the decision — but it pins what these
specific files contained, so a number in a milestone document traces to a file rather than to
somebody's memory. The counts are the part a re-run can be compared against.

| | |
|---|---|
| `campaigns/m6-campaign/` | **The committed twenty-run campaign.** 8 pass, 10 fail, 2 infrastructure error. `report.txt` is the readable report, `changed-input-diff.txt` the run-to-run diff, `rejudge-verify.txt` the byte-identity check |
| `campaigns/m6-gate-refused/` | **Its first execution, kept.** The gate correctly refused and **zero runs were attempted** — the first time the content gate has failed on a real pair for a real reason |
| `campaigns/m6-faults/` | The four ugly realities, each injected into run 001 of a three-run campaign, each survived |
| `campaigns/m5-sweep/`, `m5-sweep-first/` | M5's sweep, both executions, both counted |
| `campaigns/m4-gate/`, `m4-bytes/`, `m4-frozen/`, `m4-overload/` | The gate, the byte basis, a frozen segment, and a deliberately overloaded recorder |
| `campaigns/m3-oq6/`, `m2-oq1/`, `m2-axis/` | Rotation probed rather than read about; the twenty runs the stop predicate was decided on; the axis feasibility spike |

**A refused or defective execution is never deleted in favour of a clean one.** `m5-sweep-first`
is where F-24 was found; `m6-gate-refused` is a real gate failure; `m6-campaign/campaign.log`
still carries F-35's defect while `report.txt` carries the same records after the fix. Keeping
both is the point — re-running until the numbers are welcome is choosing evidence.

## The deliverables the brief asks for, and their status

| | Status |
|---|---|
| A git repository with the runner | Done |
| A README with the four topics | Done — indexed at the top of this file |
| A real campaign, committed as an example | Done, **with one named deviation**: the captures are not committed and a manifest stands in for them |
| A 5-minute recording | **NOT DELIVERED.** It needs a person. Scripted beat by beat in `docs/recording-script.md`, including what *not* to say |
| A page of notes on determinism | Done — `docs/determinism-notes.md`, and its §5 is the part the brief says to write carefully |

**Every success metric is now met, including the one that was reported unmet twice.** The
sweep-legibility metric names **mentor review of the sweep report** as its measurement method,
and the mentor reviewed it on 2026-09-01 and confirmed it reads.

**It was reported UNMET at PRD revs 7 and 8** — the artifact exceeding its target was explicitly
*not* accepted as a substitute for the method the metric named. So it is met now **on the terms
it was written on rather than on relaxed ones**, and `docs/m7-evidence.md` §4 keeps both the
earlier refusal and the answer, because the refusal is the part that makes the pass mean
something. The brief's own acceptance criterion 3 names no reviewer and was already met on
measured evidence; that remains a separate statement.

## Boundaries

- **`contract/` is read-only.** It holds the `n8ro-capture/1` specification and fixtures vendored
  from EXT-08. A defect in it goes back to EXT-08 rather than being worked around here.
- **No EXT-08 source, ever** — not a header, not a snippet, not a class name relied upon. The
  host and the recorder are driven as processes.
- **`C:\N8RO` is read-only.** Nothing here writes into the install tree. The host writes its own
  log there; the campaign copies out of it and never into it.
- **The SDK is linked in exactly one place**, `src/control/`, and **the capture reader and the
  determinism comparison link nothing at all** — checked on every build of each, twice for the
  reader and four times for the comparison: the SDK's and the producer's names, any global sort of
  a capture, and, for the comparison, a clock read or formatted time and an unordered container or
  locale-dependent number conversion. `n8ro-campaign` links both, which is the allowed direction;
  the requirement is that they link no SDK, not that nothing linking the SDK may link them. **The
  code that decides the gate could not reach the platform if it wanted to.**

## Layout

| Path | What |
|---|---|
| `src/proc/` | Child process supervision. Started, and ended, by handle |
| `src/control/` | The control path. The one place the N8RO SDK is linked |
| `src/run/` | The stop predicate, the run record, the run itself, and the self-test |
| `src/capture/` | The conformant reader for `n8ro-capture/1`. Links nothing |
| `src/compare/` | The determinism comparison — alignment, the coverage floor, the twelve refusals, both comparisons, the report. Links nothing |
| `src/param/` | The one parameterisation axis: its model and its campaign-file parser. Links nothing, not even the run path — an axis is a declaration |
| `src/assert/` | The conditions, the geodesy, and the evaluator that turns a stored capture into verdicts. Links nothing, and **could not reach a host or a bus if it wanted to** |
| `src/common/` | Logging, a JSON writer with no run-to-run variation in it, and an order-preserving JSON parser |
| `tests/` | Tests that link nothing and need no install. `tests\build.cmd` builds and runs them |
| `tools/n8ro-campaign/` | The execution CLI, and its golden `--help` |
| `tools/n8ro-capture/` | The reader CLI, and its golden `--help`. Its build script is the boundary's proof |
| `tools/n8ro-compare/` | The comparison CLI, and its golden `--help`. Its build script proves the boundary **and** that none of CR-DET-2's four hazards is on the path |
| `tools/n8ro-judge/` | The re-judge CLI, and its golden `--help`. Its build script proves the boundary, the hazards, **and** that the assertion path names no process, bus or control path |
| `examples/` | The committed campaign configurations and the committed condition file — the twenty-run campaign `docs/m6-assertions.md` reports, and M5's seven-value sweep |
| `tools/spike-axis/`, `tools/spike-oq4/` | M2's and M5's feasibility spikes. Evidence, not product |
| `tools/m2-checks/`, `m5-checks/`, `m6-checks/` | Analysis scripts behind the milestone documents' numbers. Evidence, not product |
| `campaigns/` | Every campaign this project has run, minus its captures. See "The evidence" |
| `docs/` | The PRD, one document per milestone, the findings index, the escalations, the determinism notes, the recording script, and the decisions taken without asking |
| `tools/spike-oq4/` | M5's OQ-4 **fidelity** spike — the criterion M2's deliberately did not measure. Evidence, not product |
| `tools/m5-checks/` | The throwaway reader for that spike's captures |
| `tools/m2-checks/` | Throwaway analysis scripts, superseded by `n8ro-capture` at M3. Kept only because `oq1_table.py` is the published reproduction command for `docs/m2-oq1.md`'s table |
| `tools/m1-run/` | M1's by-hand driver, kept as the evidence behind `docs/m1-lifecycle.md` |
| `contract/` | Vendored from EXT-08. Read-only |
| `docs/` | The PRD, the milestone records, the escalations, and `findings.md` — **one index over every issue this project has found**, and the place to start |
