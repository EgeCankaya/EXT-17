# CLAUDE.md — EXT-17 Headless Campaign Runner

> **Provisional.** Written at repo creation, before the PRD exists, to carry the two things
> that would be expensive to get wrong first. Rewrite it properly once `docs/prd.md` lands.

## What this repo is

The downstream half of a two-project pair. EXT-08 records a simulation run into a durable,
versioned capture file and judges it; **EXT-17 runs the campaign** — executing many runs,
varying parameters, asserting over the results, and reporting across them.

The client brief is `docs/EXT-17-Headless-Campaign-Runner.docx`. It is the binding contract.
It is untracked (proprietary), so read it off disk.

## The rule that matters most

**EXT-08 and EXT-17 are separate repositories with no shared source.** Not a header, not a
snippet, not a class name relied upon.

Everything EXT-17 gets from EXT-08 crosses that boundary as a **documented, versioned
artifact**, and all of it is vendored in `contract/`:

- `contract/capture-format-v1.md` — the capture format, **frozen** at EXT-08's M7
- `contract/capture-*.n8rocap.jsonl` — a real capture, as a test fixture
- `contract/condition-file-schema.md` — the referee's condition shape, reference only
- `contract/PROVENANCE.md` — **read this first.** What crossed, what did not, and the seven
  measured findings from EXT-08 that bind what EXT-17 should build

`contract/` is read-only from here. If something in it is wrong or insufficient, that is a
defect in EXT-08's contract and it goes back there rather than being worked around.

The practical proof this is possible: EXT-08 wrote a conformance reader for its own format
from `capture-format-v1.md` alone, linking neither its bridge nor the N8RO SDK. If it could be
done there it can be done here.

## Pin the format version, and reject anything else

A reader that meets a `format_version` it does not implement **rejects the file with a named
error and stops**. It does not attempt a partial parse. That is the entire compatibility
mechanism and it is deliberately blunt: partial parsing of an unknown format is how
silently-wrong analysis happens.

Currently `n8ro-capture/1`.

A key you do not recognise inside a record type you *do* recognise is **not** a version change
and must be ignored, not rejected (format §13). That is the whole reason the version has held
across three producer releases, and it has already mattered twice: producer 0.8.0 added
`header.sample_form`, and 0.9.0 added `header.limits`, `header.part`, `header.continues_from`
and `trailer.continued_in`. Reject on the **version**; ignore unknown **keys**.

## The determinism gate, since M4 — and the one question it does not close

**Both comparisons always run and are always reported. Which one *decides* is `--gate-basis`,
default `content`, and that default is this project's decision (ADR-1), not the client's.**

`OQ-2 is still unanswered` — re-checked against EXT-08's own escalation record at M4. So M4 was
built so that a ruling either way changes a default and no code: under `--gate-basis bytes` the
gate correctly fails on this platform and the campaign correctly stops with exit 3 and zero runs
attempted, which has been demonstrated (`campaigns/m4-bytes/`). **Never write, anywhere, that
[B]'s acceptance criterion 2 has been discharged.** A content pass discharges it *under the
content reading*, and every report this project produces says so.

Four things about the comparison that are already right and are easy to break:

1. **A sample present in one run and absent from the other is not a difference.** It is the
   expected case — the host publishes a different subset of frames every run. It is counted,
   reported inside the verdict, and never allowed to fail the gate.
2. **…and the intersection still has a floor.** 99% of the smaller run's comparable samples, or
   the verdict is `indeterminate` rather than `pass`. Measured: the worst of 190 pairs was
   99.8513%. "They all agreed" over three samples is a wrong number (tenet 1).
3. **The comparison never converts a number.** Runs align on the **verbatim text** of
   `sim_time_s`; values are digested from their original text. That puts CR-DET-2's locale hazard
   off the path rather than testing it back off. Do not introduce a formatted double.
4. **`indeterminate` is excluded and so is `frozen`, and having nothing left is a refusal, not a
   pass.** Measured: 2 of 42 captures have a **frozen segment 0** — part of the start-up roster
   burst published twice with byte-identical values, in a segment whose clock did not reset. So a
   campaign stops at its own gate roughly 1 pair in 14, for a reason that is not a determinism
   failure. **Do not add a retry.** The refusal names which shape it found; the imprecision in
   §5.1 went back to EXT-08 as E-4.

## The one parameterisation axis, since M5 — and the cost it carries

**OQ-4 is decided: [B]'s first axis, *initial positions and velocities*, as one declared scalar
applied to named entities before `start`.** Decided by exercising a range rather than by arguing
from M2's feasibility result — `docs/m5-oq4.md`, 18 probe runs. It beat "which scenario from the
catalogue" on **ordering** (the catalogue enumerates fine — 10 scenarios, 119 ms, asynchronous —
but a name has no order, and CR-PAR-2 asks for a *trend*), and "which entities are present" on
CR-AS-4.

It acts through `RunConfig::afterLoadBeforeStart`, the seam M2 built. **There is no second
mechanism and there must not be**: a parameter applied anywhere else is a parameter the run
record does not know about.

Five things that are already right here and are easy to break:

1. **The declared TEXT of a value is authoritative and the double is derived.** `27.5` reaches
   `run.json` and the report as `27.5`. The double publishes the value and orders the sweep, and
   is never printed. This is M4's rule (the comparison never converts a number for a decision)
   applied to the parameter — **do not introduce a formatted double on the report path.**
2. **Entities are named, never matched. There is no glob.** Resolving one would mean subscribing
   the control path to `sim/entity/state`, which perturbs the publication schedule the
   determinism gate measures. Instead, the capture read-back checks each named entity carried a
   sample, and `run.json` lists any that did not — a publish returning true says the message
   reached the bus and nothing about whether anything received it.
3. **A campaign file inverts `contract/`'s unknown-key rule, deliberately.** The capture format
   says ignore an unrecognised key (§13) and the reader does exactly that. A campaign file is
   ours and a person wrote it, so an unknown key is **refused** — `"value"` for `"values"` would
   otherwise be a sweep that silently did not happen. A key beginning with `_` is a comment.
4. **The gate runs at ONE declared value and establishes determinism for that value.** Both
   self-test runs are copies of one `RunConfig`, which is what makes them a valid pair rather
   than an arrangement (measured: 4 runs at one value, all 6 pairings, 293 576 samples, zero
   differing). **Two runs at different values are never compared** — measured negative control:
   two runs 0.1 m/s apart correctly fail the gate with 33 546 differing samples.
5. **The sweep's result column is a count read off a capture, not a verdict.** No condition
   exists until M6. `adds` is the column to read a trend from because M2 measured the roster
   lifecycle agreeing exactly across twenty identical runs; `samples` carries the platform's
   0.38% publication spread and `adds` does not.

**And the cost, which is not hidden anywhere and must not be traded away.** The axis acts before
`start`, so it can land between two publications of the start-up roster burst — making segment 0
`frozen` with repeated values that **differ**. That is a *third* shape through §5.1's one test
(a reset clock, a duplicated publication with identical values, and this), and the only one this
project causes. Measured **4 of 35** parameterised runs (11.4%) against R12's 1 of 27 ordinary
ones (3.7%), consistent across two independent batches: elevated on a sample that supports a
direction and not a number, and recorded that way. **The exclusion is not
relaxed, there is still no retry, and this strengthens E-4 rather than opening a new
escalation.** A mitigation exists and was deliberately not taken — applying the parameter after
`start` at frame ≥ 1 avoids the burst entirely (M2's `p2b`), but then the axis is "state at
frame 1" and [B]'s axis is *"initial positions and velocities"*.

**The axis also has a measured range, and the tool does not enforce it.** An injected speed is
honoured exactly up to **400 m/s** and clamped above it at 20 m/s²; at 900 m/s a run spends 42%
of itself off parameter. Staying inside the range is the campaign author's job, on purpose: the
ceiling belongs to a scenario's entity profiles, not to the campaign runner, and hard-coding 400
would be this project asserting something about scenarios it has never loaded (R13).

## Three things that will bite before anything else

Full detail and numbers in `contract/PROVENANCE.md`. In short:

1. **A byte-for-byte determinism gate cannot pass on this platform.** Measured: two headless
   runs stopped at the same frame agree on 50 358 of 50 358 samples and are still not
   byte-identical, because ~0.2% of frames go unpublished, differently each run. Key any
   determinism self-test on **content**, not bytes.
2. **A single ordinary run contains two segments**, because the engine's stop path unloads and
   reloads. Any statistic computed over a whole capture without segmenting it is wrong.
3. **An entity's identity is `(name, occupancy)`, never name.** The engine re-creates entities
   under the same name, mid-run and at teardown.

And one that was a choice rather than a trap, now **decided at M3 (OQ-6): `stop`.** The recorder
can bound and rotate its captures (`--capture-max-bytes`, `--on-size-limit`, producer 0.9.0), and
M3 exercised both on real runs rather than choosing from the documentation. `rotate` works
completely and is not lossy — a four-part capture stitched back to the same roster lifecycle as an
unrotated one. It was not chosen because **one run's two segments become five `(part, segment)`
keys**, four of them fragments of one segment that nothing in any file identifies as such. Every
run under `stop` is one file; `--capture-max-bytes` defaults to `61 000 × --frames`, three times
the measured per-frame cost, so the bound exists and does not fire on a normal run. The campaign
ceiling is `8 GiB` over the whole campaign directory. See `docs/m3-oq6.md`.

**The reader supports rotation anyway**, because a capture rotated elsewhere still has to be
readable here — and reading a rotated set found one imprecision in the frozen specification: §6.7
says a run's totals are the sum across its parts, which is false for `segments`. That is E-3 in
`docs/escalations.md`, back to EXT-08, not worked around.

## Verified environment — do not re-derive

- Install: `C:\N8RO`, runtime 2.1.328, SDK component `com.n8ro.dev` 2.1.328.
- `C:\N8RO` is **read-only** for this project. Read headers, read docs, run binaries. Never
  write into it.
- Headless host: `n8ro-sim-app.exe --sim-config SimEngineHost_SharedMemory --model-path <dir>
  --schema-file <name>`. **It takes no scenario argument** — load and start are published on
  `sim/scenario/command` and `sim/engine/command`. See `contract/PROVENANCE.md` finding 6.
- **`N8RO_RELEASE=C:\N8RO` must be set for the headless host too**, not only for
  `n8ro-sim-local`. Measured at M1: with it unset the host resolves its plugin directory from the
  current working directory, skips the plugin scan, never registers `componentPhysics` (from the
  stock `bin\plugins\sim\n8ro-physics.dll`), and **refuses every 42-entity scenario load** — while
  sitting idle rather than failing. `contract/PROVENANCE.md` finding 6 omits it; that is the
  contract's to correct, not ours. See `docs/m1-lifecycle.md` §7(a).
- **`C:\N8RO\bin` must be on `PATH`** for any binary of ours that links the SDK — it is where
  `n8ro-sim.dll` and `n8ro-core.dll` resolve from, and there is nowhere else. Measured at M2: a
  client launched from a scratch directory without it exits 53 having produced no output at all,
  which reads like a crash and is a missing DLL. This is a **second, separate** precondition from
  `N8RO_RELEASE`; setting one does not cover the other. `n8ro-campaign` sets both for its
  children and needs `PATH` itself.
- **The recorder's `--queue-size` is not `header.subscription.queue_size`.** The first bounds the
  recorder's handler-to-writer queue and is what makes `trailer.drops.samples_not_recorded`
  non-zero; the second is the bus subscription's and read `1024` in every run measured, at the
  default and under `--queue-size 4` alike. Measured at M4 while making CR-DET-1's exclusion rule
  fire for the first time (`campaigns/m4-overload/`, 2 755 samples not recorded). Consequence: a
  like-for-like check on `header.subscription` cannot see a queue-size difference between two
  captures. It does not need to — the drop counter does, and it is checked first.
- **A relative path handed to a child process means something else there.** Every child is started
  in its own working directory, so a relative `--out-dir` passed to the recorder is resolved
  against the run directory it was just placed in. Measured at M3: the recorder refused, correctly
  and with a clear message, into a file nobody was reading — and the run then executed to its stop
  predicate having recorded nothing while reporting `completed`. `n8ro-campaign` resolves
  `--out-dir` to an absolute path before anything is handed to a child, and a run asked to record
  that produces no capture is now an `infrastructure_error`.
- **Run any N8RO binary from a scratch directory.** `n8ro-sim-local.exe` writes a per-entity
  JSONL dump into its working directory, and `n8ro-sim-app.exe` creates `data/db/` and `logs/`
  there — it did so in this repo's root during M1 before the rule was known.
- **The host keeps its real log in the install tree, and appends to it across runs.**
  `C:\N8RO\logs\n8ro-logger-n8ro-sim-app.log` is one fixed filename that every host process
  **appends** to, so after twenty runs it holds all twenty (measured at M2: two identical runs
  produced a file of exactly twice one run's size, carrying two startup banners). Two
  consequences: redirected stderr is not a reliable record of a run — the same run wrote 5 777
  navigation errors to the log and, under PowerShell's redirection, three lines to `host.err` —
  and a campaign that wants one log per run must record the file's size before starting the host
  and copy only the bytes after it. `n8ro-campaign` does exactly that (`host_logger_offset` in
  each run record). A `.running` marker sits beside it, and an unclean exit makes the *next* host
  start rename the previous log as `.crash-<timestamp>.log`.
- **There is no shutdown command; ending the host is a process signal.** `sim/engine/command`'s
  vocabulary is closed at `start` / `stop` / `pause` / `step`
  (`docs/modules/n8ro-sim/dev/engine-lifecycle-and-control.md`). Measured at M2: a
  `CTRL_BREAK_EVENT` to the host's own process group shuts it down in an orderly way — the
  `.running` marker is removed and no crash rename follows — but Windows still reports exit
  `0xC000013A` (`STATUS_CONTROL_C_EXIT`), so **a non-zero host exit code is the normal case here
  and is not evidence of a crash**. `TerminateProcess` leaves the marker behind and does cause
  the crash rename, so prefer the control event and keep termination-by-handle as the fallback.
- The install ships **no geoid grid and no elevation service**, so every run floods with
  `TerrainElevationServiceClient` / `GeoidGridModel` errors. **Do not fix this.** Every
  measurement inherited from EXT-08 was taken in this configuration; provisioning terrain would
  invalidate the comparability of all of it. Setting `N8RO_RELEASE` does not change it — there is
  no grid under `C:\N8RO\data\geoid` to find.

## Where a finding goes

**`docs/findings.md` is the index over every issue this project has found** — defects in our own
code, platform behaviour we work around, imprecisions in `contract/`, and defects in the brief.
It is an index, not a second home: each row points at the milestone document, escalation or
README section that actually holds the finding, and if the two disagree the target wins.

A milestone that finds something adds a row there **and** records it where it belongs. The
reverse check is the useful one at a milestone close: every finding in that milestone's document
should have a row, and every escalation still marked `drafted` should be re-examined for whether
it can be delivered yet.

**`drafted` is not `sent`.** A finding written up and not delivered is *recorded*, not *raised*.
E-3 and E-4 went to EXT-08 as GitHub issues; **E-1 has been drafted and unsent since M1** and its
delivery is blocked on nobody but this project.

## Conventions

- Files in this repo are ours — **no Arkheon proprietary header**. That convention applies only
  to files created inside `C:\N8RO`, which this project does not do.

## The reader and the comparison, since M3 and M4

`src/capture/` and `tools/n8ro-capture/` implement `n8ro-capture/1` from
`contract/capture-format-v1.md` alone. **They link nothing** — not EXT-08, not the SDK, not a
third-party JSON library — and `tools/n8ro-capture/build.cmd` is where that is visible in one
file, plus two searches over its own sources that fail the build on a hit: one for the SDK's and
the producer's names, one for any global sort of a capture (the file's own record order is
authoritative, format §5.2).

`src/compare/` and `tools/n8ro-compare/` are the same rule again for the determinism comparison,
with two searches more in `build.cmd`: one for a clock read or a formatted time, one for an
unordered container or a locale-dependent number conversion. Those are CR-DET-2's hazards, they
are properties of the comparison's own sources, and each is *also* tested behaviourally in
`tests/determinism_test.cpp` — the search catches a reintroduction on a path no test exercises,
the test catches one the search does not know the spelling of. Neither replaces the other.

`src/param/` is the axis: its model and its campaign-file parser, linking nothing at all — not
even the run path, because an axis is a declaration and turning one into bus traffic is
`n8ro-campaign`'s job. That is what puts the **whole** of CR-PAR-1's configuration surface into
`tests/parameter_test.cpp`, which needs no install; the only part needing a simulator is whether
the platform honours a swept value, and that is measured in `docs/m5-oq4.md` rather than
asserted.

`n8ro-campaign` links all three, which is the allowed direction. The requirement is that they link no
SDK, not that nothing linking the SDK may link them.

Three things about the model that are easy to get wrong and are already right here: the segment
list is built from `segment_open` and not from records carrying samples; a segment's clock is
three-valued (`running` / `frozen` / **`indeterminate`**, the last because the format's exact test
cannot fire on an empty or one-frame segment); and a segment's time extent is
`[first sample, last sample]`, never its boundary records, which both read `0.0` on a reloaded
scenario.
