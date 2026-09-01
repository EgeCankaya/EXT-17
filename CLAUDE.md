# CLAUDE.md — EXT-17 Headless Campaign Runner

> **Current to M7, the FIFTH `contract/` pin, the cross-repo clean-room pair test, and the
> mentor's second relay (2026-09-01).**
> `docs/prd.md` (rev 11) is
> the binding contract; this file carries what is expensive to get wrong and cheap to forget.
> Every section below states something that was measured rather than something that was decided
> in a meeting, and where a number appears it is this project's own.

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
- `contract/capture-*.n8rocap.jsonl` — **two** real captures, producer 0.5.0 and 0.9.0. Both are
  fixtures: the first is the one that predates `sample_form`, `limits` and `part` and proves an
  absent key reads as *unknown*; the second carries all three. Neither replaces the other (F-49)
- `contract/condition-file-schema.md` — the referee's condition shape, **and since the fourth pin
  the arithmetic and boundary rules too**. Until 2026-09-01 it was a verbatim *excerpt* of
  EXT-08's README that stopped one heading early (F-19, F-32), so how a distance is computed and
  how a boundary is decided did not cross. Raised as **E-5**; EXT-08 fixed it by creating a real
  upstream file, which this now vendors **by identity**. The arithmetic in `src/assert/Geodesy.h`
  is unchanged and still this project's own decision — it now agrees with a vendored sentence
  rather than with an inference
- `contract/PROVENANCE.md` — **read this first.** What crossed, what did not, and the seven
  measured findings from EXT-08 that bind what EXT-17 should build

`contract/` is read-only from here. If something in it is wrong or insufficient, that is a
defect in EXT-08's contract and it goes back there rather than being worked around.

**That rule has now completed a full circuit, and the fourth pin is what it looks like.** All
four `contract/` defects this project raised — E-3, E-4, E-5 and E-6 — were fixed upstream on
2026-09-01 and vendored back at EXT-08 `ca5118c`. **Nothing here changed as a result**: every
correction confirmed a behaviour already implemented and stated beside the text it disagreed
with, which is what raising them rather than working around them buys. **And it added a rule:
a `fixed` escalation makes `contract/` stale, and nothing notices** — so re-pin in the same
breath, and re-run the suite. That is F-38, and it is a written rule and not a check.

**And F-38 turned out to be narrower than the problem, which is what the FIFTH pin is
(`bda3904`, F-47).** Two more things made `contract/` stale and **neither is an escalation this
project raised**:

1. **An escalation this project ANSWERS does it too.** E-7, E-8 and E-9 were raised by EXT-08
   *against* EXT-17; EXT-17 settled all three; EXT-08 wrote the answers into the frozen
   specification. So this project had ruled on three questions about the format it vendors and
   was reading a copy that predated its own rulings — and `docs/escalations.md` ran E-1 to E-6
   and stopped, because it only ever recorded outbound questions. It records inbound ones now.
2. **The upstream fixing its own defect.** EXT-08 regenerated its sample capture because its own
   audit found it shipped a producer-0.5.0 file against a 0.9.0 build. No escalation, no issue,
   no version bump — nothing to notice it by except **running the pin check**, which
   `contract/PROVENANCE.md` writes out in full and which had not been run since the fourth pin.

**The fifth pin is also the first that changed something here**, and that is worth remembering
because the previous four’s whole story was that they did not. E-7 widened §14’s
host-dependent field list from one to three; `src/compare/` masks one, and that gap is **F-50**,
open by decision with its bound stated. The other two confirmed behaviour already built.

**Both were found by cloning both repositories cold and walking both READMEs, not by reading
anything** — `docs/clean-room.md`. **A pass that reads one repository cannot see the seam
between two.**

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

**OQ-2 is `decided`, is `concurred` with, and was never `answered`. Those three words must never
be swapped.** Decided by the DRI on 2026-09-01 from [B]'s own words — `docs/m7-oq2-oq3.md` §1 —
and **concurred with by the mentor the same day, independently, without having seen the
reading**. That is a second opinion and **not a ruling**: OQ-2 asks whether [B]'s acceptance
criterion 2 is discharged, [B]'s acceptance criteria belong to [B]'s author, and **[B]'s author
has still never replied**. A ruling from them would still be acted on. `self-test.json` keeps
`oq2_decided_by`, `oq2_concurred_by` and `oq2_answered_by_brief_author` (`false`) as three
separate keys so the last cannot be computed from the others.

The deciding passage is **not** criterion 2's *"identical captures"*; it is [B]'s statement of
what the self-test is **for** — *"if it ever fails, you have found either a defect in your harness
or something far more interesting, and you must be able to tell which."* A byte gate fails 190 of
190 here, identically on a clean harness and a broken one, so it distinguishes neither case. The
content gate passed 190 of 190 clean **and failed on a real pair for a real reason** at M6
(`campaigns/m6-gate-refused/`).

**No code changed when it was decided** — content was already the default — and M4's shape still
holds: under `--gate-basis bytes` the gate correctly fails and the campaign stops with exit 3 and
zero runs attempted (`campaigns/m4-bytes/`), so a ruling still changes a default and no code.

**What may now be written, and what still may not.** Criterion 2 **is** claimed as met *under the
content reading*, and every claim of it carries "under the content reading" in the same breath. It
is **never** claimed under a byte reading, and it is **never** called answered, agreed, or ruled
by [B]'s author. **The mentor's concurrence may be stated and must never be stated alone** — four
checks in `tests/determinism_test.cpp` now hold this: two assert the report carries both halves of
the decided/not-answered pair, and two more assert the concurrence appears only in a report that
also says [B]'s author has not replied. The failure being guarded is not the wording; it is
somebody later reading "the mentor said content" and deleting the caveat.

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
   §5.1 went back to EXT-08 as E-4 and **was fixed there on 2026-09-01** — §5.1 now lists all
   three shapes and §14 says an emptied self-test is a refusal rather than a pass. The exclusion,
   the floor and the no-retry rule are unchanged; they are upstream text now as well as local
   discipline.

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

## Conditions and verdicts, since M6 — and the one key that had to be added

**OQ-5 is decided: adopt the vendored declaration shape, supersede three rules around it, add
exactly one key.** Decided by writing M6's conditions in the shape and evaluating them against
the seven committed captures — `docs/m6-oq5.md` — rather than by reading the schema. A file
written for EXT-08's referee parses here unmodified, and `tests/assertion_test.cpp` keeps that
true against `contract/example.conditions.json` itself.

`src/assert/` is the fourth component that **links nothing**, and `tools/n8ro-judge/build.cmd`
proves it with two searches the others do not have: one that fails the build if the assertion
path ever names a process, a bus or the control path, and one for a global sort. That is
CR-CAP-1's *"no host started and no bus subscription made"* turned from a promise into a
property. **Both searches once fired on this project's own prose** — the tokens are now call- and
include-shaped, and the comments that tripped them say why they name no forbidden spelling.

**Six things that are already right here and are easy to break:**

1. **`expect` is the only key added, and it exists because [B]'s third question needs it.** The
   vendored schema is a *referee*: it reports whether a condition was satisfied and says nothing
   about whether that is welcome. Two of [B]'s three questions survive being read as "this should
   hold"; *"did anything reach a terminal state it **should not** have"* does not, and the
   vendored shape expresses non-occurrence only for the `area` kind. **The verdict still records
   the fact in the vendored schema's own terms** (`met` / `not_met`) and the expectation is a
   separate field (`satisfied` / `violated` / `undetermined`), so EXT-08's verdicts and a
   re-judgement here stay comparable. **Do not fold them into one state.**
2. **A condition file inverts `contract/`'s unknown-key rule, for the second time and the same
   reason** — a person writes it. Unknown key and duplicate key are refused by name; `_` prefixes
   a comment. **And `scenario_unload` is refused as a `removal_reason`**: measured, 267 of 385
   removals across the sweep are the teardown, all at `sim_time_s` 0.
3. **CR-AS-4 is classified per FORM, not per kind**, which is finer than the requirement asks.
   `terminal_state` + `removal_reason` is soundly decidable in the negative by **format §8.1's
   normative invariant** — a sample carrying `(E, k)` is positive evidence of non-removal, and a
   gap does not weaken it because a re-creation carries a *higher* occupancy. That is what makes
   [B]'s own dangerous example answerable rather than permanently indeterminate.
   `field` + `equals` is **never** decidable in the negative. Do not collapse the four rows.
4. **A not-met geometric verdict is a bounded conclusion, and the bound is on the verdict.**
   `(v_a + v_b)·Δt + ½·20·Δt²`. Every such verdict carries the margin, the bound and the gap, so
   the claim is checkable. The 20 m/s² is F-21's, measured on one entity profile — that is R16,
   the weakest link, and it is stated rather than buried.
5. **There is one evaluator and one input: a stored capture.** The live campaign judges the
   capture it just wrote, through the same code `n8ro-judge` re-judges with. CR-CAP-1's identity
   is therefore structural — **do not add a "live" evaluation path**. `verdictJson` lives in
   `src/assert/Judge.cpp` and only there, and **the capture path is deliberately not a member of
   a verdict**, because a live judgement and a re-judgement are handed different paths and
   `--verify` compares byte for byte.
6. **An injected or broken run is NOT judged.** `infrastructure_error` and `timeout` carry 0
   verdicts against N declared conditions, with `judged_this_run: false` and a reason. Inventing
   verdicts for a run the harness broke would make an infrastructure failure into a test result,
   and the count mismatch is CR-AS-2's cut-short signal working as intended.

**And the cost, measured and not hidden.** R14's mechanism has a fourth shape with a
campaign-level blast radius: the pre-`start` update can race the roster burst so that the two
self-test runs differ at `sim_time_s` 0, the gate correctly fails, and **zero runs are attempted**
(exit 3). Met on the first execution of the twenty-run campaign — 23 samples, all `velocityNed`,
all raiders the axis updates — and that execution is **kept** at `campaigns/m6-gate-refused/`
rather than discarded in favour of the clean one. **The gate was not weakened and no retry was
added**, and this was the first real opportunity to do either. What makes it visible is a ~1e-14
platform artifact in a round-tripped `velocityNed` where the authored value is exactly 0 (F-30);
our arithmetic is exact.

**One defect worth remembering because of how it hid.** Until M6 every difference in an
**array-valued** field printed two *empty* values — so `positionGeodetic`, `velocityNed` and
`orientationYprRad` named the field and showed nothing. It shipped at M4 and survived three
milestones, because reaching that code needs a real content-gate failure on a real pair and M4's
failing-gate evidence came from forcing `--gate-basis bytes`, which reports a byte offset and
never gets there. **A code path only a real failure reaches needs a test that manufactures the
failure** (R17, F-31).

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
said a run's totals are the sum across its parts, which is false for `segments`. That is E-3 in
`docs/escalations.md`, back to EXT-08, not worked around — and **fixed there on 2026-09-01**:
§6.7 rule 2 and §11 now name the four counters that sum and say to subtract one per cut. The
reader is unchanged; it computed both numbers and said which was which, and now agrees with the
specification instead of reporting beside it.

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
  sitting idle rather than failing. **Confirmed by the mentor on 2026-09-01: it IS expected to be
  set in production** (E-1 part (b)). It went to EXT-08 as **E-6**
  ([issue #4](https://github.com/EgeCankaya/EXT-08/issues/4)) and **was fixed there the same
  day** — EXT-08's README R8 block now carries both preconditions and both failure modes. The
  issue as first filed cited `contract/PROVENANCE.md` finding 6, which is **ours, not EXT-08's**
  (F-37); the citation was corrected by a comment, and finding 6 itself was corrected here at the
  fourth pin. See `docs/m1-lifecycle.md` §7(a).
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
  `TerrainElevationServiceClient` / `GeoidGridModel` errors. **Do not fix this** — and since
  2026-09-01 that is the **mentor's answer**, not this project's judgement call: the degraded
  configuration is expected on this install and should be left as it is (E-1 part (d)). Every
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

**`drafted` is not `sent`, and `sent` is not `fixed`.** A finding written up and not delivered is
*recorded*, not *raised*. E-3, E-4, E-5 and E-6 went to EXT-08 as GitHub issues and **all four
came back fixed on 2026-09-01** — the strongest state in the table, because a reply is a sentence
and a fix is a diff. **E-1 was drafted and unsent from M1 to M7** and its delivery was blocked on
nobody but this project — and when it finally went, **all six parts came back answered inside a
day and not one of them changed anything.** That is the argument for asking early, and it is the
cost of the delay stated rather than absorbed.

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

`src/assert/` is the same rule a fourth time: the conditions, the geodesy the vendored digest did
not carry (E-5), and the evaluator that turns a stored capture into verdicts. Its build script
adds two searches the others do not have — one that fails the build if the assertion path ever
names a process, a bus or the control path, and one for a global sort — because CR-CAP-1's *"no
host started and no bus subscription made"* is worth making structural rather than promising.
`n8ro-judge` is the re-judge front end and `--verify` byte-compares against the live run's
verdicts.

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
