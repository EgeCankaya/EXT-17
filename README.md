# EXT-17 — Headless Campaign Runner

Runs many unattended N8RO simulation runs, varies one input across them, judges each against
conditions declared outside the code, and reports across the campaign.

**Status: milestone 4 of 7.** What exists today is the execution half, the reading half and the
gate: one run, automated, with an explicit end and a bounded timeout, repeatable unattended; a
conformant reader for the capture format that **links nothing at all**; and a determinism
self-test that runs at the start of every campaign and stops it if it does not pass. There is no
parameterisation sweep (M5) and no assertions or campaign report (M6). `n8ro-campaign` therefore
reports three outcomes rather than four; see [The four outcomes](#the-four-outcomes).

> **The determinism gate passes on content, and that is this project's decision rather than the
> client's.** The brief asks for two identical runs to *"produce identical captures"*; measured
> here across 190 pairs, they agree on every sample present in both at the same simulation
> instant and are **never** byte-identical. Both comparisons are run and reported on every
> self-test, and which one *decides* is an open question with the brief's author (OQ-2,
> unanswered). See [Proving determinism](#proving-determinism--the-self-test-and-the-gate).

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

**Which of the two decides the gate is OQ-2, out with the owner of the brief and unanswered.**
`--gate-basis content|bytes` selects it; `content` is the default and is ADR-1's decision. Under
`bytes` the gate correctly fails on this platform and the campaign correctly stops — that is the
honest implementation of the brief's strictest reading rather than an argument about it, and it
has been run. **A ruling either way therefore changes a default and no code.**

Every report says so, on every run:

```
  gate basis            content   (ADR-1: the content basis is THIS PROJECT'S decision, not the client's)
  OQ-2                  UNANSWERED. ...
  GATE                  PASS   on the content basis
                        This does NOT discharge [B]'s acceptance criterion 2 as written.
```

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

Requires Visual Studio's x64 toolchain and the N8RO SDK at `C:\N8RO`.

```
tools\n8ro-campaign\build.cmd      ->  build\n8ro-campaign\n8ro-campaign.exe
tools\n8ro-capture\build.cmd       ->  build\n8ro-capture\n8ro-capture.exe
tools\n8ro-compare\build.cmd       ->  build\n8ro-compare\n8ro-compare.exe
tests\build.cmd                    ->  builds and runs the tests
```

`tests\build.cmd` links nothing and needs no N8RO install — it covers the parts whose
correctness is about our own output rather than about the platform; since M3 the capture reader,
whose correctness is about somebody else's bytes; and since M4 the determinism comparison, whose
correctness is what every other result rests on. **78 conformance checks and 75 determinism
checks.**

**`tools\n8ro-capture\build.cmd` and `tools\n8ro-compare\build.cmd` need no N8RO install either,
and that is the point.** Look at their compile lines: no `/I`, no `/LIBPATH`, no `.lib`. Neither
the reader nor the comparison links EXT-08 or the SDK, and a build script anyone can read in ten
seconds is a better proof of that than an argument about translation units. Each build then
searches its own sources for the SDK's and the producer's names, and for any global sort of a
capture, and fails on a hit. **`n8ro-compare`'s carries two searches more** — for a clock read or
a formatted time, and for an unordered container or a locale-dependent number conversion —
because those are CR-DET-2's hazards and they are properties of the comparison's own sources
rather than of whatever links them.

Each build ends by running its binary's own `--help` and comparing it against the golden file
beside it — `tools\n8ro-campaign\help.golden.txt`, `tools\n8ro-capture\help.golden.txt`,
`tools\n8ro-compare\help.golden.txt`. A drift fails the build. This is deliberate: the PRD does not enumerate the option list in prose, because
a list nobody executes is exactly what drifted in the sibling project. **The golden file is the
CLI's specification.**

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

## What a run produces

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

The PRD requires four, never collapsed: **pass, fail, timeout, infrastructure error.** At M4
there are three, because nothing judges a run yet:

| Outcome | Means |
|---|---|
| `completed` | The stop predicate was satisfied and teardown was clean. **Not a pass** — no condition has been evaluated. `pass` and `fail` replace it at M6 |
| `timeout` | The run timeout expired before the predicate was satisfied. Its own outcome, never a failure |
| `infrastructure_error` | The harness, the host, or the scenario load failed. Never a failing scenario |

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

## Limits — what a result here does and does not prove

Partial at M4; CR-DOC-1 requires the full version at M7.

- **The determinism gate passes on content, and that is weaker than the strictest reading of the
  brief.** Two runs of one configuration are **never byte-identical here** — 0 of 190 pairs — and
  the gate does not require them to be. What it establishes is that every sample present in both
  captures at the same simulation instant carries the same values: 9 573 667 of 9 573 667 over
  those pairs, zero differing. It establishes **nothing** about the samples present in only one of
  them, and it says so in the verdict rather than in a footnote. Whether that discharges the
  brief's acceptance criterion 2 is **OQ-2, and it is unanswered**.
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
- **One imprecision in that specification has been found and not worked around.** §6.7 says a
  rotated run's totals are the sum across its parts; for `segments` that is false, because a
  segment cut by a rotation is closed in one part and opened in the next. Measured: 5 summed for a
  2-segment run. It is raised with EXT-08 as E-3, and the reader reports both numbers and names
  the gap rather than picking one silently.
- **The headless invocation is not yet confirmed.** It is measured working; whether it is the
  *intended* production shape is OQ-3, open, in `docs/escalations.md`.

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
| `src/common/` | Logging, a JSON writer with no run-to-run variation in it, and an order-preserving JSON parser |
| `tests/` | Tests that link nothing and need no install. `tests\build.cmd` builds and runs them |
| `tools/n8ro-campaign/` | The execution CLI, and its golden `--help` |
| `tools/n8ro-capture/` | The reader CLI, and its golden `--help`. Its build script is the boundary's proof |
| `tools/n8ro-compare/` | The comparison CLI, and its golden `--help`. Its build script proves the boundary **and** that none of CR-DET-2's four hazards is on the path |
| `tools/spike-axis/` | M2's R9/OQ-4 feasibility spike. Evidence, not product |
| `tools/m2-checks/` | Throwaway analysis scripts, superseded by `n8ro-capture` at M3. Kept only because `oq1_table.py` is the published reproduction command for `docs/m2-oq1.md`'s table |
| `tools/m1-run/` | M1's by-hand driver, kept as the evidence behind `docs/m1-lifecycle.md` |
| `contract/` | Vendored from EXT-08. Read-only |
| `docs/` | The PRD, the milestone records, the escalations, and `findings.md` — **one index over every issue this project has found**, and the place to start |
