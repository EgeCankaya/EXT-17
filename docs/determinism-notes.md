# Notes on determinism

**The deliverable [B] asks for**, in its own words: *"A page of notes on determinism — what you
had to do to make comparison meaningful, and anything you saw that you could not explain. The
last part is the one to write carefully."*

It is longer than a page because the second part earned the space. §5 is that part, and it is
written to be attacked.

Every number here was measured by this project's own binaries on this machine, at N8RO runtime
2.1.328, against captures written by `n8ro-bridge` 0.9.0. Nothing is inherited except where it
says so.

---

## 1. The claim, stated exactly

> **Two runs of one configuration agree on every sample present in both captures at the same
> simulation instant. They are never byte-identical.**

Both halves are measured. Over all 190 pairs of M2's twenty runs: **9 573 667 samples compared,
9 573 667 agreeing, zero differing, 190 of 190 passing the content comparison, 0 of 190
byte-identical.**

**What the claim does not say** matters as much as what it does. It says nothing about samples
present in only one capture — and there are always some. It is a claim about one machine
(ADR-1). And whether it discharges [B]'s acceptance criterion 2, which asks for *"identical
captures"*, is **OQ-2 — out with the owner of the brief and unanswered**. The tool implements
both readings as selectable gates so a ruling changes a default and no code, and under
`--gate-basis bytes` the gate correctly fails and the campaign correctly stops, demonstrated on
real runs rather than argued.

---

## 2. What had to be done to make the comparison meaningful

Six things, in the order they would otherwise have gone wrong.

### 2.1 Both runs must stop at the same place, and "the same place" cannot be a clock

Two runs stopped after the same *duration* have not covered the same simulation, so their
captures differ for a reason that has nothing to do with determinism. The stop predicate is a
**frame budget**, read off `sim/engine/state`, and no wall-clock quantity participates.

Measured over twenty runs: **one distinct observed frame (1200), one distinct `simTime` (60.0),
one distinct `dt` (0.05000)**. The same claim is false of anything counted in the capture — those
twenty runs produced **seventeen distinct sample counts**, spanning 50 361 to 50 552. A predicate
keyed on a sample count would have made every pair incomparable, and it would have looked
reasonable.

### 2.2 A sample present in one run and not the other is not a difference

It is the *expected* case: the host publishes a slightly different subset of frames every run,
about 0.2% of them, differently each time. Counting those as differences would fail every pair.

They are counted, reported inside the verdict, and never allowed to fail the gate. The verdict
says what was actually established rather than implying more:

> every sample present in **both** runs at the same simulation instant agrees. This is not a
> claim that the two captures are identical.

### 2.3 …and the intersection still needs a floor

"They all agreed" over three samples is a wrong number, and a wrong number is worse than no
number. The comparison carries a **coverage floor**: the intersection must reach 99% of the
smaller run's comparable samples, or the verdict is `indeterminate` rather than `pass`.

The floor is measured rather than chosen — the worst of those 190 pairs was **99.8513%**, so 99%
sits about 2.4× clear of anything this machine has produced. It guards against a collapsed
intersection, not against the platform.

### 2.4 The comparison never converts a number

Two runs are aligned on the **verbatim text** of `sim_time_s`, and values are digested from their
original characters. Nothing on the comparison path formats or parses a double.

This is not fastidiousness. On this machine under `German_Germany.1252` the C library writes
`0,05` for a decimal, so a comparison that round-tripped a number through the current locale
would compare two differently-mangled strings and report a difference that is entirely ours. The
format writes doubles in shortest round-trip form (§8.3), so equal text and equal double are the
same relation, and deciding on the text means the verdict never depends on a conversion at all.

### 2.5 Only running segments are compared, and `indeterminate` is not `running`

A segment's clock is **three-valued**: `running` (the format's exact test fired and passed),
`frozen` (it fired and failed), `indeterminate` (**it could not fire** — no key repeated a
`sim_time_s`).

A boolean would have to call the third case "running", which asserts the result of a test that
never ran. It is not hypothetical: M2 measured segment 1 opened and closed **empty in 15 of 20
runs**, and this project met the other shape too — 42 samples, one per entity, all at
`sim_time_s` 0.0. Comparing those teardown segments would have reported 26 "present in one run
only" between two runs, every one an artifact of comparing two teardowns that were never the same
event.

### 2.6 Nothing of ours may vary between runs, and that is checked twice over

[B] names three hazards: *"a timestamp in the compared output, an unordered container iterated, a
value read from a clock."* This project adds a fourth, the locale.

Each is checked **twice**, and neither check subsumes the other:

- **A build-time search** over the comparison's own sources, which fails the build on a hit. It
  catches a reintroduction on a path no test happens to exercise.
- **A behavioural test.** The same comparison run twice produces byte-identical report text; the
  same captures with entities and segments inserted in the opposite order produce byte-identical
  report text; the whole suite is re-run under a comma-decimal locale and every report must match
  the C-locale one. It catches a hazard spelled in a way the search does not know.

Each search was verified to reject by injecting its own hazard. **Both halves were needed at
M6**: the locale search caught nothing real, and the locale *test* caught F-26 — a suite written
to prove locale-independence whose own capture builder was locale-dependent.

---

## 3. What the gate costs, and why there is no retry

**The gate refuses roughly 1 pair in 14, for a reason that is not a determinism failure.**

Part of the start-up roster burst is published twice with byte-identical values, inside a segment
whose clock never reset. That satisfies the format's frozen-clock test exactly, so the segment is
excluded — and with segment 0 excluded there is nothing left to compare. Measured **2 of 42
captures**; **4 of 35** parameterised runs (11.4%).

**There is deliberately no retry.** A harness that re-rolls its gate until it likes the answer has
no gate. The cost is contained three other ways: the refusal *names which shape it found* — a
duplicated publication or a reset clock, distinguished by whether the repeated records' values
agree; the frequency is in the README so an operator expects it; and the imprecision went to
EXT-08 as **E-4**, because §5.1 presents one test as detecting one phenomenon and it detects
three.

**A mitigation exists and was deliberately not taken.** Applying the parameter after `start` at
frame ≥ 1 avoids the roster burst entirely — measured at M2. It was declined because the axis
would then be *"state at frame 1"*, and [B]'s axis is *"initial positions and velocities"*.

---

## 4. Changing one input, and where the runs diverge

[B] asks for both halves of the diff, and they are different questions. The same machinery
answers both; what changes is what the answer means.

**Same configuration twice** → they should agree, and a divergence is a determinism finding.
That is the gate.

**One input changed** → they should diverge, and the question is *where*. `n8ro-compare
--changed-input` reports the first divergence by segment, `(entity, occupancy)`, `sim_time_s` and
field — never by line number — and prints no gate line and no pass/fail word, because neither
would be true. **Agreement there is the outcome worth more attention**, not less: it means the
changed input did not take effect, which is the shape this project keeps finding.

Measured, two runs 0.1 m/s apart — physically negligible, configurationally total: **33 546
differing samples**. The divergence is real, immediate and attributable.

**Nothing in a campaign can confuse the two.** `n8ro-campaign` can only ever produce a self-test
pair, because both gate runs are *copies of one `RunConfig`* rather than two configurations
arranged to match. The changed-input framing has to be asked for.

---

## 5. Things seen that could not be explained

**This is the section [B] says to write carefully.** Each item is something measured and not
accounted for. Where a mechanism is guessed at, it says so.

### 5.1 About 0.2% of frames go unpublished, differently every run, with every counter at zero

The single most consequential observation in this project, and it is inherited, reproduced and
still unattributed.

Twenty identical runs produced **seventeen distinct sample counts**, up to five whole frames
missing, a 0.38% spread — while all nine drop counters and every bus metric read **zero**. The
loss is frame-shaped rather than sample-shaped: whole frames go missing, not scattered entities.
It is **not** driven by rate — three times the message rate produced a complete capture upstream
— and it appears even in an artifact written inside the host process with no bus in its path, so
no consumer configuration avoids it.

**What cannot be explained is the silence.** Something between the engine and the recorder does
not publish a frame, and nothing anywhere reports that it happened. Every counter designed to
notice reads zero.

**Everything downstream is built around it rather than in spite of it.** It is why a sample in one
run and not the other is not a difference (§2.2); why the intersection has a floor (§2.3); and
why an assertion never reads absence as evidence — a condition answered *"no record says it
happened"* is answered from a file that may be missing the frame in which it did.

### 5.2 Part of the roster burst is published twice, byte-identically

Measured in 2 of 42 ordinary captures: 13 instants in one run carried a second, **byte-identical**
sample for the same `(entity, occupancy)`, inside a segment with 1 200 distinct `sim_time_s`
spanning 0 to 60. The clock did not reset.

Whatever republishes sits **upstream of the recorder**: the duplicate records are byte-identical
and both are in the file, so the producer recorded faithfully what it was handed. Why the burst
is sometimes published twice is not known here.

Its consequence is §3's — the segment satisfies the frozen-clock test and is excluded.

### 5.3 An injected velocity comes back as `-1.0103336092965664e-14` where the authored one is exactly `0`

Found at M6, and it is the sharpest of these because both sides are known exactly.

The axis computes `velocityNed = direction × value`. For a declared direction of `[0, -1, 0]` at
55 m/s that is exactly `[0, -55, 0]` — no normalisation, no square root, no conversion. The
capture reports:

```
run 000  RedUAV_E_01  t=0  velocityNed [-1.0103336092965664e-14, -55, 0]
run 001  RedUAV_E_01  t=0  velocityNed [0, -55, 0]
```

`positionGeodetic` is identical in both. Run 001's value is the scenario's authored one, untouched
by us; run 000's is ours, after a round trip through `sendEntityUpdate` and back out through the
capture.

**What cannot be explained is where the 1e-14 enters.** The magnitude is consistent with a
rotation through a frame and back — a north component computed as `cos(θ)·0 − sin(θ)·(−55)` for a
θ that should be zero — but that is a guess about code this project does not have and has
deliberately never read. What is certain is that it is not ours: our value is exact, and the
authored value is exact, and only the round-tripped one is not.

**It matters more than 1e-14 suggests**, because it is what makes §5.4 visible.

### 5.4 The pre-`start` update races the roster burst, and the loser is the whole campaign

A parameterised run's update lands after `loaded` and before `start`. The burst is published at
load. Normally the burst is finished first and both runs' `t = 0` samples carry the authored
value. When the burst is republished **after** the update in one run of the self-test pair and
not the other, the two captures disagree at `t = 0`.

Measured on the first execution of the committed twenty-run campaign: **23 samples differing,
every one at `sim_time_s` 0, every one in `velocityNed`, every one a raider the axis updates**.
The gate correctly failed. The campaign correctly stopped at exit 3 with **zero runs attempted**.

That execution is kept, at `campaigns/m6-gate-refused/`.

**Two things about it are worth stating plainly.**

It is the **first time the content gate has failed on a real pair for a real reason**. Every prior
demonstration of a failing gate came from forcing `--gate-basis bytes`. That difference had a
consequence nobody predicted: it exposed **F-31**, a defect that had shipped at M4 and survived
three milestones, in which every difference in an *array-valued* field printed two empty values.
`positionGeodetic`, `velocityNed` and `orientationYprRad` are all arrays. The diff had been
naming the field correctly and showing nothing, on exactly the fields that matter, and no test
could see it because every synthetic capture in the suite carried only scalars.

And **what is unexplained is narrower than it looks**. The race itself is understood. What is not
understood is §5.2's mechanism — *why* the burst is sometimes published twice — which is the same
open question in a third costume. §5.2, R14 and this are one phenomenon observed three ways, and
that is why it strengthened **E-4** rather than opening a fourth escalation.

### 5.5 A non-zero host exit code is the normal case here

`CTRL_BREAK_EVENT` shuts the host down in an orderly way — the `.running` marker is removed and no
crash rename follows — and Windows still reports `0xC000013A`. So a non-zero host exit is not
evidence of a crash on this platform, and treating it as one would have made every clean teardown
look like a failure.

This one is explained; it is here because it looks exactly like a determinism problem the first
time it is seen, and because the run record now states it rather than leaving each reader to
rediscover it.

---

## 6. What would change these answers

- **A ruling on OQ-2.** It changes a default and no code. Until it arrives, every report this
  project produces says the content basis is this project's decision and not the client's, and no
  document anywhere claims [B]'s acceptance criterion 2 as discharged.
- **A second machine.** ADR-1 measures one. [B] claims the platform's guarantee holds *"on every
  run and every machine"*; nothing here tests the second half and no document claims it.
- **A producer that positively bounds completeness.** If a future counter could say *"this capture
  is complete"* rather than leaving §5.1 unattributed, several conditions that are currently
  `indeterminate` become decidable, and ADR-6 says so explicitly.
- **An answer to §5.2.** If the double publication is a known host behaviour, naming it beside the
  frame-skipping measurement in the format's §14 would save the next consumer the same
  investigation. That is the last paragraph of E-4, offered rather than asked.
